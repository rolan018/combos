from pathlib import Path
from .project import Project
from .group import Group

experiment_dir = Path(__file__).parent


class ConfigurationFile:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)
        self.projects = []
        self.clusters = []
    
    def add_project(self, project: Project):
        self.projects.append(project)
    def add_cluster(self, cluster: Group):
        self.clusters.append(cluster)


    def __getattr__(self, name):
        return self.__dict__.get(name, None)
    def Serialize(self, excute_path=None):
        log_path = "<execute_state_log_path> " + str(experiment_dir.resolve()) + "/execute_stat_{2}.csv </execute_state_log_path>"
        result = f"""
<simulation_time>{self.simulation_time or 4}</simulation_time>                <!-- Simulation time in hours  -->
<warm_up_time>{self.warm_up_time or 0}</warm_up_time>                <!-- Warm up time in hours -->
<!-- Server side -->
<server_side>
    <n_projects>{len(self.projects)}</n_projects>                <!-- Number of projects -->
"""
        for project in self.projects:
            result += project.Serialize()
        result += f"<server_side/>\n\n<client_side>\n    <n_groups>{len(self.clusters)}</n_groups>                    <!-- Number of groups -->\n"

        for cluster in self.clusters:
            result += cluster.Serialize()
        
        result += f"""
<client_side/>

<experiment_run>
    <seed_for_deterministic_run> {self.seed_for_deterministic_run or 6523446} </seed_for_deterministic_run>
    <measures>
        <clean_before_write> true </clean_before_write>
        <save_filepath> {str(experiment_dir.resolve())}/save_file.txt </save_filepath>
    <measures/>
"""
        if excute_path:
            result += f"""
    <timeline>
        <execute_state_log_path> {excute_path} </execute_state_log_path>
        <observable_clients> c10,c11,c12,c13,c14,c15,c16,c20,c21,c22,c23,c24,c25,c26 </observable_clients> <!-- format is "c[cluster_ind][client_ind],c[cluster_ind][client_ind]". Cluster's index starts with 1, when client's - with 0 -->
    <timeline/>
"""
        result += f"""
<experiment_run/>
"""
        return result