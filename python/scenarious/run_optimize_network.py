from pathlib import Path
from python.configer.project import Project
from python.configer.group import Group
from python.configer.group_project import GroupProject
from python.configer.conf_file import ConfigurationFile
from python.configer.shell import run_in_schell
from python.utils.utils import zip_with_fixed
import numpy as np

experiment_dir = Path(__file__).parent

proj_name_1 = "RakeSearchtype1e15@home"
proj_name_2 = "RakeSearchtype2e13@home"


OPTIMIZED_FLOPS = 26315789473
REPEATING_PER_CONFIG = 1
MAX_SPEED = 500
# Mbps Link bandwidth (between group and data servers)
LDBW = np.arange(1, 20, 2, dtype=int)
# Gbps Link bandwidth (between group and scheduling servers)
LSBW = np.arange(1, 8, 0.5, dtype=float)


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

file_name = f"results/resultNetwork/result.csv"

with open(experiment_dir / file_name, "w") as fout:
    print(f"proj_name,lsbw,ldbw,{','.join(key_to_look_at)}", file=fout)
    for (lsbw,ldbw) in zip_with_fixed(LSBW, LDBW):
        print(f"LSBW:{lsbw}, LDBW:{ldbw}")
        # form configuration file
        config = ConfigurationFile(simulation_time = 96)
        config.add_project(Project(name=proj_name_1, snumber= 0, task_fpops=OPTIMIZED_FLOPS))
        config.add_project(Project(name=proj_name_2, snumber=1, task_fpops=OPTIMIZED_FLOPS))
        cluster = Group(max_speed=MAX_SPEED)
        cluster.set_project(GroupProject(pnumber=0, lsbw=lsbw, ldbw=ldbw))
        cluster.set_project(GroupProject(pnumber=1, lsbw=lsbw, ldbw=ldbw))
        config.add_cluster(cluster)

        cluster1 = Group(n_clients = 100, max_speed=MAX_SPEED)
        cluster1.set_project(GroupProject(pnumber=0, lsbw=lsbw, ldbw=ldbw))
        cluster1.set_project(GroupProject(pnumber=1, lsbw=lsbw, ldbw=ldbw))
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
                    get_stat = line.strip().split(key)[1].replace(': \t\t', '').replace(":", "").replace(" 	", "").replace("\t", "")
                    row_b = -1
                    for row, s in enumerate(get_stat):
                        if s.isspace():
                            row_b = row
                            break
                    extract_result.append(get_stat[:row_b])
                if len(extract_result) == len(key_to_look_at):
                    print(f"{proj_name},{lsbw},{ldbw},{','.join(extract_result)}", file=fout)
                    extract_result = []
