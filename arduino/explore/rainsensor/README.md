# Overview of rainsensor folder

## Python scripts

#### online-sensors.py

Runs continously on an RPi to gather star counts from MQTT and rain shower
data from buienradar.nl.

#### process_data.py

Processes all sensor data and output from online-sensors.py. Reads all data
from data/raw and writes results to data/clearsky_data_9xx.parquet.

#### model_data.py

This script attempts to build a model from the data in clearsky_data_9xx.parquet
that predicts when it is safe to operate the en9xx station. It writes the model
output to the data/model folder. It also writes a list of critical nights to
data/en915-downloads.txt.

#### en915_list_downloader.py

This script is a wrapper around the en9xx_downloader.exe windows program that
downloads all images from a single night from the Ondrejov file server. It
uses data/en915-downloads.txt to guide the downloads.

#### analyze_old_testrun.py

Superseded by process_data.py

## Streamlit webapp

Run with:

```bash
streamlit run clearsky/app.py 9xx
```
A docker version is available in the docker-clearsky folder.

## Various files

#### rainmonitor/rainmonitor.ino

The Arduino script for gathering and storing data from the Melexis NIR-sensor
and the conductive rainsensor.

#### Operation_manual_REGME_Rain_detector_.pdf

#### wiring-conducting-sensor.txt

#### VW-rainsensor.txt

#### LIN-Basics_for_Beginners-EN.pdf 

#### 'LINTTL3 LIN to TTL Module'