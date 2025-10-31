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
# setsid python online-sensors.py < /dev/null > /dev/null 2>&1 &
# ToDo: run as startup daemon
# RPi400 also has a defunct DNS resolv config, so add once:
# sudo sh -c 'echo "nameserver 8.8.8.8" >> /etc/resolv.conf'

import os
from datetime import datetime, timedelta, timezone
import time

from dotenv import load_dotenv
import paho.mqtt.client as mqtt
import requests
from urllib3.util.retry import Retry

load_dotenv()

# MQTT configuration
STATION = "NL000W"  # Utrecht; NL001A: Alphen aan den Rijn
MQTT_BROKER = "6831f8e4add443adb5ccd2fac74382e0.s1.eu.hivemq.cloud"
MQTT_PORT = 8883  # SSL/TLS port
MQTT_USERNAME = os.environ["MQTT_USERNAME"]
MQTT_PASSWORD = os.environ["MQTT_PASSWORD"]
MQTT_CLIENT_ID = f"rain_sensor_{STATION}"
MQTT_BASE_TOPIC = "gmnstation"
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
    try:
        with requests.Session() as session:
            # https://urllib3.readthedocs.io/en/latest/reference/urllib3.util.html#module-urllib3.util.retry
            retry = Retry(total=4, connect=3, backoff_factor=1)  # Beware of exceeding usage limits
            adapter = requests.adapters.HTTPAdapter(max_retries=retry)
            session.mount('https://', adapter)
            response = session.get(url)
        # response = requests.get(url)
        lines = response.text.split("\n")[:-1]
    except requests.exceptions.ConnectionError:
        lines = []
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
        return nstars
    else:
        print("Error, no star counts received!")
        return -1


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
    if msg.topic.endswith("stars"):
        star_counts.append(int(msg.payload))
        print(msg.topic + " " + msg.payload.decode("utf8"))


if __name__ == "__main__":
    mqtt_connect()
    time.sleep(1)
    run()

# Crashes op RPI400/Raspbian:
# Traceback (most recent call last):
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connection.py", line 198, in _new_conn
#     sock = connection.create_connection(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/util/connection.py", line 60, in create_connection
#     for res in socket.getaddrinfo(host, port, family, socket.SOCK_STREAM):
#   File "/usr/lib/python3.9/socket.py", line 953, in getaddrinfo
#     for res in _socket.getaddrinfo(host, port, family, type, proto, flags):
# socket.gaierror: [Errno -3] Temporary failure in name resolution
#
# The above exception was the direct cause of the following exception:
#
# Traceback (most recent call last):
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 787, in urlopen
#     response = self._make_request(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 488, in _make_request
#     raise new_e
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 464, in _make_request
#     self._validate_conn(conn)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 1093, in _validate_conn
#     conn.connect()
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connection.py", line 753, in connect
#     self.sock = sock = self._new_conn()
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connection.py", line 205, in _new_conn
#     raise NameResolutionError(self.host, self, e) from e
# urllib3.exceptions.NameResolutionError: <urllib3.connection.HTTPSConnection object at 0xf65a75f8>: Failed to resolve 'gps.buienradar.nl' ([Errno -3] Temporary failure in name resolution)
#
# The above exception was the direct cause of the following exception:
#
# Traceback (most recent call last):
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/adapters.py", line 644, in send
#     resp = conn.urlopen(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 841, in urlopen
#     retries = retries.increment(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/util/retry.py", line 519, in increment
#     raise MaxRetryError(_pool, url, reason) from reason  # type: ignore[arg-type]
# urllib3.exceptions.MaxRetryError: HTTPSConnectionPool(host='gps.buienradar.nl', port=443): Max retries exceeded with url: /getrr.php?lat=52.110425&lon=5.1434641 (Caused by NameResolutionError("<urllib3.connection.HTTPSConnection object at 0xf65a75f8>: Failed to resolve 'gps.buienradar.nl' ([Errno -3] Temporary failure in name resolution)"))
#
# During handling of the above exception, another exception occurred:
#
# Traceback (most recent call last):
#   File "/home/pi/Projects/gps-controlled-lcd-shutter/arduino/explore/rainsensor/online-sensors.py", line 171, in <module>
#     run()
#   File "/home/pi/Projects/gps-controlled-lcd-shutter/arduino/explore/rainsensor/online-sensors.py", line 53, in run
#     predict_dt, rain_values = retrieve_predictions(BIKO_LATLON)
#   File "/home/pi/Projects/gps-controlled-lcd-shutter/arduino/explore/rainsensor/online-sensors.py", line 89, in retrieve_predictions
#     response = requests.get(url)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/api.py", line 73, in get
#     return request("get", url, params=params, **kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/api.py", line 59, in request
#     return session.request(method=method, url=url, **kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/sessions.py", line 589, in request
#     resp = self.send(prep, **send_kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/sessions.py", line 703, in send
#     r = adapter.send(request, **kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/adapters.py", line 677, in send
#     raise ConnectionError(e, request=request)
# requests.exceptions.ConnectionError: HTTPSConnectionPool(host='gps.buienradar.nl', port=443): Max retries exceeded with url: /getrr.php?lat=52.110425&lon=5.1434641 (Caused by NameResolutionError("<urllib3.connection.HTTPSConnection object at 0xf65a75f8>:
# Failed to resolve 'gps.buienradar.nl' ([Errno -3] Temporary failure in name resolution)"))

