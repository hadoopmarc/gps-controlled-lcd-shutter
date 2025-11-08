"""
Simplifying assumptions for processing:
 - gathering training data will not cover much more than a year
 - the number of data points is sufficiently small that all data can be
   (re)processed every time when new data is added and can be stored in
   a single parquet file
 - time ranges of a physical data file may overlap with these from multiple
   online data files, and vice versa
"""

from datetime import datetime, timedelta
from pathlib import Path

import numpy as np
import pandas as pd

RAW_DIR = "data/raw"


def run():
    # Read csv
    online_dfs = []
    arduino_dfs = []
    for path in Path(RAW_DIR).glob("*.csv"):
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

    # Online data
    online_df = (
        pd.concat(online_dfs)
        .rename(columns={"datetime": "datetime"})
        .reset_index(drop=True)
    )
    # Data before 2025-07-07 were not saved as UTC
    no_utc = datetime(2025,7,7)
    online_df.loc[online_df.datetime < no_utc, "datetime"] = \
        online_df.loc[online_df.datetime < no_utc, "datetime"] - timedelta(hours=2)
    interpolated_dfs = [online_df]
    for dt in range(1,5):
        shifted_df = online_df.loc[:].apply(interpolate_rain, dt=dt, axis=1)
        interpolated_dfs.append(shifted_df)
    online_df = pd.concat(interpolated_dfs).sort_values(by="datetime").reset_index(drop=True)
    print(online_df)
    # online_df.to_excel("tmp_online.xlsx")

    # Sensor data
    arduino_df = (
        pd.concat(arduino_dfs, ignore_index=True)
        .sort_values(by="datetime")
        .reset_index(drop=True)
    )
    # Older data had erroneous patterns +000.-4 and -001.-6 for negative temperatures
    arduino_df["t_sky"] = arduino_df["t_sky"].str.replace(
        r"([+-][\d]{3})[.]-([\d]+)",
        lambda m: f"{m.group(1)}.{m.group(2)}" if m.group(2) != "10" else f"{int(m.group(1)) - 1}.0",
        regex=True
    ).astype(float)
    print(arduino_df)
    # arduino_df.to_excel("tmp_arduino.xlsx")

    # Combined data
    all_df = (
        pd.merge_ordered(arduino_df, online_df, left_on="datetime", right_on="datetime", how="outer")
    )
    all_df["deltaT"] = all_df["t_amb"] - all_df["t_sky"]
    all_df["numwet"] = all_df["iswet"].apply(lambda x: len([c for c in x if c == '1']) if pd.notna(x) else np.nan)
    col_max = {
        "nstars": 200.,
        "numwet": 10.,
        "deltaT": 40.,
        "t0": 6.,
        "t1": 6.
    }
    for col, max_val in col_max.items():
        all_df[col] = all_df[col].apply(lambda x: x / max_val if x != -1 else np.nan)
    all_df.to_parquet(Path("data", "rain_data.parquet"))
    # Only run once when data format changes
    # all_df.tail(10000).to_parquet(Path("streamlit", "rain_data_sample.parquet"))
    print(all_df)


def add_date(row, obsdate, dateshift_index):
    hours, minutes, seconds = row["datetime"].split(":")  # 16:32:00
    rel_time = timedelta(hours=int(hours), minutes=int(minutes), seconds=int(seconds))
    if row.name >= dateshift_index:
        days = 1
    else:
        days = 0
    row["datetime"] = obsdate + timedelta(days=days) + rel_time
    return row


def interpolate_rain(row, dt):
    # dt takes values of 1, 2, 3, 4 minutes
    rain_names = [f"t{i}" for i in range(24)]
    for i, name in enumerate(rain_names[:-1]):
        row[name] = ((5. - dt) * row[name] + dt * row[rain_names[i+1]]) / 5.
    row["datetime"] = row["datetime"] + timedelta(minutes=dt)
    return row


if __name__ == "__main__":
    run()
