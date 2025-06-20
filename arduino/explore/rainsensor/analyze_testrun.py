from datetime import datetime, timedelta
from pathlib import Path

import pandas as pd

TESTRUN_DIR = "testrun-2025-06-02"


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
            obsdate = datetime.strptime(path.name[:-4].split("-")[-1], "%d%m%y")
            df = pd.read_csv(
                path,
                sep=r"\s+",
                names=["datetime", "iswet", "t_amb", "t_sky"],
                dtype={"iswet":  str}
            ).apply(add_date, obsdate=obsdate, axis=1)
            arduino_dfs.append(df)
    online_df = (
        pd.concat(online_dfs)
        .rename(columns={"datetime": "datetime_wet"})
        .sort_values(by="datetime_wet")
        .reset_index(drop=True)
    )
    print(online_df)
    arduino_df = (
        pd.concat(arduino_dfs)
        .sort_values(by="datetime")
        .reset_index(drop=True)
    )
    print(arduino_df)
    all_df = (
        pd.merge_asof(arduino_df, online_df, left_on="datetime", right_on="datetime_wet")
        .apply(interpolate_rain, axis=1)
        .drop("datetime_wet", axis=1)
    )
    print(all_df)


def add_date(row, obsdate):
    hours, minutes, seconds = row["datetime"].split(":")
    if int(hours) >= 12:
        days = 0
    else:
        days = 1
    row["datetime"] = obsdate + timedelta(
        days=int(days), hours=int(hours), minutes=int(minutes), seconds=int(seconds))
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
    run()
