"""Run as:
 streamlit run streamlit/app.py
"""
from datetime import datetime
from dateutil.relativedelta import relativedelta
import os
from pathlib import Path
import time

from dotenv import load_dotenv
import pandas as pd
import plotly.express as px
import streamlit as st
from streamlit_calendar import calendar

from read_wireguard import get_en_image_path

load_dotenv()
DATA_PATH = Path("data") / "rain_data.parquet"  # mount point for user data in Docker
SAMPLE_PATH = Path("clearsky/rain_data_sample.parquet")

with open("clearsky/app.css") as f:
    app_css = "\n".join(f.readlines())
with open("clearsky/fc.css") as f:
    custom_css = " ".join(f.readlines())


@st.cache_data
def read_data():
    try:
        df = pd.read_parquet(DATA_PATH)
    except FileNotFoundError:
        df = pd.read_parquet(SAMPLE_PATH)
    return df


def recent_date():
    df = read_data()
    return df.iloc[-1]["datetime"].date()


def build_events(yearmonth):
    year, month = yearmonth.split("-")
    start_time = datetime(int(year), int(month), 1, 12, minute=0, second=0)
    end_time = start_time + relativedelta(months=1)
    df = read_data()
    df = df.loc[(df.datetime >= start_time) & (df.datetime <= end_time)]
    month_events = []
    for day in list(df.datetime.dt.date.drop_duplicates())[:-1]:
        nclear = len(df.loc[(df.datetime.dt.date == day) & (df.deltaT > 0.5)])
        nwet = len(df.loc[(df.datetime.dt.date == day) & (df.numwet > 0)])
        month_events.append(
            {
                "title": f"c{nclear}:w{nwet}",
                "color": "#FF6C6C",
                "start": day.strftime('%Y-%m-%d'),
            }
        )
    return month_events


@st.dialog("Sky photograph", width="large")
def show_photograph(skydatetime: datetime):
    assert type(skydatetime) is datetime
    # skydatetime = datetime.fromisoformat("2026-01-20 17:44:44")
    image_path = get_en_image_path(skydatetime)
    st.image(image_path, width=1024)


calendar_options = {
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
    "initialDate": recent_date().strftime('%Y-%m-%d'),
    "initialView": "dayGridMonth",
}


def run():
    st.markdown(f"<style> {app_css} </style>", unsafe_allow_html=True)
    st.set_page_config(layout="wide")

    header = st.container()  # For sticky header with custom css
    header.title("Clear Sky Detector Utrecht")
    header.write("""<div class='fixed-header'/>""", unsafe_allow_html=True)

    # Chart can just plot 60.000 points (31 x 24 x 60 = 44.640)
    # Browser crashe on 600.000 points (365 x 24 x 60 = 525.600)
    # Pragmatic for now: take data for one day
    chartdate = st.session_state.get(
        "chartdate", (recent_date() - relativedelta(days=1)).strftime('%Y-%m-%d'))
    year, month, day = chartdate.split("-")
    start_time = datetime(int(year), int(month), int(day), 12, minute=0, second=0)
    end_time = start_time + relativedelta(days=1)
    df_chart = read_data()
    df_chart = df_chart.loc[(df_chart.datetime >= start_time) & (df_chart.datetime <= end_time)]
    if len(df_chart) > 0:
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
                    show_photograph(photodate)
                st.session_state["last_photodate"] = photodate
        except IndexError:
            pass  # OK, no selection available
    yearmonth = st.session_state.get("yearmonth", f"{year}-{month}")
    calendar_state = calendar(
        events=build_events(yearmonth),
        options=calendar_options,
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
            expected_events = build_events(yearmonth)
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
    run()
