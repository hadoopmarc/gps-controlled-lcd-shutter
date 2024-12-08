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

from datetime import datetime, timedelta
import time

import requests

SHOWER_URL = "https://gps.buienradar.nl/getrr.php"
NVALUE = 24  # the number of lines returned by the buienradar API
BIKO_LATLON = "lat=52.110425&lon=5.1434641"  # 3573BH 298 backyard
BIKO_CSV = "predictions-biko.csv"
# KNMI_LATLON = "lat=52.09957&lon=5.176514"  # De Bilt official measuring station
# KNMI_CSV = "predictions-knmi.csv"


def run():
    with open(BIKO_CSV, "w") as f:
        time_headers = ','.join([f't{i}' for i in range(24)])
        print(f"datetime,{time_headers}", file=f)
    # with open(KNMI_CSV, "w") as f:
    #     print(f"datetime,{time_headers}", file=f)
    while True:
        wait_one_minute_before()
        predict_dt, rain_values = retrieve_predictions(BIKO_LATLON)
        with open(BIKO_CSV, "a") as f:
            values = ','.join([f'{v:.1f}' if v >= 0. else '' for v in rain_values])
            print(f"{str(predict_dt)[:16]},{values}", file=f)
        # predict_dt, rain_values = retrieve_predictions(KNMI_LATLON)
        # with open(KNMI_CSV, "a") as f:
        #     values = ','.join([f'{v:.1f}' for v in rain_values])
        #     print(f"{str(predict_dt)[:16]},{values}", file=f)
        time.sleep(60)


def wait_one_minute_before():
    current_dt = datetime.now()
    remainder = current_dt.timestamp() % 300
    if remainder > 240:
        remainder -= 300
    print(current_dt, 240 - remainder)
    time.sleep(240 - remainder)


def retrieve_predictions(latlon):
    # Generate expected prediction times in hh:mm format
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
    return predict_dt, rain_rates


if __name__ == "__main__":
    run()
