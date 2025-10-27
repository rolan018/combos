import matplotlib.pyplot as plt
import pandas as pd

max_speed = "1"

target = "Results success"

df = pd.read_csv(f"exp/result_20_800.csv")


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

FLOPS = [20, 60, 100, 150, 200, 300, 350, 400, 550, 600, 700, 750]
for flop in FLOPS:
    df1 = df[df["flop"] == flop]
    plot_1d(df1, "norm", f"производительность клиентов {flop} GFLOPS")