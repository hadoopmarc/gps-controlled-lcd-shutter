from datetime import datetime, timedelta
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

TESTRUN_DIR = "testrun-2025-07-05"


def run():
    online_dfs= []
    arduino_dfs = []
    for path in Path(TESTRUN_DIR).glob("*.csv"):
        if "buien" in path.name:
            online_dfs.append(pd.read_csv(
                path,
                parse_dates=["datetime"],
                date_format="ISO8601"
            ))
        else:
            df = (pd.read_csv(
                path,
                sep=r"\s+",
                names=["datetime", "iswet", "t_amb", "t_sky"],
                dtype={"iswet":  str, "t_sky": str}
            ))
            obsdate = datetime.strptime(path.name[:-4].split("-")[-1], "%d%m%y")
            dateshift_indices = list(df.loc[df.datetime == "00:00:00"].index)
            if len(dateshift_indices) == 1:
                dateshift_index = dateshift_indices[0]
            else:
                dateshift_index = len(df)
            df = df.apply(add_date, obsdate=obsdate, dateshift_index=dateshift_index, axis=1)
            arduino_dfs.append(df)
    online_df = (
        pd.concat(online_dfs)
        .rename(columns={"datetime": "datetime_wet"})
        .sort_values(by="datetime_wet")
        .reset_index(drop=True)
    )
    # Data not saved as UTC
    if TESTRUN_DIR <= "testrun-2025-07-05":
        online_df["datetime_wet"] = online_df["datetime_wet"] - timedelta(hours=2)
    arduino_df = (
        pd.concat(arduino_dfs)
        .sort_values(by="datetime")
        .reset_index(drop=True)
    )
    # Self built Arduino sprintf implementation can print the erroneous patterns +000.-4 and -001.-6
    arduino_df["t_sky"] = arduino_df["t_sky"].str.replace(
        r"([+-][\d]{3})[.]-([\d]+)",
        lambda m: f"{m.group(1)}.{m.group(2)}" if m.group(2) != "10" else f"{int(m.group(1)) - 1}.0",
        regex=True
    ).astype(float)
    all_df = (
        pd.merge_asof(arduino_df, online_df, left_on="datetime", right_on="datetime_wet")
        .apply(interpolate_rain, axis=1)
        .drop("datetime_wet", axis=1)
    )
    all_df["deltaT"] = all_df["t_amb"] - all_df["t_sky"]
    all_df["numwet"] = all_df["iswet"].apply(lambda x: len([c for c in x if c == '1']))
    print(all_df)  # .loc[all_df["iswet"] != "0000000000"])
    col_max = {
        "nstars": 200.,
        "numwet": 10.,
        "deltaT": 40.,
        "t0": 6.,
        "t1": 6.
    }
    for col, max_val in col_max.items():
        all_df[col] = all_df[col] / max_val
    sns.set_theme(rc={'figure.figsize': (16., 8.)})
    timerange1 = (all_df["datetime"] >= datetime(2025, 6, 29, 16, 0)) & \
                (all_df["datetime"] <= datetime(2025, 6, 30, 8,0))
    timerange2 = (all_df["datetime"] >= datetime(2025, 7, 2, 14, 0)) & \
                (all_df["datetime"] <= datetime(2025, 7, 3, 6 ,0))
    timerange3 = (all_df["datetime"] >= datetime(2025, 7, 4, 16, 0)) & \
                (all_df["datetime"] <= datetime(2025, 7, 5, 8,0))
    rainplot = sns.lineplot(data=all_df.set_index('datetime')[col_max.keys()])
    rainplot.get_figure().savefig(Path(TESTRUN_DIR) / "rain0.png")
    plt.show()
    rainplot = sns.lineplot(data=all_df.loc[timerange1].set_index('datetime')[col_max.keys()])
    rainplot.get_figure().savefig(Path(TESTRUN_DIR) / "rain1.png")
    plt.show()
    rainplot = sns.lineplot(data=all_df.loc[timerange2].set_index('datetime')[col_max.keys()])
    rainplot.get_figure().savefig(Path(TESTRUN_DIR) / "rain2.png")
    plt.show()
    rainplot = sns.lineplot(data=all_df.loc[timerange3].set_index('datetime')[col_max.keys()])
    rainplot.get_figure().savefig(Path(TESTRUN_DIR) / "rain3.png")
    plt.show()


def add_date(row, obsdate, dateshift_index):
    hours, minutes, seconds = row["datetime"].split(":")  # 16:32:00
    rel_time = timedelta(hours=int(hours), minutes=int(minutes), seconds=int(seconds))
    if row.name >= dateshift_index:
        days = 1
    else:
        days = 0
    row["datetime"] = obsdate + timedelta(days=days) + rel_time
    return row


def interpolate_rain(row):
    rain_names = [f"t{i}" for i in range(24)]
    time_diff = (row["datetime"] - row["datetime_wet"]).total_seconds()
    if pd.isna(time_diff):
        print(f"Time diff is None at {row['datetime']}, should only happen at the start")
    elif time_diff < 0.:
        print("Time diff smaller than 0, should not happen!")
    elif time_diff < 300.:
        for i, name in enumerate(rain_names[:-1]):
            row[name] = ((300. - time_diff) * row[name] + time_diff * row[rain_names[i+1]]) / 300.
    elif time_diff >= 300.:
        pass  # "5 min <= time_diff < 15 min, still to be implemented!"
    return row


if __name__ == "__main__":
    pd.set_option('display.max_rows', 500)
    run()
