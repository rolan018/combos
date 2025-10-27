class Project:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def __getattr__(self, name):
        return self.__dict__.get(name, None)

    def Serialize(self):
        return f"""
    <sproject>
        <snumber>{self.snumber or 0}</snumber>                <!-- Project number -->
        <name>{self.name or "project1"}</name>                <!-- Project name -->
        <nscheduling_servers>{self.nscheduling_servers or 1}</nscheduling_servers>    <!-- Number of scheduling servers -->
        <ndata_servers>{self.ndata_servers or 1}</ndata_servers>        <!-- Number of data servers of the project -->
        <ndata_client_servers>{self.ndata_client_servers or 1}</ndata_client_servers>  <!-- Number of data client servers -->
        <server_pw>{self.server_pw or "12000000000f"}</server_pw>        <!-- Server power in FLOPS -->
        <disk_bw>{self.disk_bw or 167772160}</disk_bw>            <!-- Disk speed in bytes/  -->
        <ifgl_percentage>{self.ifgl_percentage or 100}</ifgl_percentage>           <!-- Percentage of input files generated locally -->
        <ifcd_percentage>{self.ifcd_percentage or 100}</ifcd_percentage>           <!-- Percentage of times clients must download new input files (they can't use old input files) -->
        <averagewpif>{self.averagewpif or 1}</averagewpif>            <!-- Average number of workunits that share the same input files -->
        <input_file_size>{self.input_file_size or 1024}</input_file_size>    <!-- Input file size in bytes -->
        <task_fpops>{self.task_fpops or 10000000000}</task_fpops>        <!-- Task duration in flops -->
        <output_file_size>{self.output_file_size or 11000}</output_file_size>    <!-- Answer size in bytes -->
        <min_quorum>{self.min_quorum or 1}</min_quorum>            <!-- Minimum quorum for a result to be considered valid -->
        <target_nresults>{self.target_nresults or 1}</target_nresults>
        <max_error_results>{self.max_error_results or 8}</max_error_results> <!-- Maximum number of error results -->
        <max_total_results>{self.max_total_results or 10}</max_total_results>  <!-- Maximum number of results -->
        <max_success_results>{self.max_success_results or 8}</max_success_results> <!-- Maximum number of successful results -->
        <delay_bound>{self.delay_bound or 64800}</delay_bound>            <!-- Maximum delay for a result -->
        <output_file_storage>{self.output_file_storage or 0}</output_file_storage>    <!-- Output file storage in bytes -->
        <dsreplication>{self.dsreplication or 1}</dsreplication>        <!-- Data server replication factor -->
        <dcreplication>{self.dcreplication or 1}</dcreplication>        <!-- Data client replication factor -->
    <sproject/>
"""