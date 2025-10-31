
# ToDo: dockerize https://docs.streamlit.io/deploy/tutorials/docker

from datetime import date, datetime
from dateutil.relativedelta import relativedelta
from pathlib import Path

import pandas as pd
import plotly.express as px
import streamlit as st
from streamlit_calendar import calendar

DATA_PATH = Path("streamlit") / "rain_data.parquet"

with open("streamlit/app.css") as f:
    app_css = "\n".join(f.readlines())
with open("streamlit/fc.css") as f:
    custom_css = " ".join(f.readlines())

calendar_options = {
    "timeZone": "UTC",  # https://fullcalendar.io/docs/timeZone
    "editable": "false",
    "contentHeight": "200px",  # Full month without scrollbar
    "navLinksDayClick": "false",
    "selectable": "true",
    "headerToolbar": {
        "left": "prev,next",
        "center": "title",
        "right": "today",
    },
    "initialDate": "2025-07-01",
    "initialView": "dayGridMonth",
}


@st.cache_data
def read_data():
    df = pd.read_parquet(DATA_PATH)
    return df


def build_events(year_month):
    year, month = year_month.split("-")
    start_time = datetime(int(year), int(month), 1, 12, minute=0, second=0)
    end_time = start_time + relativedelta(months=1)
    df = read_data()
    df = df.loc[(df.datetime >= start_time) & (df.datetime <= end_time)]
    month_events = []
    for day in list(df.datetime.dt.date.drop_duplicates())[:-1]:
        nclear = df.loc[(df.datetime.dt.date == day) & (df.deltaT > 0.5)].size
        nwet = df.loc[(df.datetime.dt.date == day) & (df.numwet > 0)].size
        month_events.append(
            {
                "title": f"{nclear}:{nwet}",
                "color": "#FF6C6C",
                "start": day.strftime('%Y-%m-%d'),
            }
        )
    return month_events


@st.dialog("Sky photograph", width="large")
def show_photograph(skydatetime):
    st.image("streamlit/lemmon.png", width=1024)


def run():
    st.markdown(f"<style> {app_css} </style>", unsafe_allow_html=True)
    st.set_page_config(layout="wide")

    header = st.container()  # For sticky header with custom css
    header.title("Clear Sky Detector Utrecht")
    header.write("""<div class='fixed-header'/>""", unsafe_allow_html=True)

    # 60.000 punten gaat nog net goed (31 x 24 x 60 = 44.640)
    # 600.000 punten crasht in browser (365 x 24 x 60 = 525.600)
    # Voorlopig pragmatisch: data voor een dag
    read_data()  # Be sure caching happens on startup
    chartdate = st.session_state.get("chartdate")
    if not chartdate:
        chartdate = date.today().isoformat()
    year, month, day = chartdate.split("-")
    start_time = datetime(int(year), int(month), int(day), 12, minute=0, second=0)
    end_time = start_time + relativedelta(days=1)
    df_chart = read_data()
    df_chart = df_chart.loc[(df_chart.datetime >= start_time) & (df_chart.datetime <= end_time)]
    if len(df_chart) > 0:
        fig = px.scatter(
            df_chart,
            x=df_chart.datetime,
            y=[df_chart.nstars, df_chart.numwet, df_chart.deltaT, df_chart.t0, df_chart.t1],
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
                photodate = points[0]["x"]
                if photodate != st.session_state.get("last_photodate"):
                    show_photograph(photodate)
                st.session_state["last_photodate"] = photodate
        except IndexError:
            pass  # OK, no selection available

    calendar_state = calendar(
        events=build_events("2025-06"),
        options=calendar_options,
        custom_css=custom_css,
        key="daygrid",
    )
    if calendar_state.get("callback") == "eventClick":
        # Conditional to prevent endless loop
        if st.session_state.get("chartdate") != calendar_state["eventClick"]["event"]["start"]:
            st.session_state["chartdate"] = calendar_state["eventClick"]["event"]["start"]
            st.rerun()

    try:
        st.write(chart_state)
    except NameError:
        pass  # OK, happens on startup when no data are available for the current date
    st.write(calendar_state)


if __name__ == "__main__":
    run()

# Initial calendar_state:
# {
#   "callback": "eventsSet",
#   "eventsSet": {
#     "events": [
#       {
#         "allDay": true,
#         "title": "24h",
#         "start": "2025-07-03",
#         "backgroundColor": "#FF6C6C",
#         "borderColor": "#FF6C6C"
#       }
#     ],
#     "view": {
#       "type": "dayGridMonth",
#       "title": "June 2025",
#       "activeStart": "2025-05-31T22:00:00.000Z",
#       "activeEnd": "2025-07-12T22:00:00.000Z",
#       "currentStart": "2025-05-31T22:00:00.000Z",
#       "currentEnd": "2025-06-30T22:00:00.000Z"
#     }
#   }
# }

# After day click:
# {
#   "callback": "eventsSet",
#   "eventsSet": {
#     "events": [
#       {
#         "allDay": true,
#         "title": "24h",
#         "start": "2025-07-03",
#         "backgroundColor": "#FF6C6C",
#         "borderColor": "#FF6C6C"
#       }
#     ],
#     "view": {
#       "type": "dayGridDay",
#       "title": "June 23, 2025",
#       "activeStart": "2025-06-22T22:00:00.000Z",
#       "activeEnd": "2025-06-23T22:00:00.000Z",
#       "currentStart": "2025-06-22T22:00:00.000Z",
#       "currentEnd": "2025-06-23T22:00:00.000Z"
#     }
#   }
# }
