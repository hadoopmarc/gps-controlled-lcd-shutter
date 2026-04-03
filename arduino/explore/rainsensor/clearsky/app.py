"""Run as:
 streamlit run streamlit/app.py
"""
import argparse
from datetime import datetime, timedelta
from dateutil.relativedelta import relativedelta
import os
from pathlib import Path
import time

from dotenv import load_dotenv
import pandas as pd
import plotly.express as px
import streamlit as st
from streamlit_calendar import calendar

from read_wireguard import get_en_image_path, RemoteImageException

load_dotenv()
SAMPLE_PATH = Path("clearsky/clearsky_data_sample.parquet")
dialog_datetime = None

with open("clearsky/app.css") as f:
    app_css = "\n".join(f.readlines())
with open("clearsky/fc.css") as f:
    custom_css = " ".join(f.readlines())


@st.cache_data
def read_data(station_no):
    try:
        df = pd.read_parquet(Path("data") / f"clearsky_data_{station_no}.parquet")
    except FileNotFoundError:
        df = pd.read_parquet(SAMPLE_PATH)
    return df


def recent_date(station_no):
    df = read_data(station_no)
    return df.iloc[-1]["datetime"].date()


def build_events(station_no, yearmonth):
    year, month = yearmonth.split("-")
    start_time = datetime(int(year), int(month), 1, 12, minute=0, second=0)
    end_time = start_time + relativedelta(months=1)
    df = read_data(station_no)
    df = df.loc[(df.datetime >= start_time) & (df.datetime <= end_time)]
    month_events = []
    for day in list(df.datetime.dt.date.drop_duplicates())[:-1]:
        # A day in the calendar is displayed as a diagram from
        # "x 12:00 UT" until "x+1 12:00 UT"
        period_24h = (
            (df.datetime >= (datetime.fromordinal(day.toordinal()) + timedelta(hours=12)))
            & (df.datetime < (datetime.fromordinal(day.toordinal()) + timedelta(hours=36)))
        )
        nclear = len(df.loc[period_24h & (df.deltaT > 0.50)])
        nwet = len(df.loc[period_24h & (df.numwet > 0)])
        nstars = len(df.loc[period_24h & (df.nstars > 0.02)])
        month_events.append(
            {
                "title": f"c{nclear} : w{nwet} : s{nstars}",
                "color": "#FF6C6C",
                "start": day.strftime('%Y-%m-%d'),
            }
        )
    return month_events


@st.dialog("Sky photograph", width="large", dismissible=False)
def show_photograph(station_no, skydatetime: datetime):
    assert type(skydatetime) is datetime
    with st.container(horizontal=True, gap="large", vertical_alignment="center"):
        if st.button("Close"):
            st.rerun()
        st.write(skydatetime)
    progress_text = "Downloading image..."
    image_progress = st.progress(0, text=progress_text)

    def update_progress(transferred: int, tobe_transferred: int):
        completed = transferred / tobe_transferred
        image_progress.progress(completed, text=progress_text)

    try:
        image_path = get_en_image_path(station_no, skydatetime, update_progress)
        st.image(image_path, width=896)
    except RemoteImageException:
        st.write("No image available for this date and time")
    image_progress.empty()


def get_calendar_options(station_no):
    return {
        "timeZone": "UTC",  # https://fullcalendar.io/docs/timeZone
        "editable": "false",
        "contentHeight": "240px",  # Full month without scrollbar
        "navLinksDayClick": "false",
        "selectable": "false",
        "headerToolbar": {
            "left": "prev,next",
            "center": "title",
            "right": "today",
        },
        "initialDate": recent_date(station_no).strftime('%Y-%m-%d'),
        "initialView": "dayGridMonth",
    }


def run(station_no):
    st.markdown(f"<style> {app_css} </style>", unsafe_allow_html=True)
    st.set_page_config(layout="wide")

    header = st.container()  # For sticky header with custom css
    header.title(f"Clear Sky Detector EN{station_no}")
    header.write("""<div class='fixed-header'/>""", unsafe_allow_html=True)

    # Chart can just plot 60.000 points (31 x 24 x 60 = 44.640)
    # Browser crashe on 600.000 points (365 x 24 x 60 = 525.600)
    # Pragmatic for now: take data for one day
    chartdate = st.session_state.get(
        "chartdate", (recent_date(station_no) - relativedelta(days=1)).strftime('%Y-%m-%d'))
    year, month, day = chartdate.split("-")
    start_time = datetime(int(year), int(month), int(day), 12, minute=0, second=0)
    end_time = start_time + relativedelta(days=1)
    df_chart = read_data(station_no)
    df_chart = df_chart.loc[(df_chart.datetime >= start_time) & (df_chart.datetime <= end_time)]
    fig = px.scatter(
        df_chart,
        x="datetime",
        y=["nstars", "numwet", "deltaT", "t0", "t1"],
        width=1200,
    )
    fig.update_traces(marker=dict(size=3), mode="lines+markers")
    fig.update_layout(dragmode="zoom")
    config = {
        "displaylogo": False,
        "modeBarButtonsToRemove": ["select", "lasso",  "zoomIn", "zoomOut", "autoScale"],
        "dragmode": "zoom",
    }
    chart_state = st.plotly_chart(
        fig, config=config, selection_mode="points", on_select="rerun")
    try:
        points = chart_state["selection"]["points"]
        if len(points) == 1:  # maybe unnecessary now selection_mode is added
            photodate = datetime.fromisoformat(points[0]["x"])
            if photodate != st.session_state.get("last_photodate"):
                show_photograph(station_no, photodate)
            st.session_state["last_photodate"] = photodate
    except IndexError:
        pass  # OK, no selection available
    yearmonth = st.session_state.get("yearmonth", f"{year}-{month}")
    calendar_state = calendar(
        events=build_events(station_no, yearmonth),
        options=get_calendar_options(station_no),
        custom_css=custom_css,
        key="daygrid",
    )
    if os.getenv("DEBUG"):
        print(f"Callback: {calendar_state.get('callback')} {time.time()}")

    if calendar_state.get("callback") == "eventsSet":
        yearmonth = calendar_state["eventsSet"]["view"]["currentStart"][:7]
        current_events = calendar_state["eventsSet"]["events"]
        current_dates = [x["start"] for x in current_events]
        # Conditional to prevent endless loop
        if st.session_state.get("yearmont") != yearmonth:
            st.session_state["yearmonth"] = yearmonth
            expected_events = build_events(station_no, yearmonth)
            expected_dates = [x["start"] for x in expected_events]
            if expected_dates != current_dates:
                st.rerun()
    if calendar_state.get("callback") == "eventClick":
        # Conditional to prevent endless loop
        if st.session_state.get("chartdate") != calendar_state["eventClick"]["event"]["start"]:
            st.session_state["chartdate"] = calendar_state["eventClick"]["event"]["start"]
            st.rerun()

    if os.getenv("DEBUG"):
        try:
            st.write(chart_state)
        except NameError:
            pass  # Happens on startup when no data are available for the current date
        st.write(calendar_state)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("en_station", type=str, help="EN station 900 <= int < 1000")
    args = parser.parse_args()
    run(args.en_station)
