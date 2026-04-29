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


def status_norm_vector(file_path):
    client_dur = {}
    client_unav = {}
    client_idle = {}
    for value in sum_all_statuses(file_path):
        if value[1] == "busy:Project1" or value[1] == "busy:Project2":
            client_dur[value[0]] = round(client_dur.get(value[0], 0) + value[2], 0)
        if value[1] == "unavailable:":
            client_unav[value[0]] = value[2]
        if value[1] == "idle:":
            client_idle[value[0]] = value[2]
    sum_of_param = 0
    i=0
    for key, value in client_dur.items():
        var = round(value/(value+client_idle[key])*100, 2)
        i += 1
        sum_of_param += var
        client_dur[key] = var
    sorted_dict = dict(sorted(client_dur.items(), key=lambda item: item[1]))
    sorted_keys = list(sorted_dict.keys())
    return client_dur[sorted_keys[0]], client_dur[sorted_keys[-1]], round(sum_of_param/i, 2)


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
    