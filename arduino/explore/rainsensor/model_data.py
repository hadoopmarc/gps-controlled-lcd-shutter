
import argparse
from datetime import datetime, timedelta
from pathlib import Path

from astral import Observer, sun
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns

# Needs pip install astral, matplotlib, openpyxl, seaborn, PyQt5

en_stations = {
    915: Observer(52.091, 5.122, 0.0)  # Utrecht
}


def run(station_no):
    df = pd.read_parquet(Path("data") / f"clearsky_data_{station_no}.parquet")
    df = df.loc[df.datetime.notna()]
    print(df)
    # First try a zero order model with a single threshold value for deltaT.
    # Prepare a diagram with fractions of measured minutes (cumulative values)
    # above a threshold value deltaT >= DT for:
    # - the "clear sky line" nstars > 0
    # - the "wet line" nwet > 0
    # This model works if these lines do not intersect

    # Only use night observations because deltaT is influenced by direct
    # incidence of light and heat from the sun.
    dusk_dawn_times = get_dusk_dawn(station_no, df["datetime"])
    df["isnight"] = df["datetime"].apply(
        lambda obs_dt: (obs_dt > dusk_dawn_times[obs_dt.date()][0])
        and (obs_dt < dusk_dawn_times[obs_dt.date()][1])
    )
    night_df = df.loc[df.isnight]

    # Limit of nstars >= 0.02 is a bit arbitrary, but accounts for conditions of
    # limited use with just one or two bright stars or planets visible.
    # Including these conditions makes it much harder to discriminate between
    # dense cirrus clouds and other clouds with precipitation.
    dt_values = [x / 100 for x in range(0, 130)]
    clear_numbers = [
        len(night_df.loc[(night_df.deltaT >= dt) & (night_df.nstars >= 0.02)])
        for dt in dt_values
    ]
    clear_percentages = [x / max(clear_numbers) for x in clear_numbers]
    wet_numbers = [
        len(night_df.loc[(night_df.deltaT < dt) & (night_df.numwet > 0)])
        for dt in dt_values
    ]
    wet_percentages = [x / max(wet_numbers) for x in wet_numbers]
    optimize_df = pd.DataFrame(
        zip(dt_values, clear_percentages, wet_percentages),
        columns=["deltaTmin", "clear-line", "wet-line"]
    ).set_index("deltaTmin")
    sns.set_theme(rc={'figure.figsize': (16., 8.)}, font_scale=2.)
    optiplot = sns.lineplot(data=optimize_df, linewidth=2.)
    optiplot.get_figure().savefig(Path("data") / "model" / "deltaT_0.png")
    plt.show()

    # Write crossing point of clearline and wetline
    optimize_df.reset_index(inplace=True)
    optimize_df["abs_diff"] = (optimize_df["clear-line"] - optimize_df["wet-line"]).abs()
    optirow = optimize_df.sort_values(by="abs_diff", ascending=True).iloc[0]
    print(optirow)

    # Write lists of critical minutes to xlsx
    # xlsx_night_path = Path("data") / "model" / "all_nights.xlsx"
    # night_df.to_excel(xlsx_night_path)
    xlsx_w060_path = Path("data") / "model" / f"wetline060-{station_no}.xlsx"
    df_w060 = night_df.loc[(night_df.numwet > 0) & (0.60 <= night_df.deltaT)]
    df_w060.to_excel(xlsx_w060_path)
    print(f"Critical minutes for wetline, all: {len(night_df)}, "
          f"wet: {len(night_df.loc[(night_df.numwet > 0)])}, deltaT>=0.60: {len(df_w060)}")

    xlsx_c060_path = Path("data") / "model" / f"clearline060-{station_no}.xlsx"
    df_c060 = night_df.loc[(night_df.nstars >= 0.05) & (night_df.deltaT < 0.60)]
    df_c060.to_excel(xlsx_c060_path)
    print(f"Critical minutes for clearline, all: {len(night_df)}, "
          f"stars: {len(night_df.loc[(night_df.nstars >= 0.05)])}, 0.54<=deltaT<0.60: {len(df_c060)}")

    # Write critical nights to download instructions
    critical_nights = set()
    for dt in pd.concat([df_w060, df_c060])["datetime"]:
        # Times of images to be downloaded for a day run from
        # "x 12:00 UT" until "x+1 12:00 UT"
        if dt.hour >= 12:
            critical_nights.add(dt.date())
        else:
            critical_nights.add(dt.date() - timedelta(days=1))
    with open(Path("data", f"en9xx-downloads.txt"), "w") as f:
        for night in sorted(critical_nights):
            f.write(f"{station_no} {night}\n")


def get_dusk_dawn(station_no, obs_datetimes: pd.Series):
    observer = en_stations[station_no]
    dd_tups = {}
    for obs_date in obs_datetimes.dt.date.unique():
        s_today = sun.sun(observer, obs_date, dawn_dusk_depression=6.)
        s_tomorrow = sun.sun(observer, obs_date + timedelta(days=1), dawn_dusk_depression=6.)
        # Make UTC timezone implicit to avoid problems with date comparisons and xlsx ouotput
        dd_tups[obs_date] = (
            s_today["dusk"].replace(tzinfo=None),
            s_tomorrow["dawn"].replace(tzinfo=None)
        )
    return dd_tups


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("en_station", type=int, help="EN station 900 <= int < 1000")
    args = parser.parse_args()
    run(args.en_station)
