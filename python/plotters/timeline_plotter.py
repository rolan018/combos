import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

INPUT_DATA = "exp/FLOPS/min_quorum_3_success_95/result.csv"

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
    # ax[i, j].hlines(y=240, xmin=0, xmax=450000000000, color='green', ls='-')
    # ax[i, j].vlines(x=450000000000, ymin=200, ymax=240, color='green', ls='-')
    # ax[i, j].vlines(x=0, ymin=200, ymax=240, color='green', ls='-')
    # ax[i, j].hlines(y=200, xmin=0, xmax=450000000000, color='green', ls='-')
    ax[i, j].set_title(title)
    ax[i, j].set_xlabel(x_label)
    ax[i, j].set_ylabel(y_label)
    ax[i, j].legend() 

FLOPS = [100, 400, 1000, 10000, 12000, 15000, 16000, 20000, 40000, 90000]

ROWS = 2
all_items = int(len(FLOPS)/ROWS)

fig, ax = plt.subplots(nrows=ROWS, ncols=all_items)

for i, flop in enumerate(FLOPS):
    j = i%all_items
    df1 = df[df["flop"] == flop]
    title = f"производительность клиентов {flop} GFLOPS"
    add_plot(i//all_items, j, ax, df1, "norm", title)

plt.show()