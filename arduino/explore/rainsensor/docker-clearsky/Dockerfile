# If you already have docker engine or docker desktop installed, you can replace "podman"
# by "docker" in all commands below. Otherwise, install podman desktop from:
# https://podman-desktop.io/docs/installation
#
# Start podman desktop and leave it alone
# Start a CMD terminal
#
# Without cloned git repository
# podman build --tag clearsky --no-cache https://github.com/hadoopmarc/gps-controlled-lcd-shutter/tree/rainsensor/arduino/explore/rainsensor/docker-clearsky
#
# With cloning the git repository
# git clone https://github.com/hadoopmarc/gps-controlled-lcd-shutter.git
# git checkout rainsensor
# cd gps-controlled-lcd-shutter\arduino\explore\rainsensor
# podman build --tag clearsky --no-cache docker-clearsky
#
# podman image list
# podman run -p 8501:8501 clearsky

FROM python:3.13-slim

WORKDIR /app

RUN apt-get update && apt-get install -y \
    build-essential \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/hadoopmarc/gps-controlled-lcd-shutter.git . && git checkout rainsensor

WORKDIR /app/arduino/explore/rainsensor

RUN pwd && ls -la

RUN  pip3 install -r streamlit/requirements.txt

EXPOSE 8501

HEALTHCHECK CMD curl --fail http://localhost:8501/_stcore/health

ENTRYPOINT ["streamlit", "run", "streamlit/app.py", "--server.port=8501", "--server.address=0.0.0.0"]
