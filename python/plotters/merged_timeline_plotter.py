import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import random

BASE_DIR = "/mnt/c/Users/rolan/Desktop/Projects/combos/exp/test/point_runs/utilize_tester"
dir_input = f"{BASE_DIR}/merged_timeline.csv"

# 1. Загрузка данных
df = pd.read_csv(dir_input)

# Присвоим цвета статусам (можно расширить список)
unique_statuses = df['status'].unique()
colors = plt.cm.tab10(range(len(unique_statuses)))
status_color_map = dict(zip(unique_statuses, colors))

# 2. Получаем уникальные пары flop и prop для группировки
groups = df.groupby(['flop', 'prop'])

# Выбираем 5-6 комбинаций (если их больше, берем первые 6)
# 1. Получаем все уникальные пары flop и prop
unique_combinations = list(df.groupby(['flop', 'prop']).groups.keys())

# 2. Выбираем случайные 
target_groups = [(10, 6000000000000), (1000, 1000000000000), (7000, 900000000000)]
# num_to_show = min(len(unique_combinations), 6)
# target_groups = random.sample(unique_combinations, num_to_show)

# 3. Построение графиков
fig, axes = plt.subplots(len(target_groups), 1, figsize=(12, 3 * len(target_groups)), constrained_layout=True)

# Если групп мало, axes может быть не списком, приводим к списку
if len(target_groups) == 1: axes = [axes]

for ax, (f, p) in zip(axes, target_groups):
    subset = df[(df['flop'] == f) & (df['prop'] == p)]
    
    # Рисуем горизонтальные полосы для каждого клиента
    for i, (name, row) in enumerate(subset.iterrows()):
        duration = row['end'] - row['start']
        ax.barh(row['client_name'], duration, left=row['start'], 
                color=status_color_map[row['status']], edgecolor='black', alpha=0.8)
    
    ax.set_title(f'Simulation: Flop={f}, Prop={p}')
    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Clients')
    ax.grid(axis='x', linestyle='--', alpha=0.7)

# 4. Легенда
legend_patches = [mpatches.Patch(color=status_color_map[s], label=s) for s in unique_statuses]

# Размещаем легенду под графиками
# ncol=3 можно менять, если статусов много
fig.legend(handles=legend_patches, 
           loc='upper center', 
           bbox_to_anchor=(0.5, 0.08), 
           ncol=min(len(unique_statuses), 4),
           frameon=True)

# Ключевой момент: rect определяет область для графиков, оставляя место снизу (bottom=0.1)
plt.tight_layout(rect=[0, 0.1, 1, 1])
plt.show()