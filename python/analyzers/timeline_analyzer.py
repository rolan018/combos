import pandas as pd
import copy
import matplotlib.pyplot as plt
import numpy as np
from python.utils.utils import zip_with_fixed
from matplotlib import collections as mc


def timeline_analyzer(file_path):
    idles = []
    timeline_stat = {}
    data = pd.read_csv(file_path)
    
    types = data.type.unique()
    clients = data.client.unique()

    for client in clients:
        client_stat = {}
        for typ in types:
            timeline = []
            values_of_type = data.query(f'type == \'{typ}\' & client == \'{client}\'')
            starts = values_of_type.start.values
            ends = values_of_type.end.values
            for i in range(len(values_of_type)):
                delta = float(ends[i] - starts[i])
                timeline.append(delta)
            client_stat[typ] = round(sum(timeline))
        timeline_stat[client] = client_stat

    for ts_name in timeline_stat.keys():
        s = 0
        idle = timeline_stat[ts_name]["idle:"]
        ts_dict = timeline_stat[ts_name]
        for val in ts_dict.values():
            s += val
        idles.append(round(idle/s * 100, 3))
    nor = np.linalg.norm(idles)
    idles.append(float(round(nor,3)))
    return idles


FL1 = np.arange(20, 120, 20, dtype=int)
FL2 = np.arange(150, 800, 50, dtype=int)
FLOPS = np.hstack([FL1, FL2])
PROP = np.arange(1000000000, 1000000000000, 5000000000, dtype=int)


BASE_DIR = "/mnt/c/Users/rolan/Desktop/Projects/combos/exp"
dir_output = f"{BASE_DIR}/result_20_800.csv"

with open(dir_output, "w") as fout:
    print(f"flop,prop,c11,c12,c13,c14,c15,c16,c17,c21,c22,c23,c24,c25,c26,c27,norm", file=fout)
    for flop in FLOPS:
        for prop in PROP:
            dir_input = f"{BASE_DIR}/execute_{flop}_{prop}.csv"
            try:
                idles = timeline_analyzer(f"{BASE_DIR}/FLOPS_20_1500/execute_{flop}_{prop}.csv")
                idles = ",".join(list(map(str, idles)))
                print(f"{flop},{prop},{idles}", file=fout)
            except FileNotFoundError:
                print("Not found:", f"execute_{flop}_{prop}.csv")