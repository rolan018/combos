# write parameters.xml file, gather output of simulator and save FLOPS into file for each hosts' power distribution
from pathlib import Path
from python.configer.project import Project
from python.configer.group import Group
from python.configer.group_project import GroupProject
from python.configer.conf_file import ConfigurationFile
from python.configer.shell import run_in_schell
import numpy as np

experiment_dir = Path(__file__).parent.parent.parent

proj_name_1 = "RakeSearchtype1e15@home"
proj_name_2 = "RakeSearchtype2e13@home"

LOG_DIR = "exp/FLOPS_20_1500"

REPEATING_PER_CONFIG = 1
FLOPS = np.arange(800, 1500, 50, dtype=int)
PROP = np.arange(1000000000, 1000000000000, 5000000000, dtype=int)

key_to_look_at = ['Messages received',
                  'Work requests received',
                  'Results created',
                  'Results sent',
                  'Results cancelled',
                  'Results received',
                  'Results analyzed',
                  'Results success',
                  'Results failed',
                  'Results too late',
                  'Results valid',
                  'Workunits total',
                  'Workunits completed',
                  'Workunits not completed',
                  'Workunits valid',
                  'Workunits error',
                  'Throughput',]


for flop in FLOPS:
    file_name = f"{LOG_DIR}/result_{flop}.csv"
    print("----------START----------")
    print(flop)
    with open(experiment_dir / file_name, "w") as fout:
        print(f"proj_name,param,{','.join(key_to_look_at)}", file=fout)

        for l in PROP:
            print(f"{l} task_fpops")
            # form configuration file
            config = ConfigurationFile(simulation_time = 96)
            config.add_project(Project(name=proj_name_1, snumber=0, task_fpops=l))
            config.add_project(Project(name=proj_name_2, snumber=1, task_fpops=l))
            cluster = Group(max_speed=flop)
            cluster.set_project(GroupProject(pnumber=0))
            cluster.set_project(GroupProject(pnumber=1))
            config.add_cluster(cluster)

            cluster1 = Group(n_clients = 100, max_speed=flop)
            cluster1.set_project(GroupProject(pnumber=0))
            cluster1.set_project(GroupProject(pnumber=1))
            config.add_cluster(cluster1)

            # form parameters file and all linked
            with open("parameters.xml", "w") as wfile:
                file_name = f"{LOG_DIR}/execute_{flop}_{l}.csv"
                execute_path = experiment_dir / file_name
                print(config.Serialize(execute_path), file=wfile)
            run_in_schell("./generator")


            # run REPEATING_PER_CONFIG times and save in file.
            # in ./generator they shuffle file with host power, so it
            # must be included here, no above when using traces_file
            for i in range(REPEATING_PER_CONFIG):
                get_macro_stat = run_in_schell("./execute")
                
                proj_name = None
                extract_result = []

                for line in get_macro_stat.split('\n'):
                    if proj_name_1 in line:
                        proj_name = proj_name_1
                    elif proj_name_2 in line:
                        proj_name = proj_name_2
                    for key in key_to_look_at:
                        if key not in line:
                            continue
                        #     Results valid:                910 (80.7%)
                        get_stat = line.strip().split(key)[1].replace(': \t\t', '').replace(":", "").replace(" 	", "").replace("\t", "")
                        row_b = -1
                        for row, s in enumerate(get_stat):
                            if s.isspace():
                                row_b = row
                                break
                        extract_result.append(get_stat[:row_b])
                        #     Results valid:                910 (80.7%)
                    if len(extract_result) == len(key_to_look_at):
                        print(f"{proj_name},{l},{','.join(extract_result)}", file=fout)
                        extract_result = []
    print("----------FINISH----------")