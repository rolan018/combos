import pandas as pd
import copy
import matplotlib.pyplot as plt
import numpy as np
from python.utils.utils import zip_with_fixed
from matplotlib import collections as mc


def timeline_analyzer(file_path):
    idles = []
    timeline_stat = {}
    data = pd.read_csv(file_path)
    
    types = data.type.unique()
    clients = data.client.unique()

    for client in clients:
        client_stat = {}
        for typ in types:
            timeline = []
            values_of_type = data.query(f'type == \'{typ}\' & client == \'{client}\'')
            starts = values_of_type.start.values
            ends = values_of_type.end.values
            for i in range(len(values_of_type)):
                delta = float(ends[i] - starts[i])
                timeline.append(delta)
            client_stat[typ] = round(sum(timeline))
        timeline_stat[client] = client_stat

    for ts_name in timeline_stat.keys():
        s = 0
        idle = timeline_stat[ts_name]["idle:"]
        ts_dict = timeline_stat[ts_name]
        for val in ts_dict.values():
            s += val
        idles.append(round(idle/s * 100, 3))
    nor = np.linalg.norm(idles)
    idles.append(float(round(nor,3)))
    return idles


def sum_all_statuses(file_path):
    data = pd.read_csv(file_path)
    # Вычисляем длительность для каждой строки
    data['duration'] = data['end'] - data['start']

    # Группируем по клиенту и типу состояния, суммируем длительность
    result = data.groupby(['client', 'type'])['duration'].sum()

    for (client, state), total_time in result.items():
        yield (client, state, round(total_time, 1))


def union_all_statuses(file_path):
    df = pd.read_csv(file_path)
    
    df['diff_client'] = df['client'] != df['client'].shift()
    df['diff_status'] = df['type'] != df['type'].shift()
    df['diff_time'] = df['start'] != df['end'].shift()
    
    # Флаг начала новой группы
    df['new_group'] = df[['diff_client', 'diff_status', 'diff_time']].any(axis=1)
    df['group_id'] = df['new_group'].cumsum()
    
    # 3. Агрегируем
    merged = df.groupby(['group_id', 'client', 'type']).agg(
        start=('start', 'min'),
        end=('end', 'max')
    ).reset_index()
    
    # 4. Добавляем duration
    merged['duration'] = merged['end'] - merged['start']
    
    merged = merged[['client', 'type', 'start', 'end', 'duration']]

    # 5. Возвращаем только нужные колонки
    for index, row in merged.iterrows():
        yield row
    