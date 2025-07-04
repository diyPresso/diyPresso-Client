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
#
# ##############################
#
#  excecute ./build.sh in the project root to build packages (using git bash on Windows)
# 
# Nuitka compilation parameters:
#   nuitka-project: --product-name="diyPresso Client"
#   nuitka-project: --file-version=0.1.0
#   nuitka-project: --output-filename="diypresso"
#   nuitka-project: --standalone
#   nuitka-project: --onefile
#   nuitka-project: --follow-imports
#   nuitka-project: --noinclude-default-mode=error
#   nuitka-project: --assume-yes-for-downloads
#   nuitka-project: --lto=yes
#   
#   nuitka-project-if: {OS} in ("Windows"):
#      nuitka-project:  --msvc=latest
#
#   nuitka-project-if: {OS} in ("Darwin"):
#      nuitka-project: --macos-sign-notarization
#      nuitka-project: --macos-sign-identity="Developer ID Application: diyPresso B.V. (V3QR5FV6B7)"
#      #uitka-project: --macos-create-app-bundle


# restore old firmware for testing:
#   python ./src/main.py -v -u -p "./src/firmware-1.6.0-dev.bin"
#   python ./src/main.py -v --force-upload-firmware -b "./bin/firmware/firmware-1.6.0-dev.bin"
#
# restore machine stuck in bootloader mode:
#   python ./src/main.py -v --force-upload-firmware -b "./bin/firmware/firmware.bin"
#   python3 ./src/main.py -v -r --settings-file ./settings_20250131_092628.json


# TODO:
# Nice to have:
# - proper JSON format for settings with correct data types
# - option to save settings to a file with --get-settings
# - option to opt-out of saving settings to a file
# - Get info to print JSON
# - Detect *.bin files in the same directory as the script and ask user to select one
# - multiple actions in one command? e.g. update settings and plot?
# - refactor argument handling
#
# critical:
# - None :)

VERSION = "0.9.1"

from os.path import dirname, abspath, join
import sys

# Add the direcrtory with the diyPresso modules to the path
FILE_DIR = dirname(__file__)
MODULE_DIR = abspath(join(FILE_DIR, '..'))
sys.path.append(MODULE_DIR)

from diyPresso.serial import Serial
from diyPresso.firmwareUploader import FirmwareUploader
from time import sleep, time
from diyPresso.util import Util
from datetime import datetime
import json

global VERBOSE, BAUDRATE

try:
    __compiled__ # type: ignore
except NameError:
    __compiled__ = False

VERBOSE = False
BAUDRATE = 115200
FIRMWARE_PATH = ""
BOSSAC_PATH = ""
SETTINGS_PATH = ""

def handle_arguments():
    global VERBOSE
    action = 'SHOW_HELP' # default action

    if VERBOSE: print(sys.argv) #only verbose if the default above is set to True
    args = sys.argv

    if '-v' in args or '--verbose' in args:
        VERBOSE = True
        if '-v' in args: args.remove('-v')
        if '--verbose' in args: args.remove('--verbose')

    # -p [file] or --binary-file [file] to specify the path to the firmware binary
    if '-b' in args or '--binary-file' in args:
        global FIRMWARE_PATH
        index = args.index('-b') if '-b' in args else args.index('--binary-file')
        FIRMWARE_PATH = args[index + 1].strip('\"')
        
        if '-b' in args: args.remove('-b')
        if '--binary-file' in args: args.remove('--binary-file')
        args.remove(FIRMWARE_PATH)

        FIRMWARE_PATH = Util.normalize_path(FIRMWARE_PATH)

    if '--bossac-file' in args:
        global BOSSAC_PATH
        index = args.index('--bossac-file')
        BOSSAC_PATH = args[index + 1]

        args.remove('--bossac-file')
        args.remove(BOSSAC_PATH)

        BOSSAC_PATH = Util.normalize_path(BOSSAC_PATH)

    if '--settings-file' in args:
        global SETTINGS_PATH
        index = args.index('--settings-file')
        SETTINGS_PATH = args[index + 1]

        args.remove('--settings-file')
        args.remove(SETTINGS_PATH)

        SETTINGS_PATH = Util.normalize_path(SETTINGS_PATH)

    # get the action, can only be one
    if '-h' in args or '--help' in args:
        action = 'SHOW_HELP'
        if '-h' in args: args.remove('-h')
        if '--help' in args: args.remove('--help')

    elif '-l' in args or '--list-devices' in args:
        action = 'LIST_DEVICES'
        if '-l' in args: args.remove('-l')
        if '--list-devices' in args: args.remove('--list-devices')
    
    elif '--get-port' in args:
        action = 'GET_PORT'
        if '--get-port' in args: args.remove('--get-port')
    
    elif '--monitor' in args:
        action = 'MONITOR'
        if '--monitor' in args: args.remove('--monitor')

    elif '-s' in args or '--get-settings' in args:
        action = 'GET_SETTINGS'
        if '-s' in args: args.remove('-s')
        if '--get-settings' in args: args.remove('--get-settings')

    elif '-r' in args or '--restore-settings' in args:
        action = 'RESTORE_SETTINGS'
        if '-r' in args: args.remove('-r')
        if '--restore-settings' in args: args.remove('--restore-settings')
    
    elif '--bootloader' in args:
        action = 'BOOTLOADER'
        if '--bootloader' in args: args.remove('--bootloader') 

    elif '-u' in args or '--upload-firmware' in args:
        action = 'UPLOAD_FIRMWARE'
        if '-u' in args: args.remove('-u')
        if '--upload-firmware' in args: args.remove('--upload-firmware')

    elif '--force-upload-firmware' in args:
        action = 'UPLOAD_FIRMWARE_FORCED'
        if '--force-upload-firmware' in args: args.remove('--force-upload-firmware')

    elif '-i' in args or '--get-info' in args:
        action = 'GET_INFO'
        if '-i' in args: args.remove('-i')
        if '--get-info' in args: args.remove('--get-info')

    if len(args) > 1:
        if args[1] != '':
            print("Invalid arguments or multiple actions specified")
            print("Use diypresso -h for help")
            Util.exit(exit_code=1)
            if VERBOSE: print(args[1])

    return action

