import pandas as pd
import matplotlib.pyplot as plt
from ..utils.utils import zip_with_fixed


def network_analyzer(target_column):
    file_name = "results/resultNetwork/result.csv"
    df = pd.read_csv(file_name)
    lsbw = df["lsbw"]
    ldbw = df["ldbw"]
    target = df[target_column]
    target_max = df.iloc[df[target_column].idxmax()][target_column]
    df_filtered = df[df["Workunits completed"] >= target_max]
    print(df_filtered.transpose())
