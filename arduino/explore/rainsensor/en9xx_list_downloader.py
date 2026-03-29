import os
from pathlib import Path
import subprocess


def run():
    os.chdir("data")
    print(os.listdir("..\\..\\..\\..\\dev"))
    with open("en9xx-downloads.txt")  as f:
        downloads = [x.replace("\n", "") for x in f.readlines()]
    for download in downloads:
        print("Start downloading", download)
        station_no, date = download.split(" ")
        if not list((Path(station_no) / date[:4]).glob(f"{date[:10]}*")):
            subprocess.call(["..\\..\\..\\..\\dev\\en9xx_downloader.exe", station_no, date])


if __name__ == "__main__":
    run()