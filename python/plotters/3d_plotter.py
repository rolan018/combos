import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

file_path = "/mnt/c/Users/rolan/Desktop/Projects/combos/exp/FLOPS/min_quorum_3_success_95/after_analyze/timeline_analyzer.csv"
df = pd.read_csv(file_path)

def plot_3d(df, status_name):
    target = df[df['status'] == status_name].groupby(['flop', 'prop'])['value'].mean().reset_index()
    

    fig = plt.figure(figsize=(10, 7))
    ax = fig.add_subplot(111, projection='3d')
    
    # Сетка для поверхности
    X = target['flop'].values
    Y = target['prop'].values
    Z = target['value'].values

    surf = ax.plot_trisurf(X, Y, Z, cmap='coolwarm', edgecolor='none')
    ax.set_xlabel('Flop')
    ax.set_ylabel('Prop')
    ax.set_zlabel('Average Value')
    ax.set_title(f'3D Поверхность для статуса: {status_name}')
    fig.colorbar(surf)
    plt.show()

plot_3d(df, 'idle:')