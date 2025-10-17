
# ToDo: https://docs.streamlit.io/deploy/tutorials/docker

from datetime import date
from pathlib import Path

import pandas as pd
import plotly.express as px
import streamlit as st
from streamlit_calendar import calendar

DATA_PATH = Path("streamlit") / "rain_data.parquet"

with open("streamlit/rain.css") as f:
    custom_css = "\n".join(f.readlines())

calendar_options = {
    "custom_css": custom_css,
    "timeZone": "UTC",  # https://fullcalendar.io/docs/timeZone
    "editable": "false",
    "aspectRatio": 2.,
    "navLinks": "false",
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
    df["ymd"] = df.datetime.dt.strftime("%Y-%m-%d")
    return df


def build_events(year_month):
    df = read_data()
    df = df.loc[df.ymd.str.startswith(year_month)]
    print("!!!df", df)
    month_events = []
    for day in df.ymd.drop_duplicates():
        nclear = df.loc[(df.ymd == day) & (df.deltaT > 0.5)].size
        nwet = df.loc[(df.ymd == day) & (df.numwet > 0)].size
        month_events.append(
            {
                "title": f"{nclear}:{nwet}",
                "color": "#FF6C6C",
                "start": day,
            }
        )
    return month_events


# =============== PAGE LAYOUT ====================

st.title('Clear Sky Detector Utrecht')

calendar_state = calendar(
    events=build_events("2025-06"),
    options=calendar_options,
    key="daygrid",
)
if calendar_state.get("callback") == "eventClick":
    st.session_state["selected_date"] = calendar_state["eventClick"]["event"]["start"]


@st.dialog("Sky photographs", width="large")
def show_photograph(skydatetime):
    st.image("streamlit/lemmon.png", width=1024)


# 60.000 punten gaat nog net goed (31 x 24 x 60 = 44.640)
# 600.000 punten crasht in browser (365 x 24 x 60 = 525.600)
# Voorlopig pragmatisch: data voor een dag
read_data()  # Be sure caching happens on startup
selected_date = st.session_state.get("selected_date")
if not selected_date:
    selected_date = date.today().isoformat()
df_chart = read_data()
df_chart = df_chart.loc[df_chart.datetime.dt.strftime("%Y-%m-%d") == f"{selected_date}"]  # selected_date]
if len(df_chart) > 0:
    fig = px.scatter(
        df_chart,
        x=df_chart.datetime,
        y=[df_chart.nstars, df_chart.numwet, df_chart.deltaT, df_chart.t0, df_chart.t1],
        width=1200,
    )
    fig.update_traces(marker=dict(size=3), mode="lines+markers")
    chart_state = st.plotly_chart(fig, on_select="rerun")
    try:
        skydate = chart_state["selection"]["points"][0]["x"]
        show_photograph(skydate)
    except IndexError:
        pass  # OK, no selection available
    st.write(chart_state)
    if calendar_state.get("callback") == "eventClick":
        skydate = calendar_state["eventClick"]["event"]["start"]
        st.write(skydate)

st.write(calendar_state)

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