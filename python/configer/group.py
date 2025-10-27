from .group_project import GroupProject

class Group:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)
        self.gprojects = []

    def __getattr__(self, name):
        return self.__dict__.get(name, None)
    
    def set_project(self, project: GroupProject):
        self.gprojects.append(project)

    def Serialize(self):
        result = f"""
    <group>
        <n_clients>{self.n_clients or 10}</n_clients>            <!-- Number of clients of the group -->
        <ndata_clients>{self.ndata_clients or 1}</ndata_clients>        <!-- Number of data clients of the group -->
        <connection_interval>{self.connection_interval or 60}</connection_interval>    <!-- Connection interval -->    
        <scheduling_interval>{self.scheduling_interval or 3600}</scheduling_interval>    <!-- Scheduling interval -->
        <gbw>{self.gbw or "50Mbps"}</gbw>                <!-- Cluster link bandwidth in bps -->
        <glatency>{self.glatency or "7.3ms"}</glatency>            <!-- Cluster link latency -->
           
        <traces_file>{self.traces_file or "/Traces/boom_host_cpu"}</traces_file>    <!-- Host power traces file-->

        <max_speed>{self.max_speed or 117.71}</max_speed>            <!-- Maximum host speed in GFlops -->
        <min_speed>{self.min_speed or 0.07}</min_speed>            <!-- Minumum host speed in GFlops -->
        <pv_distri>5</pv_distri>            <!-- Speed fit distribution [ran_weibull, ran_gamma, ran_lognormal, normal, hyperx, exponential] -->
        <pa_param>0.0534</pa_param>            <!-- A -->
        <pb_param>-1</pb_param>                <!-- B -->

        <db_traces_file>/Traces/storage/boom_disk_capacity</db_traces_file>    <!-- Host power traces file -->
        <db_distri>2</db_distri>            <!-- Disk speed fit distribution [ran_weibull, ran_gamma, ran_lognormal, normal, hyperx, exponential] -->
        <da_param>21.086878916910564</da_param>            <!-- A -->
        <db_param>159.1597190181666</db_param>                <!-- B -->
        
        <av_distri>1</av_distri>            <!-- Availability fit distribution [ran_weibull, ran_gamma, ran_lognormal, normal, hyperx, exponential] -->
        <aa_param>0.357</aa_param>            <!-- A -->
        <ab_param>43.652</ab_param>            <!-- B -->

        <nv_distri>8</nv_distri>            <!-- Non-availability fit distribution [ran_weibull, ran_gamma, ran_lognormal, normal, hyperx, exponential, 8=3-phase hyperx] -->
        <na_param>0.338,0.390,0.272</na_param>            <!-- A, devided by dots if there are many -->
        <nb_param>0.029,30.121,1.069</nb_param>            <!-- B, devided by dots if there are many -->
        """
        result += f"<att_projs>{len(self.gprojects)}</att_projs>            <!-- Number of projects attached -->"

        for project in self.gprojects:
            result += project.Serialize()
        return result + f"""
    <group/>"""