def print_help_text():
    help_text = f"""
Management client for the controller of diyPresso espresso machines. Version: {VERSION}

Usage:
    diypresso <action> [options]

Actions:
    -h, --help                            Show this help message and exit
    -l, --list-devices                    List all connected serial devices
        --get-port                        Get the port of the connected diyPresso
    -i, --get-info                        Print device info from the diyPresso machine
        --monitor                         Monitor the serial output from the diyPresso
    -s, --get-settings                    Print the settings from the diyPresso
    -r, --restore-settings                Restore the settings to the diyPresso
        --bootloader                      Reset the diyPresso controller into bootloader mode. Note: this will revome the current settings and firmware.
    -u, --upload-firmware                 Upload firmware to the diyPresso controller
        --force-upload-firmware           Upload the firmware without trying to restore the settings or commision state.

Options:
    -b, --binary-file "path/to/file.bin"  Specify the path to the firmware binary
        --bossac-file "path/to/bossac"    Specify the path to the bossac tool
        --settings-file "./settings.json" Specify the path to the settings file
    -v, --verbose                        Enable verbose mode


Examples:
    diypresso -l
    diypresso -u
    diypresso --get-info
    diypresso --monitor
    diypresso -u -b "../firmware.bin"
    diypresso --force-upload-firmware -b "../firmware.bin"

To update with the diyPresso machine with the supplied firmware.bin file in the same dir as the excecutable, use: diypresso -u 
    """
    print(help_text)

def print_bootloadermode_error():
    print("""
The diyPresso is in bootloader mode. The requested action requires the device to be in normal mode. Restart the device to try to switch to normal operation.
Alternatively, use the --force-upload-firmware action to upload (new) firmware.
""")


