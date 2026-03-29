"""
SSH options python
paramiko  basically run shell commands. Has a listdir:
    https://docs.paramiko.org/en/latest/api/sftp.html
ssh2-python low level
    https://ssh2-python.readthedocs.io/en/stable/sftp_handle.html
    https://stackoverflow.com/questions/75757388/how-to-list-directory-files-in-sftp-using-parallel-ssh
ssh2-parallel
"""
from datetime import datetime, timedelta
import os
from pathlib import Path

from dotenv import load_dotenv
import paramiko

DEFAULT_MAX_EXPOSURE = 63
MAX_TIMEDELTA = timedelta(seconds=os.getenv("EN_MAX_EXPOSURE", DEFAULT_MAX_EXPOSURE))


def get_en_image_path(station_no: str, ut_datetime: datetime, callback=None) -> Path:
    """Gets an EN image from the local disk cache or downloads it from the remote server
    otherwise, using the EN_SERVER and EN_USER env variables.
    Returns the local image path, either:
        data/915/single/2026-01-20_16-45-25.jpg
    or:
        data/915/2025/2025-09-25_18-05-13_00331/img_915_2025-09-25_18-05-19-657_0331-0001-0.jpg
    It raises a RemoteImageException if the ut_datetime is not available at the remote server.
    """
    # ToDo: rather return an object that contains the requested Path but also allows to
    # cancel the operation if remote downloading turns out to be too slow or impossible.
    # asyncio has the right primitives to implement this
    try:
        image_path = _local_image_path(station_no, ut_datetime)
        print(f"Image available from download store at {image_path}")
    except DownloadedImageException:
        try:
            image_path = _single_image_path(station_no, ut_datetime)
            print(f"Image available from single image cache at {image_path}")
        except CachedImageException:
            try:
                image_path = _download_en_image(station_no, ut_datetime, callback)
                print("Image downloaded from remote server")
            except Exception as e:
                raise RemoteImageException("No image evailable for the requested datetime")
    return image_path


def _local_image_path(station_no: str, ut_datetime: datetime) -> Path:
    image_dir = Path("data") / station_no / str(ut_datetime.year)
    for image_path in sorted(image_dir.glob(f"{str(ut_datetime.date())}*/*.jpg")):
        parts = image_path.name.split("_")
        iso_date = parts[2]
        iso_time = parts[3][:8].replace("-", ":")
        stored_datetime = datetime.fromisoformat(f"{iso_date} {iso_time}")
        if timedelta(0) <= ut_datetime - stored_datetime <= MAX_TIMEDELTA:
            return image_path
    raise DownloadedImageException()


def _single_image_path(station_no: str, ut_datetime: datetime) -> Path:
    image_dir = Path("data") / station_no / "single"
    for image_path in sorted(image_dir.glob("*.jpg")):
        print("!!!", image_path)
        iso_date, iso_time = image_path.name[:-4].split("_")
        iso_time = iso_time.replace("-", ":")
        stored_datetime = datetime.fromisoformat(f"{iso_date} {iso_time}")
        if timedelta(0) <= ut_datetime - stored_datetime <= MAX_TIMEDELTA:
            return image_path
    raise CachedImageException()


def _download_en_image(station_no: str, ut_datetime: datetime, callback):
    """Downloads the jpg image for the given UT datetime for the configured EN_USER.
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
    # Remote folders are named according to the date in the evening
    folder_datetime = ut_datetime
    if folder_datetime.hour < 12:
        folder_datetime -= timedelta(days=1)
    year_folder = f"/data/{folder_datetime.year}"
    entry = None  # Causes TypeError below if no valid entry found in the loop
    for entry in sftp.listdir(year_folder):
        if entry.startswith(folder_datetime.date().isoformat()):
            break
    day_folder = os.path.join(year_folder, entry)
    for entry in sftp.listdir(day_folder):
        if not entry.endswith(".jpg"):
            continue
        parts = entry.split("_")
        iso_date = parts[2]
        iso_time = parts[3][:8].replace("-", ":")
        remote_datetime = datetime.fromisoformat(f"{iso_date} {iso_time}")
        if timedelta(0) <= ut_datetime - remote_datetime <= MAX_TIMEDELTA:
            remote_image_path = os.path.join(day_folder, entry)
            local_image_dir = os.path.join("data", station_no, "single")
            local_image_fname = "_".join([parts[2], parts[3][:8] + ".jpg"])
            os.makedirs(local_image_dir, exist_ok=True)
            local_image_path = os.path.join(local_image_dir, local_image_fname)
            print(f"Start downloading from {remote_image_path} to {local_image_path}")
            sftp.get(remote_image_path, local_image_path, callback=callback)
            return local_image_path
    raise RuntimeError("Neither image nor exception; this should not happen!")


class DownloadedImageException(Exception):
    pass


class CachedImageException(Exception):
    pass


class RemoteImageException(Exception):
    pass


if __name__ == "__main__":
    load_dotenv()


    def progress(transferred: int, tobe_transferred: int):
        print(f"Download progress: {(100 * transferred) / tobe_transferred:.1f}%")


    get_en_image_path("915", datetime(2026, 1, 20, 17, 45, 25), progress)
