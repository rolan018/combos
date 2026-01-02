import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

INPUT_DATA = "results/result_100_8500.csv"
INPUT_DATA = "results/resultTest/FLOPS_quorum_50percent_100_10000/result.csv"

df = pd.read_csv(INPUT_DATA)

def plot_1d(df, target, title):
    x_label = "Сложность задач"
    y_label = "Норма вектора ожидания клиентов"
    ax = plt.figure().add_subplot()
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)
    line, = ax.plot(df["prop"], df[target], "o")
    ax.axvline(x=100000000000, color='red', ls='--')
    ax.legend()
    plt.title(title)
    plt.show()

def add_plot(i, j, ax, df, target, title):
    x_label = "Сложность задач"
    y_label = "Норма вектора ожидания клиентов"
    ax[i, j].plot(df["prop"], df[target], "o")
    ax[i, j].hlines(y=240, xmin=0, xmax=450000000000, color='green', ls='-')
    ax[i, j].vlines(x=450000000000, ymin=200, ymax=240, color='green', ls='-')
    ax[i, j].vlines(x=0, ymin=200, ymax=240, color='green', ls='-')
    ax[i, j].hlines(y=200, xmin=0, xmax=450000000000, color='green', ls='-')
    ax[i, j].set_title(title)
    ax[i, j].set_xlabel(x_label)
    ax[i, j].set_ylabel(y_label)
    ax[i, j].legend() 

FLOPS = [100, 250, 500, 1000, 2500, 10000]
all_items = int(len(FLOPS)/2)

fig, ax = plt.subplots(nrows=2, ncols=all_items)

for i, flop in enumerate(FLOPS):
    j = i%all_items
    df1 = df[df["flop"] == flop]
    add_plot(i//all_items, j, ax, df1, "norm", f"производительность клиентов {flop} GFLOPS")

    # df1 = df[df["flop"] == flop]
    # plot_1d(df1, "norm", f"производительность клиентов {flop} GFLOPS")

plt.show()