def main():
    global VERBOSE, BAUDRATE, FIRMWARE_PATH, BOSSAC_PATH, SETTINGS_PATH
    dp_serial = None

    print(f"diyPresso management client v{VERSION}")

    action =  handle_arguments()

    if VERBOSE: print(f"Running in compiled mode: {__compiled__}")
    if VERBOSE: print(f"Verbose: {VERBOSE}")
    if VERBOSE: print(f"Baudrate: {BAUDRATE}")
    if VERBOSE: print(f"Firmware file: {FirmwareUploader.get_firmware_path()}")
    if VERBOSE: print(f"Bossac file: {FirmwareUploader.get_bossac_path()}")
    if VERBOSE: print(f"Settings file: {SETTINGS_PATH}")
    
    if VERBOSE: print(f"Action: {action}")

    if action == 'SHOW_HELP':
        print_help_text()
        Util.exit()

    if action == 'LIST_DEVICES':
        devices = Serial.get_devices()

        if len(devices) == 0:
            print("No devices found")

        for device in devices:
            print(device.__dict__)
    
    # initiate serial class and find device port for actions that require it
    if action in ['GET_PORT', 'MONITOR', 'BOOTLOADER', 'GET_INFO', 'GET_SETTINGS', 'RESTORE_SETTINGS']:
        dp_serial = Serial(verbose=VERBOSE)
        print(f"Port: {dp_serial.port}, bootloader mode: {dp_serial.get_bootloader_mode()}")

    # actions not supported in bootloader mode
    if action in['MONITOR', 'GET_INFO', 'GET_SETTINGS', 'RESTORE_SETTINGS']:
        if dp_serial.get_bootloader_mode():
            print_bootloadermode_error()
            Util.exit(1)

    # connect to the serial port
    if action in['MONITOR', 'GET_INFO', 'GET_SETTINGS']:
        res = dp_serial.connect(BAUDRATE)

        if not res:
            Util.exit(exit_code=1, message="Failed to connect to the diyPresso")
        else:
            print("Connected to the diyPresso")

    # monitor and print to the console
    if action in ['MONITOR']:
        while True:
            if dp_serial.has_data(): print(dp_serial.readline())
            if not(dp_serial.has_data()): sleep(0.05)

    # get info from the device
    if action in ['GET_INFO']:
        print("Getting device info:")
        print("")

        lines = dp_serial.get_device_info()

        if len(lines) == 0:
            print("The current frimware version on the diyPresso does not to supoort this feature")

        for line in lines:
            print(line)

        print("")
        print(f"Detected firmware version: {dp_serial.get_device_firmware_version()}")
        Util.exit(0)
    
    # reset the device into bootloader mode
    if action in ['BOOTLOADER']:
        #ask for confirmation
        print("This will reset the diyPresso into bootloader mode. This will remove the current settings and firmware, and will render the machine inoperable until new firmware is uploaded with --force-upload-firmware.")
        print("Do you want to continue? (y/n)")
        answer = input()
        if answer.lower() != 'y':
            Util.exit()

        dp_serial.reset_into_bootloader()
        print("Device reset into bootloader mode")
        Util.exit()
    
    settings = []
    #### UPLOAD and GET / RESTORE SETTINGS ####
    if action in ['UPLOAD_FIRMWARE', 'GET_SETTINGS']:
        # check if device is connected: if not ask to connect and wait for it be available over serial
        port, bootloader_mode = Serial.get_port(verbose=VERBOSE)

        #Mode 1: get the settings during boot sequence of the device (mostly for pre-1.6.2 firmware versions)
        if port is None:
            print("")
            print("Looking for diyPresso...")
            print("Please connect the diyPresso to an USB port now (or press ctrl-c to exit).")

            # poll for the device to be connected
            timeout = 30 # seconds
            start_time = time()
            while (time() - start_time) < timeout:
                port, bootloader_mode = Serial.get_port(verbose=VERBOSE)
                if port is not None: break
                sleep(0.1)

            if port is None:
                print(f"diyPresso not found after {timeout} seconds. Ensure the diyPresso is POWERED OFF and connect it via USB after staring this script.")
                Util.exit(exit_code=1)

            #device is connected, initiate the serial class
            dp_serial = Serial(verbose=VERBOSE)

            if dp_serial.get_bootloader_mode():
                print_bootloadermode_error()
                Util.exit(1)

            print("")
            print("Retrieving current settings...")
            dp_serial.connect(BAUDRATE)
            settings  = dp_serial.get_settings_from_bootsequence()

        #Mode 2: get the settings with GET settings for post 1.6.2 firmware versions
        else:
            dp_serial = Serial(verbose=VERBOSE)

            if dp_serial.get_bootloader_mode():
                print_bootloadermode_error()
                Util.exit(1)

            dp_serial.connect(BAUDRATE)
            
            device_firmware_version = ""

            device_firmware_version = dp_serial.get_device_firmware_version()
            print(f"Detected firmware version: {device_firmware_version}")

            # pre-1.6.2 requires the device to boot while connected to the serial port
            if device_firmware_version == "pre-1.6.2":
                print("")
                print("Please power off and disconnect the diyPresso machine from the USB port before starting an upload for machines with pre-1.6.2 firmware versions.")
                Util.exit()

            settings = dp_serial.get_settings()

        if VERBOSE or action == 'GET_SETTINGS': 
            print("Settings retrieved:")
            print(json.dumps(settings, indent=4))

    if action in ['UPLOAD_FIRMWARE']:
        # save settings to json file
        SETTINGS_PATH = f"settings_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"
        with open(SETTINGS_PATH, 'w') as file:
            json.dump(settings, file, indent=4)

        print(f"Settings retrieved and saved to {SETTINGS_PATH}")

        # exit the script if fewer than 12 settings are found
        if len(settings) < 12:
            Util.exit(exit_code=1, message="Error: settings not retrieved correctly")

    # UPLOAD_FIRMWARE_FORCED skips the settings restore
    if action in ['UPLOAD_FIRMWARE', 'UPLOAD_FIRMWARE_FORCED']:

        # for UPLOAD_FIRMWARE_FORCED
        if dp_serial is None: dp_serial = Serial(verbose=VERBOSE)
        
        # reset the device into bootloader mode
        dp_serial.reset_into_bootloader()
        print("Device reset into bootloader mode")

        # upload firmware
        uploader = FirmwareUploader(verbose=VERBOSE)
        uploader.upload_firmware(dp_serial, FIRMWARE_PATH, BOSSAC_PATH)

    if action in ['UPLOAD_FIRMWARE']:
        print("waiting for the device to reboot...")
        sleep(5)

        # port can be changed after uploading the firmware, especially on Windows
        dp_serial.set_port()
        
    

    # load settings from file
    if action in ['RESTORE_SETTINGS']:
        if SETTINGS_PATH == "":
            print("Error: no settings file specified")
            Util.exit(exit_code=1)

        with open(SETTINGS_PATH, 'r') as file:
            settings = json.load(file)

    if action in ['UPLOAD_FIRMWARE', 'RESTORE_SETTINGS']:
        print("")
        print("restoring settings...")
        dp_serial.connect(BAUDRATE)
        res = dp_serial.send_settings(settings)
        if not(res):
            print(f"Failed to restore settings. A backup of the settings has been stored as {SETTINGS_PATH}")
            Util.exit(exit_code=1)
        else:
            print("Settings restored")
            print("")

    Util.exit()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)