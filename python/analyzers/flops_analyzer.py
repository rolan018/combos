import pandas as pd
import numpy as np

target = "Results too late"
FLOPS = np.arange(100, 1050, 50, dtype=int)

def flop_list_analyzer(flops, target_column):
    target_list = []
    for flop in flops:
        df = pd.read_csv(f"resultSpecialFlops/result_{flop}.csv")
        fpops = df.iloc[df[target_column].idxmax()]["param"]
        target_list.append(fpops)
        print(f"max in {flop}:", fpops)

    print("Avg:", np.average(target_list))
    print("Min:", np.min(target_list))
    print("Max:", np.max(target_list))