"""
SSH options python
paramiko  basically run shell commands. Has a listdir:
    https://docs.paramiko.org/en/latest/api/sftp.html
ssh2-python low level
    https://ssh2-python.readthedocs.io/en/stable/sftp_handle.html
    https://stackoverflow.com/questions/75757388/how-to-list-directory-files-in-sftp-using-parallel-ssh
ssh2-parallel
"""
from datetime import date, datetime, timedelta
from stat import S_ISDIR, S_ISREG
import os
from pathlib import Path

from dotenv import load_dotenv
import paramiko

DEFAULT_MAX_EXPOSURE = 63
MAX_TIMEDELTA = timedelta(seconds=os.getenv("EN_MAX_EXPOSURE", DEFAULT_MAX_EXPOSURE))


def get_en_image_path(ut_datetime: datetime, callback=None) -> Path:
    """Gets an EN image from the local disk cache or downloads it from the remote server
    otherwise, using the EN_SERVER, EN_STATION, EN_USER and EN_CACHE_DIR env variables.
    Returns the local image path, e.g.:
        {EN_CACHE_DIR}/2026/2026-01-20/{EN_STATION}_2026-01-20_16-45-25.jpg
    It raises a RemoteImageException if the ut_datetime is not available at the remote server.
    """
    try:
        image_path = _local_image_path(ut_datetime)
        print(f"Image available from local cache at {image_path}")
    except CachedImageException:
        image_path = _download_en_image(ut_datetime, callback)
        print("Image downloaded from remote server")
    return image_path


def _download_en_image(ut_datetime: datetime, callback):
    """Downloads the jpg image for the given UT datetime for the configured EN_STATION and EN_USER.
    Example directory on the EN_SERVER:
        /data/2026/2026-01-20_16-45-25_00438/
    Example filename in this directory:
        img_915_2026-01-20_16-46-35-650_0438-0002-0.jpg
    """
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(
        os.getenv("EN_SERVER"),
        username=os.getenv("EN_USER"),
        key_filename=os.getenv("EN_PRIVATE_KEY")
    )
    sftp = ssh.open_sftp()
    remote_year_dir = f"/data/{ut_datetime.year}"
    entry = None  # Causes TypeError below if no valid entry found in the loop
    for entry in sftp.listdir(remote_year_dir):
        if entry.startswith(ut_datetime.date().isoformat()):
            break
    remote_day_dir = os.path.join(remote_year_dir, entry)
    for entry in sftp.listdir(remote_day_dir):
        if not entry.endswith(".jpg"):
            continue
        parts = entry.split("_")
        iso_date = parts[2]
        iso_time = parts[3][:8].replace("-", ":")
        remote_datetime = datetime.fromisoformat(f"{iso_date} {iso_time}")
        if timedelta(0) <= ut_datetime - remote_datetime <= MAX_TIMEDELTA:
            remote_image_path = os.path.join(remote_day_dir, entry)
            local_image_dir = os.path.join(
                os.getenv("EN_CACHE_DIR"), str(ut_datetime.year), str(ut_datetime.date().isoformat()))
            local_image_fname = "_".join([os.getenv("EN_STATION"), parts[2], parts[3][:8] + ".jpg"])
            os.makedirs(local_image_dir)
            local_image_path = os.path.join(local_image_dir, local_image_fname)
            print(f"Start downloading from {remote_image_path} to {local_image_path}")
            sftp.get(remote_image_path, local_image_path, callback=callback)
            return local_image_path
    raise RemoteImageException()


def _local_image_path(ut_datetime: datetime) -> Path:
    image_dir = Path(os.getenv("EN_CACHE_DIR")) / str(ut_datetime.year) / str(ut_datetime.date())
    for image_path in sorted(image_dir.glob("*.jpg")):
        iso_date = str(image_path)[-23:-13]
        iso_time = str(image_path)[-12:-4].replace("-", ":")
        stored_datetime = datetime.fromisoformat(f"{iso_date} {iso_time}")
        if timedelta(0) <= ut_datetime - stored_datetime <= MAX_TIMEDELTA:
            return image_path
    raise CachedImageException()


def progress(transferred: int, tobe_transferred: int):
    print(f"Download progress: {(100 * transferred) / tobe_transferred:.1f}%")


class CachedImageException(Exception):
    pass


class RemoteImageException(Exception):
    pass


if __name__ == "__main__":
    load_dotenv()
    get_en_image_path(datetime(2026, 1, 20, 17, 45, 25), progress)
