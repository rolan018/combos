# write parameters.xml file, gather output of simulator and save FLOPS into file for each hosts' power distribution
from pathlib import Path
from python.configer.project import Project
from python.configer.group import Group
from python.configer.group_project import GroupProject
from python.configer.conf_file import ConfigurationFile
from python.configer.shell import run_in_schell
import numpy as np

experiment_dir = Path(__file__).parent

proj_name_1 = "RakeSearchtype1e15@home"
proj_name_2 = "RakeSearchtype2e13@home"

REPEATING_PER_CONFIG = 1
# FLOPS = np.arange(100, 1000, 50, dtype=int)
FLOPS = [1, 10, 1000, 100000]
PROP = [3000000000, 7000000000, 10000000000, 
        20000000000, 40000000000, 60000000000, 
        80000000000, 100000000000, 120000000000,
        140000000000, 160000000000, 180000000000,
        200000000000, 250000000000, 500000000000]

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
    file_name = f"result_{flop}.csv"
    print("----------START----------")
    print(flop)
    if flop == 1:
        min_speed = 0.5
    else:
        min_speed = flop - 1
    with open(experiment_dir / file_name, "w") as fout:
        print(f"proj_name,param,{','.join(key_to_look_at)}", file=fout)

        for l in PROP:
            print(f"{l} task_fpops")
            # form configuration file
            config = ConfigurationFile(simulation_time = 96)
            config.add_project(Project(name=proj_name_1, snumber= 0, task_fpops=l))
            config.add_project(Project(name=proj_name_2, snumber=1, task_fpops=l))
            cluster = Group(n_clients = 10, min_speed=min_speed, max_speed=flop)
            cluster.set_project(GroupProject(pnumber=0))
            cluster.set_project(GroupProject(pnumber=1))
            config.add_cluster(cluster)

            cluster1 = Group(n_clients = 100, min_speed=min_speed, max_speed=flop)
            cluster1.set_project(GroupProject(pnumber=0))
            cluster1.set_project(GroupProject(pnumber=1))
            config.add_cluster(cluster1)

            # form parameters file and all linked
            with open("parameters.xml", "w") as wfile:
                print(config.Serialize(), file=wfile)
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