# This script periodically retrieves expected rainfall from buitenradar.nl, see:
#     https://www.buienradar.nl/overbuienradar/gratis-weerdata
# Example URL:
#     https://gps.buienradar.nl/getrr.php?lat=51&lon=3
# Rain intensity in mm/hour must be transformed using:
#     rain_intensity = 10^((value-109)/32)
# The buienradar API gives predictions for the next 24 time instances every 5 minutes, e.g.
# if you retrieve data at 22:14 you get predictions for 22:15, 22:20, 22:25, etc.
# The table to prepare looks like:
#     datetime        t0    t1  ... t23
# 2024-12-02 22:14    0.1   0.2 ... 0.4
# 2024-12-02 22:19    0.2   0.4 ... 0.7
#
# This script also retrieves the published number of visible stars from a nearby
# RMS camera from the Global Meteor Network
#
# The script is best run on a standalone headless computing node (e.g. RPi).
# Run as daemon from a ssh shell:
# https://stackoverflow.com/questions/19233529/run-bash-script-as-daemon
# setsid python online-sensors.py >/dev/null 2>&1 < /dev/null &
# ToDo: run as startup daemon
import os
from datetime import datetime, timedelta, timezone
import time

from dotenv import load_dotenv
import paho.mqtt.client as mqtt
import requests

load_dotenv()

# MQTT configuration
STATION = "NL000W"  # Utrecht; NL001A: Alphen aan den Rijn
MQTT_BROKER = "6831f8e4add443adb5ccd2fac74382e0.s1.eu.hivemq.cloud"
MQTT_PORT = 8883  # SSL/TLS port
MQTT_USERNAME = os.environ["MQTT_USERNAME"]
MQTT_PASSWORD = os.environ["MQTT_PASSWORD"]
MQTT_CLIENT_ID = f"rain_sensor_{STATION}"
MQTT_BASE_TOPIC = "gmnstation"
camera_status = "uninitialized"
star_counts = []

# Buienradar configuration
SHOWER_URL = "https://gps.buienradar.nl/getrr.php"
NVALUE = 24  # the number of lines returned by the buienradar API
BIKO_LATLON = "lat=52.110425&lon=5.1434641"  # 3573BH 298 backyard
BIKO_CSV = "buien-biko-{}.csv"
# KNMI_LATLON = "lat=52.09957&lon=5.176514"  # De Bilt official measuring station
# KNMI_CSV = "buien-knmi-{}.csv"


def run():
    filename = BIKO_CSV.format(int(time.time()))
    with open(filename, "w") as f:
        time_headers = ','.join([f't{i}' for i in range(24)])
        print(f"datetime,nstars,{time_headers}", file=f)
    # filename = KNMI_CSV.format(int(time.time()))
    # with open(filename, "w") as f:
    #     print(f"datetime,{time_headers}", file=f)
    while True:
        wait_one_minute_before()
        predict_dt, rain_values = retrieve_predictions(BIKO_LATLON)
        nstars = process_star_counts()
        with open(filename, "a") as f:
            values = ','.join([f'{v:.1f}' for v in rain_values])
            print(f"{str(predict_dt)[:16]},{nstars:06.1f},{values}", file=f)
        # predict_dt, rain_values = retrieve_predictions(KNMI_LATLON)
        # with open(filename, "a") as f:
        #     values = ','.join([f'{v:.1f}' for v in rain_values])
        #     print(f"{str(predict_dt)[:16]},{values}", file=f)
        time.sleep(150)


def wait_one_minute_before():
    """Waits until 1 minute before a five minute interval"""
    while True:
        current_dt = datetime.now(timezone.utc)
        if int(current_dt.timestamp()) % 300 ==  240:
            break
        time.sleep(0.5)
    print(f"{str(current_dt)[:21]}")


def retrieve_predictions(latlon):
    # Generate expected local prediction times in hh:mm format
    predict_dt = datetime.now()
    start_m = 5 * (int(predict_dt.minute) // 5 + 1)
    if start_m < 60:
        predict_dt = predict_dt.replace(minute=start_m)
    else:
        predict_dt = predict_dt.replace(minute=0)
        predict_dt += timedelta(minutes=60)
    hms = [predict_dt + i * timedelta(minutes=5) for i in range(NVALUE)]
    hms = [f"{hm.hour:02d}:{hm.minute:02d}" for hm in hms]

    # Put predictions for rain rates in a lookup table by "hh:mm" values
    url = f"{SHOWER_URL}?{latlon}"
    response = requests.get(url)
    lines = response.text.split("\n")[:-1]
    predictions = {}
    for line in lines:
        value, hm = line.split("|")
        rate = round(10 ** ((int(value) - 109) / 32) + 0.01, 1)
        predictions[hm] = rate

    # Look up available predictions for expected prediction times and pad with -1 rates
    # The procedure below accounts for the following edge cases:
    # - earlier minute still in predictions -> prediction ignored
    # - prediction not present -> append -1 to rain_values
    # - predictions out of order -> does not matter
    rain_rates = [predictions.get(hm, -1) for hm in hms]
    # Astronomy wants time in UTC
    tz_timedelta = predict_dt.replace(tzinfo=timezone.utc).astimezone().tzinfo.utcoffset(predict_dt)
    predict_utc = predict_dt - tz_timedelta
    return predict_utc, rain_rates


def process_star_counts():
    if star_counts:
        nstars = sum(star_counts) / len(star_counts)
        star_counts.clear()
        if camera_status == "online":
            return nstars
        else:
            return -1
    else:
        print("Error, no star counts received!")
        return -2


def mqtt_connect():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, MQTT_CLIENT_ID)  # noqa
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.tls_set()
    client.tls_insecure_set(False)  # Set to False if using a valid certificate
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    while True:
        try:
            client.connect(MQTT_BROKER, MQTT_PORT)
            break
        except Exception as e:
            print(f"MQTT connection failed: {e}")
            time.sleep(30)
    client.loop_start()


def on_connect(client, _, __, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
        client.subscribe(f"{MQTT_BASE_TOPIC}/{STATION}/status", 0)
        client.subscribe(f"{MQTT_BASE_TOPIC}/{STATION}/stars", 0)
    else:
        print(f"Failed to connect, return code {rc}")


def on_disconnect(_, __, ___):
    print("Disconnected from MQTT broker, trying to reconnect...")


def on_subscribe(_, __, mid, granted_qos):
    print(f"Subscribed to topic {mid} with QOS {granted_qos}")


def on_message(_, __, msg):
    if msg.topic.endswith("status"):
        global camera_status
        if camera_status != msg.payload.decode("utf8"):
            camera_status = msg.payload.decode("utf8")
            print(f"Camera is {camera_status}")
    elif msg.topic.endswith("stars"):
        star_counts.append(int(msg.payload))
        print(msg.topic + " " + msg.payload.decode("utf8"))


if __name__ == "__main__":
    mqtt_connect()
    time.sleep(1)
    run()
