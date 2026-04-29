import pandas as pd
import copy
import matplotlib.pyplot as plt
import numpy as np
from python.analyzers.timeline_analyzer import union_all_statuses, status_norm_vector
from python.utils.utils import zip_with_fixed
from matplotlib import collections as mc

# FLOPS_0 = np.arange(100, 10300, 300, dtype=int)
# FLOPS_1 = np.arange(11000, 21000, 1000, dtype=int)
# FLOPS_2 = np.arange(30000, 100000, 10000, dtype=int)
# FLOPS = np.concatenate([FLOPS_0, FLOPS_1, FLOPS_2])

# PROP = np.arange(50_000_000_000, 1_500_000_000_000, 50_000_000_000, dtype=int)

FLOPS = np.arange(10, 200, 250, dtype=int)

PROP = np.arange(10_000_000_000, 20_000_000_000, 50_000_000_000, dtype=int)


FLOPS = [10]

PROP = [6000000000000]

BASE_DIR = "/mnt/c/Users/rolan/Desktop/Projects/combos/exp/test/point_runs/utilize_tester"
dir_output = f"{BASE_DIR}/merged_timeline.csv"

# with open(dir_output, "w") as fout:
#     print(f"flop,prop,client_name,status,start,end,duration", file=fout)
#     for flop in FLOPS:
#         for prop in PROP:
#             dir_input = f"{BASE_DIR}/execute_{flop}_{prop}.csv"
#             try:
#                 for cl, tp, start, end, duration in union_all_statuses(dir_input):
#                     print(f"{flop},{prop},{cl},{tp},{start},{end},{duration}", file=fout)
#             except FileNotFoundError:
#                 print("Not found:", f"execute_{flop}_{prop}.csv")

with open(dir_output, "w") as fout:
    # print(f"flop,prop,client_name,status,value", file=fout)
    for flop in FLOPS:
        for prop in PROP:
            dir_input = f"{BASE_DIR}/execute_{flop}_{prop}.csv"
            try:
                min_value, max_value, avg_value = status_norm_vector(dir_input)
                print(f"MIN busy workload:{min_value}") 
                print(f"MAX busy workload:{max_value}")
                print(f"AVG busy workload:{avg_value}")
                    # print(f"{flop},{prop},{value[0]},{value[1]},{value[2]}", file=fout)
            except FileNotFoundError:
                print("Not found:", f"execute_{flop}_{prop}.csv")

# with open(dir_output, "w") as fout:
#     print(f"flop,prop,c11,c12,c13,c14,c15,c16,c17,c21,c22,c23,c24,c25,c26,c27,norm", file=fout)
#     for flop in FLOPS:
#         for prop in PROP:
#             dir_input = f"{BASE_DIR}/execute_{flop}_{prop}.csv"
#             try:
#                 idles = timeline_analyzer(f"{BASE_DIR}/execute_{flop}_{prop}.csv")
#                 idles = ",".join(list(map(str, idles)))
#                 print(f"{flop},{prop},{idles}", file=fout)
#             except FileNotFoundError:
#                 print("Not found:", f"execute_{flop}_{prop}.csv")