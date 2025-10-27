import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
max_speed = "1"

target = "Results success"

df1 = pd.read_csv(f"results/resultBigFlops_03_07/result_0_5.csv")
df10 = pd.read_csv(f"results/resultBigFlops_03_07/result_10.csv")
df1000 = pd.read_csv(f"results/resultBigFlops_03_07/result_1000.csv")
df100000 = pd.read_csv(f"results/resultBigFlops_03_07/result_100000.csv")

avx_1 = 1
avx_2 = 2
# fig, ax = plt.subplots(nrows=2, ncols=2)

plt.plot(df1[df1["proj_name"] == "RakeSearchtype2e13@home"]["param"], df1[df1["proj_name"] == "RakeSearchtype2e13@home"][target], 'bo', c='red', label='RakeSearchtype2e13')
plt.plot(df1[df1["proj_name"] == "RakeSearchtype1e15@home"]["param"], df1[df1["proj_name"] == "RakeSearchtype1e15@home"][target], 'bo', c='blue', label='RakeSearchtype1e15')
plt.axvline(x=75000000000, color='red', ls='--')
plt.axvline(x=150000000000, color='red', ls='--')
# ax[0, 1].plot(df10[df10["proj_name"] == "RakeSearchtype2e13@home"]["param"], df10[df10["proj_name"] == "RakeSearchtype2e13@home"][target], 'bo', c='red', label='RakeSearchtype2e13')
# ax[0, 1].plot(df10[df10["proj_name"] == "RakeSearchtype1e15@home"]["param"], df10[df10["proj_name"] == "RakeSearchtype1e15@home"][target], 'bo', c='blue', label='RakeSearchtype1e15')
# ax[0, 1].axvline(x=75000000000, color='red', ls='--')
# ax[0, 1].axvline(x=150000000000, color='red', ls='--')
# ax[1, 0].plot(df1000[df1000["proj_name"] == "RakeSearchtype2e13@home"]["param"], df1000[df1000["proj_name"] == "RakeSearchtype2e13@home"][target], 'bo', c='red', label='RakeSearchtype2e13')
# ax[1, 0].plot(df1000[df1000["proj_name"] == "RakeSearchtype1e15@home"]["param"], df1000[df1000["proj_name"] == "RakeSearchtype1e15@home"][target], 'bo', c='blue', label='RakeSearchtype1e15')
# ax[1, 0].axvline(x=75000000000, color='red', ls='--')
# ax[1, 0].axvline(x=150000000000, color='red', ls='--')
# ax[1, 1].plot(df100000[df100000["proj_name"] == "RakeSearchtype2e13@home"]["param"], df100000[df100000["proj_name"] == "RakeSearchtype2e13@home"][target], 'bo', c='red', label='RakeSearchtype2e13')
# ax[1, 1].plot(df100000[df100000["proj_name"] == "RakeSearchtype1e15@home"]["param"], df100000[df100000["proj_name"] == "RakeSearchtype1e15@home"][target], 'bo', c='blue', label='RakeSearchtype1e15')
# ax[1, 1].axvline(x=75000000000, color='red', ls='--')
# ax[1, 1].axvline(x=150000000000, color='red', ls='--')

plt.title('100 GFLOPS')
plt.xlabel("flop")
plt.ylabel("Results success")
plt.legend()

# ax[0, 1].set_title('10 GFLOPS')
# ax[0, 1].set_xlabel("flop")
# ax[0, 1].set_ylabel("Успешные Результаты")
# ax[0, 1].legend()
# ax[1, 0].set_title('1000 GFLOPS')
# ax[1, 0].set_xlabel("flop")
# ax[1, 0].set_ylabel("Успешные Результаты")
# ax[1, 0].legend()
# ax[1, 1].set_title('100000 GFLOPS')
# ax[1, 1].set_xlabel("flop")
# ax[1, 1].set_ylabel("Успешные Результаты")
# ax[1, 1].legend()


plt.show()

# def plot_1d(x_label, y_label, target):
#     ax = plt.figure().add_subplot()
#     ax.set_xlabel(x_label)
#     ax.set_ylabel(y_label)
#     line, = ax.plot(df["param"], df[target], "o")
#     ax.legend()
#     plt.title(f'{target}({x_label})')
#     plt.show()
# plot_1d("fpops", "Results too late", "Results too late")
# plot_1d("fpops", "Workunits completed", "Workunits completed")
# plot_1d("fpops", "Results received", "Results received")