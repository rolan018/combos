class GroupProject:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)


    def __getattr__(self, name):
        return self.__dict__.get(name, None)
    
    def Serialize(self):
        return f"""
        <gproject>
            <pnumber>{self.pnumber or 0}</pnumber>            <!-- Project number -->
            <priority>{self.priority or 10}</priority>            <!-- Project priority -->
            <lsbw>{self.lsbw or 10}Gbps</lsbw>            <!-- Link bandwidth (between group and scheduling servers) -->
            <lslatency>{self.lslatency or 50}us</lslatency>        <!-- Link latency (between group and scheduling servers) -->
            <ldbw>{self.ldbw or 2}Mbps</ldbw>            <!-- Link bandwidth (between group and data servers) -->
            <ldlatency>{self.ldlatency or 50}us</latency>        <!-- Link latency (between group and data servers) -->    
        
            <success_percentage>{self.success_percentage or 98}</success_percentage> <!-- Percentage of successful results -->
            <canonical_percentage>{self.canonical_percentage or 84}</canonical_percentage> <!-- Percentage of canonical results -->
        <gproject/>
"""