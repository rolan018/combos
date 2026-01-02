import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
max_speed = "1"

TARGET = "Results success"
INPUT_DATA = "results/resultSpecialFlops"
INPUT_DATA = "results/resultTest/FLOPS_quorum_50percent_100_10000"

def add_plot(i, j, ax, df1, num):
    ax[i, j].plot(df1[df1["proj_name"] == "RakeSearchtype2e13@home"]["param"], df1[df1["proj_name"] == "RakeSearchtype2e13@home"][TARGET], 'bo', c='red', label='RakeSearchtype2e13')
    ax[i, j].plot(df1[df1["proj_name"] == "RakeSearchtype1e15@home"]["param"], df1[df1["proj_name"] == "RakeSearchtype1e15@home"][TARGET], 'bo', c='blue', label='RakeSearchtype1e15')
    # ax[i, j].plot(750000000000, 11120, marker='o', color='green')
    # ax[i, j].axvline(x=750000000000, color='red', ls='--')
    # ax[i, j].vlines(x=450000000000, ymin=11090, ymax=11160, color='green', ls='-')
    # ax[i, j].vlines(x=0, ymin=11090, ymax=11160, color='green', ls='-')
    # ax[i, j].hlines(y=11090, xmin=0, xmax=450000000000, color='green', ls='-')
    # ax[i, j].hlines(y=11160, xmin=0, xmax=450000000000, color='green', ls='-')
    ax[i, j].set_title(f'{num} GFLOPS')
    ax[i, j].set_xlabel("flop")
    ax[i, j].set_ylabel("Успешные Результаты")
    ax[i, j].legend() 



FLOPS = [1, 10, 1000, 100000]
FLOPS = [100, 250, 500, 1000, 2500, 7000]
all_items = int(len(FLOPS)/2)

fig, ax = plt.subplots(nrows=2, ncols=all_items)

for i, flop in enumerate(FLOPS):
    j = i%all_items
    df = pd.read_csv(f"{INPUT_DATA}/result_{flop}.csv")
    add_plot(i//all_items, j, ax, df, flop)

plt.show()