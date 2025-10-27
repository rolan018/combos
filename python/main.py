from python.analyzers.timeline_analyzer import timeline_analyzer

import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path

experiment_dir = Path(__file__).parent

FLOPS = np.arange(100, 1000, 20, dtype=int)
# FLOPS = [100000]
PROP = [3000000000, 7000000000, 10000000000, 
        20000000000, 40000000000, 60000000000, 
        80000000000, 100000000000, 140000000000,
        180000000000]
serv = ["c10", "c11", "c12", "c13", "c14", 
        "c15", "c16", "c17", "c18", "c19",
        "c20", "c21", "c22", "c23", "c24", 
        "c25", "c26", "c27", "c28", "c29", ]

with open(experiment_dir / "result.csv", "w") as fout:
    print(f"flop,prop,c11,c12,c13,c14,c15,c16,c17,c21,c22,c23,c24,c25,c26,c27,norm", file=fout)
    for flop in FLOPS:
        for prop in PROP:
            idles = timeline_analyzer(f"exp/FLOPS_100_1000/execute_{flop}_{prop}.csv", serv)
            nor = round(np.linalg.norm(idles),2)
            idles = ",".join(list(map(str, idles)))
            print(f"{flop},{prop},{idles},{nor}", file=fout)