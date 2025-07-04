# This file is part of the diyPresso management client software.
#
# Copyright (C) 2025 diyPresso and authors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

import os
import sys
import subprocess
from diyPresso.util import Util
from time import sleep

DEFAULT_FIRMWARE_PATH = "firmware.bin"
DEFAULT_DEV_FIRMWARE_PATH = "../../bin/firmware/firmware.bin"

DEFAULT_BOSSAC_PATH_MACOS = "bossac"
DEFAULT_DEV_BOSSAC_PATH_MACOS = "../../bin/bossac/bossac"
DEFAULT_BOSSAC_PATH_WIN = "bossac.exe"
DEFAULT_DEV_BOSSAC_PATH_WIN = r"..\..\bin\bossac\bossac.exe"

try:
    __compiled__
except NameError:
    __compiled__ = False

class FirmwareUploader:
    def __init__(self, verbose=False):
        self.verbose = verbose

    def upload_firmware(self, serial, firmware_path="", bossac_path=""):

        if firmware_path is None or firmware_path == "":
            firmware_path = FirmwareUploader.get_firmware_path()

        if bossac_path is None or bossac_path == "":
             bossac_path = FirmwareUploader.get_bossac_path()

        if self.verbose: print(f"Firmware path: {firmware_path}")
        if self.verbose: print(f"Bossac path: {bossac_path}")
        
        if serial.get_bootloader_mode() == False:
            Exception("Error: Bootloader mode not triggered, trigger the bootloader mode first.")

        #print current path
        # if self.verbose: print(f"current path: {os.getcwd()}")

        # bossac --info --port "cu.usbmodem11301" --write --verify --reset --erase -U true .pio/build/mkr_wifi1010/firmware.bin
        command = [
            bossac_path, "--info", "--port", serial.port, "--write", "--verify", "--reset", "--erase", "-U", "true", firmware_path
        ]

        if self.verbose: print(f"Running command: {' '.join(command)}")

        res = subprocess.run(command, capture_output=True, text=True)

        if res.returncode == 0:
            print("Firmware upload successful! Restarting diyPresso.")
        else:
            print("Firmware upload failed!")
            print(res.stderr)
            Util.exit(exit_code=1)

    @staticmethod
    def get_firmware_path():
        # Get the directory of the current script
        script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))

        if __compiled__: # Path for the compiled version
            firmware_path = os.path.join(script_dir, DEFAULT_FIRMWARE_PATH)
        else:            # Path for the development version
            firmware_path = os.path.join(script_dir, DEFAULT_DEV_FIRMWARE_PATH)

        firmware_path = os.path.normpath(firmware_path)

        #check if the file exists
        if not os.path.exists(firmware_path):
            Util.exit(exit_code=1, message=f"Error: Firmware file not found at {firmware_path}")  

        return firmware_path
    
    @staticmethod
    def get_bossac_path():
        script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))

        if   __compiled__ and os.name == 'nt': # Windows
                bossac_path = os.path.join(script_dir, DEFAULT_BOSSAC_PATH_WIN)
        elif __compiled__ and  os.name == 'posix': # macOS or other Unix-like systems
                bossac_path = os.path.join(script_dir, DEFAULT_BOSSAC_PATH_MACOS)
        elif not __compiled__ and os.name == 'nt': # Windows
                bossac_path = os.path.join(script_dir, DEFAULT_DEV_BOSSAC_PATH_WIN)
        elif not __compiled__ and os.name == 'posix': # macOS or other Unix-like systems
                bossac_path = os.path.join(script_dir, DEFAULT_DEV_BOSSAC_PATH_MACOS)

        bossac_path = os.path.normpath(bossac_path)

        #check if the file exists
        if not os.path.exists(bossac_path):
            Util.exit(exit_code=1, message=f"Error: Bossac file not found at {bossac_path}")

        return os.path.normpath(bossac_path)
           

