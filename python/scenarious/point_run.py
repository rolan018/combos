# write parameters.xml file, gather output of simulator and save FLOPS into file for each hosts' power distribution
from configer.project import Project
from configer.group import Group
from configer.group_project import GroupProject
from configer.conf_file import ConfigurationFile
from configer.shell import run_in_schell

REPEATING_PER_CONFIG = 1

MIN_QUORUM = 1
SUCCESS_PERCENTAGE = 98
CANONICAL_PERCENTAGE = 84

KEY_TO_LOOK_AT = ['Messages received',
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

def point_run(experiment_dir,
              log_dir,
              flops,
              prop,
              proj_name_1 = "RakeSearchtype1e15@home",
              proj_name_2 = "RakeSearchtype2e13@home",
              min_quorum = MIN_QUORUM,
              success_percentage = SUCCESS_PERCENTAGE,
              canonical_percentage = CANONICAL_PERCENTAGE,
              key_to_look_at = KEY_TO_LOOK_AT):
    for j, flop in enumerate(flops):
        file_name = f"{log_dir}/result_{flop}.csv"
        print("----------START----------")
        print("FLOPS:", flop)
        prop_list_execution = []
        with open(experiment_dir / file_name, "w") as fout:
            print(f"proj_name,param,{','.join(key_to_look_at)}", file=fout)
            for l in prop:
                print(f"{l} task_fpops")
                # form configuration file
                config = ConfigurationFile(simulation_time = 96)
                config.add_project(Project(name=proj_name_1, snumber=0, task_fpops=l, min_quorum=min_quorum))
                config.add_project(Project(name=proj_name_2, snumber=1, task_fpops=l, min_quorum=min_quorum))
                cluster = Group(max_speed=flop, min_speed=flop-4*flop//10)
                cluster.set_project(GroupProject(pnumber=0, success_percentage=success_percentage, canonical_percentage=canonical_percentage))
                cluster.set_project(GroupProject(pnumber=1, success_percentage=success_percentage, canonical_percentage=canonical_percentage))
                config.add_cluster(cluster)

                cluster1 = Group(n_clients = 100, max_speed=flop, min_speed=flop-4*flop//10)
                cluster1.set_project(GroupProject(pnumber=0, success_percentage=success_percentage, canonical_percentage=canonical_percentage))
                cluster1.set_project(GroupProject(pnumber=1, success_percentage=success_percentage, canonical_percentage=canonical_percentage))
                config.add_cluster(cluster1)

                # form parameters file and all linked
                with open(experiment_dir / "parameters.xml", "w") as wfile:
                    file_name = f"{log_dir}/execute_{flop}_{l}.csv"
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