# Drukke tijd op buienradar? Need additional backoff loop!
#
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# 2025-10-26 14:44:00.4
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# 2025-10-26 14:49:00.1
# ...
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# 2025-10-27 13:44:00.2
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# 2025-10-27 13:49:00.4
# gmnstation/NL000W/stars 0
# gmnstation/NL000W/stars 0
# 2025-10-27 13:54:00.1
# Traceback (most recent call last):
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connection.py", line 198, in _new_conn
#     sock = connection.create_connection(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/util/connection.py", line 85, in create_connection
#     raise err
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/util/connection.py", line 73, in create_connection
#     sock.connect(sa)
# OSError: [Errno 101] Network is unreachable
#
# The above exception was the direct cause of the following exception:
#
# Traceback (most recent call last):
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 787, in urlopen
#     response = self._make_request(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 488, in _make_request
#     raise new_e
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 464, in _make_request
#     self._validate_conn(conn)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 1093, in _validate_conn
#     conn.connect()
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connection.py", line 753, in connect
#     self.sock = sock = self._new_conn()
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connection.py", line 213, in _new_conn
#     raise NewConnectionError(
# urllib3.exceptions.NewConnectionError: <urllib3.connection.HTTPSConnection object at 0xf5dfb238>: Failed to establish a new connection: [Errno 101] Network is unreachable
#
# The above exception was the direct cause of the following exception:
#
# Traceback (most recent call last):
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/adapters.py", line 644, in send
#     resp = conn.urlopen(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/connectionpool.py", line 841, in urlopen
#     retries = retries.increment(
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/urllib3/util/retry.py", line 519, in increment
#     raise MaxRetryError(_pool, url, reason) from reason  # type: ignore[arg-type]
# urllib3.exceptions.MaxRetryError: HTTPSConnectionPool(host='gps.buienradar.nl', port=443): Max retries exceeded with url: /getrr.php?lat=52.110425&lon=5.1434641 (Caused by NewConnectionError('<urllib3.connection.HTTPSConnection object at 0xf5dfb238>: Failed to establish a new connection: [Errno 101] Network is unreachable'))
#
# During handling of the above exception, another exception occurred:
#
# Traceback (most recent call last):
#   File "/home/pi/Projects/gps-controlled-lcd-shutter/arduino/explore/rainsensor/online-sensors.py", line 171, in <module>
#     run()
#   File "/home/pi/Projects/gps-controlled-lcd-shutter/arduino/explore/rainsensor/online-sensors.py", line 53, in run
#     predict_dt, rain_values = retrieve_predictions(BIKO_LATLON)
#   File "/home/pi/Projects/gps-controlled-lcd-shutter/arduino/explore/rainsensor/online-sensors.py", line 89, in retrieve_predictions
#     response = requests.get(url)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/api.py", line 73, in get
#     return request("get", url, params=params, **kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/api.py", line 59, in request
#     return session.request(method=method, url=url, **kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/sessions.py", line 589, in request
#     resp = self.send(prep, **send_kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/sessions.py", line 703, in send
#     r = adapter.send(request, **kwargs)
#   File "/home/pi/venv/rain/lib/python3.9/site-packages/requests/adapters.py", line 677, in send
#     raise ConnectionError(e, request=request)
# requests.exceptions.ConnectionError: HTTPSConnectionPool(host='gps.buienradar.nl', port=443): Max retries exceeded with url: /getrr.php?lat=52.110425&lon=5.1434641 (Caused by NewConnectionError('<urllib3.connection.HTTPSConnection object at 0xf5dfb238>: Failed to establish a new connection: [Errno 101] Network is unreachable'))
