# write parameters.xml file, gather output of simulator and save FLOPS into file for each hosts' power distribution
from pathlib import Path
from configer.project import Project
from configer.group import Group
from configer.group_project import GroupProject
from configer.conf_file import ConfigurationFile
from configer.shell import run_in_schell
import numpy as np

experiment_dir = Path(__file__).parent.parent.parent

proj_name_1 = "RakeSearchtype1e15@home"
proj_name_2 = "RakeSearchtype2e13@home"

LOG_DIR = "exp/FLOPS/min_quorum_3_success_95"

REPEATING_PER_CONFIG = 1

FLOPS = np.arange(10600, 10300, 300, dtype=int)

FLOPS_1 = np.arange(11000, 21000, 1000, dtype=int)
FLOPS_2 = np.arange(30000, 100000, 10000, dtype=int)
FLOPS = np.concatenate([FLOPS_1, FLOPS_2])

PROP = np.arange(50_000_000_000, 1_500_000_000_000, 50_000_000_000, dtype=int)

MIN_QUORUM = 3
SUCCESS_PERCENTAGE = 95
CANONICAL_PERCENTAGE = 85

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

print(f"LOG_DIR:{LOG_DIR}")
for j, flop in enumerate(FLOPS):
    file_name = f"{LOG_DIR}/result_{flop}.csv"
    print("----------START----------")
    print("FLOPS:", flop)
    prop_list_execution = []
    with open(experiment_dir / file_name, "w") as fout:
        print(f"proj_name,param,{','.join(key_to_look_at)}", file=fout)
        for l in PROP:
            print(f"{l} task_fpops")
            # form configuration file
            config = ConfigurationFile(simulation_time = 96)
            config.add_project(Project(name=proj_name_1, snumber=0, task_fpops=l, min_quorum=MIN_QUORUM))
            config.add_project(Project(name=proj_name_2, snumber=1, task_fpops=l, min_quorum=MIN_QUORUM))
            cluster = Group(max_speed=flop, min_speed=flop-4*flop//10)
            cluster.set_project(GroupProject(pnumber=0, success_percentage=SUCCESS_PERCENTAGE, canonical_percentage=CANONICAL_PERCENTAGE))
            cluster.set_project(GroupProject(pnumber=1, success_percentage=SUCCESS_PERCENTAGE, canonical_percentage=CANONICAL_PERCENTAGE))
            config.add_cluster(cluster)

            cluster1 = Group(n_clients = 100, max_speed=flop, min_speed=flop-4*flop//10)
            cluster1.set_project(GroupProject(pnumber=0, success_percentage=SUCCESS_PERCENTAGE, canonical_percentage=CANONICAL_PERCENTAGE))
            cluster1.set_project(GroupProject(pnumber=1, success_percentage=SUCCESS_PERCENTAGE, canonical_percentage=CANONICAL_PERCENTAGE))
            config.add_cluster(cluster1)

            # form parameters file and all linked
            with open(experiment_dir / "parameters.xml", "w") as wfile:
                file_name = f"{LOG_DIR}/execute_{flop}_{l}.csv"
                execute_path = experiment_dir / file_name
                print(config.Serialize(execute_path), file=wfile)
            run_in_schell(f"cd {experiment_dir};./generator")


            for i in range(REPEATING_PER_CONFIG):
                get_macro_stat = run_in_schell(f"cd {experiment_dir};./execute")

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
                        row_b = 0
                        for row, s in enumerate(get_stat):
                            row_b = row
                            if s.isspace():
                                break
                            row_b += 1
                        extract_result.append(get_stat[:row_b].replace(",", ""))
                    if len(extract_result) == len(key_to_look_at):
                        prop_list_execution.append(l)
                        print(f"{proj_name},{l},{','.join(extract_result)}", file=fout)
                        extract_result = []
print("----------FINISH----------")
