import os
import subprocess


def run():
    os.chdir("data")
    print(os.listdir("..\\..\\..\\..\\dev"))
    with open("en9xx-downloads.txt")  as f:
        downloads = [x.replace("\n", "") for x in f.readlines()]
    for download in downloads:
        print("Start downloading", download)
        subprocess.call(["..\\..\\..\\..\\dev\\en9xx_downloader.exe", *download.split(" ")])


if __name__ == "__main__":
    run()