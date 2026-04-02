import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt


file_path = "/mnt/c/Users/rolan/Desktop/Projects/combos/exp/FLOPS/min_quorum_3_success_95/after_analyze/timeline_analyzer.csv"
df = pd.read_csv(file_path)

# Агрегируем данные по статусам
def plot_heatmaps(df):
    g = sns.FacetGrid(df, col="status", col_wrap=2, height=4, sharey=True)
    
    def draw_heatmap(*args, **kwargs):
        data = kwargs.pop('data')
        # Создаем pivot-таблицу для тепловой карты
        pivot = data.pivot_table(index='flop', columns='prop', values='value', aggfunc='mean')
        sns.heatmap(pivot, annot=False, cmap='YlOrRd', cbar=True, **kwargs)

    g.map_dataframe(draw_heatmap)
    g.fig.suptitle('Среднее время в статусе (в зависимости от flop и prop)', y=1.02)
    plt.show()

plot_heatmaps(df)