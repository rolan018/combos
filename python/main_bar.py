from python.analyzers.timeline_analyzer import timeline_analyzer
import numpy as np
import matplotlib.pyplot as plt  

# Avg LSBW: 4.2644927536231885
# Min LSBW: 1.0
# Max LSBW: 7.5
# Avg LDBW: 10.130434782608695
# Min LDBW: 1
# Max LDBW: 19

serv = ["c10", "c11", "c12", "c13", "c14", 
        "c15", "c16", "c17", "c18", "c19",
        "c20", "c21", "c22", "c23", "c24", 
        "c25", "c26", "c27", "c28", "c29", ]
client_nms, statistics = timeline_analyzer(serv)
data = np.array(statistics)
categories = [i for i in client_nms]  # Создаем подписи для каждой полосы
print(categories)
groups = ['busy:Project2', 'busy:Project1', 'idle', 'unavailable']  # Подписи для рядов

# Создаем график
fig, ax = plt.subplots(figsize=(12, 8))  # Увеличим размер графика для лучшей читаемости

# Рисуем stacked bar chart
ax.bar(categories, data[:, 0], label=groups[0])
ax.bar(categories, data[:, 1], bottom=data[:, 0], label=groups[1])
ax.bar(categories, data[:, 2], bottom=data[:, 0] + data[:, 1], label=groups[2])
ax.bar(categories, data[:, 3], bottom=data[:, 0] + data[:, 1] + data[:, 2], label=groups[3])

# Добавляем подписи к осям и заголовок
ax.set_xlabel('Bars')
ax.set_ylabel('Percentage')
ax.set_title('Total 100%')

# Добавляем легенду
ax.legend()

# Поворачиваем подписи на оси X для лучшей читаемости
plt.xticks(rotation=45, ha='right')

# Отображаем график
plt.tight_layout()  # Чтобы подписи не обрезались
plt.show()