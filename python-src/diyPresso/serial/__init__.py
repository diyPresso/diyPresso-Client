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

import serial.tools.list_ports
from time import sleep, time
from datetime import datetime, timedelta
from diyPresso.util import Util
import re

#IDs for identifying the diyPresso device in USB serial interface. 
ARDUINO_VENDOR_ID = 9025
ARDUINO_MKR_WIFI_1010_PRODUCT_ID = 32852          # pid: 32852 in normal operation
ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER = 84  # pid: 84 in bootloader mode
 
TIMEOUT = 5 # seconds (not used consistently)

class Serial:
    def __init__(self, verbose=False):
        
        self.verbose = verbose
        if self.verbose: print(f"Initiating serial class")

        self.connection = None
        self.port = None #will be set by self.set_port()
        self.bootloader_mode = False #will be set by self.set_port()

        self.set_port()

    def get_bootloader_mode(self):
        return self.bootloader_mode
        
    def connect(self, baudrate=115200, timeout=5):
        if self.port is None:
            Util.exit(exit_code=1, message="No port found to connect to.")

        self.connection = serial.Serial(self.port, baudrate, timeout=timeout)
        if self.verbose: print(f"Connected to {self.port} at {baudrate} baudrate")

        if self.connection.isOpen():
            return True
        else:
            return False
        
    def is_connected(self):
        if self.connection is not None:
            return self.connection.isOpen()
        else:
            return False
        
    def close(self):
        if self.connection is not None:
            self.connection.close()
            if self.verbose: print("Connection closed")
            self.connection = None
            self.bootloader_mode = False


    def has_data(self):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")
        
        return self.connection.in_waiting > 0
        
    def readline(self):
        if self.connection is None:
            Util.exit(exit_code=1, message="No connection established when trying to read line.")
        
        line = self.connection.readline()
        return line.decode('utf-8').strip()
    
    def send_line(self, line):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")
        
        self.connection.write(f"{line}\n".encode('utf-8'))


    # setpoint:98.00, power:70.63, average:67.47, act_temp:85.47, boiler-state:heating, boiler-error:OK, brew-state:idle, weight:-1160.40, end_weight:-1159.54, reservoir_level:77.3
    def get_state(self):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")
        
        state = {}
        timeout = 8 # seconds
        sleep_time = 0.1 # seconds
        start_time = time()
        while (time() - start_time) < timeout:
            if self.has_data():
                line = self.readline()
                if self.verbose: print(f"{datetime.now():%H:%M:%S} - Received: {line}")
                
                #ignore lines not starting with "setpoint:"
                if not(line.startswith("setpoint:")): continue

                state = {}

                #the diyPresso retunrs lines like this, lets parse them
                # setpoint:98.00, power:70.63, average:67.47, act_temp:85.47, boiler-state:heating, boiler-error:OK, brew-state:idle, weight:-1160.40, end_weight:-1159.54, reservoir_level:77.3
                # use the regex to match "<variable_name or variable-name>:<value>"
                pattern = r'([\w\-]+):([^,\s]+)'
                matches = re.findall(pattern, line)
                for match in matches:
                    key = match[0]
                    value = match[1]
                    state[key] = value

                # Create a mapping of old keys to new keys
                key_mapping = {
                    "setpoint": "boiler_setpoint_temperature",
                    "power": "boiler_heater_power",
                    "average": "boiler_heater_average_power", # TODO: check what this is
                    "act_temp": "boiler_actual_temperature",
                    "boiler-state": "boiler_state",
                    "boiler-error": "boiler_error",
                    "brew-state": "brew_state",
                    "weight": "reservoir_weight",
                    "end_weight": "reservoir_end_weight", # TODO: check what this is
                    "reservoir_level": "reservoir_water_level" # TODO: check what this is
                }

                # Rename keys based on the mapping
                for old_key, new_key in key_mapping.items():
                    if old_key in state:
                        state[new_key] = state.pop(old_key)
                
                return state

            
            else:
                sleep(sleep_time)
                continue
        
        Util.exit(exit_code=1, message="Error: unable to read diyPresso state data")

        
            

    # get the settings using the GET settings command
    def get_settings(self):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")

        self.send_line("GET settings")

        #read lines unit timeout or "GET settings OK" is received
        settings = {}
        start_time = time()
        success = False
        while (time() - start_time) < TIMEOUT:
            if self.has_data():
                line = self.readline()

                #ignore lines starting with "setpoint:"
                if line.startswith("setpoint:"): continue

                #use the regex to match "<variable_name>=<value>"
                match = re.fullmatch(r'(\w+)=(.*)', line)
                if match:
                    key = line.split("=")[0]
                    value = line.split("=")[1]
                    settings[key] = value

                if line == "GET settings OK":
                    success = True
                    break
            else:
                sleep(0.1)
        
        if not success:
            Util.exit(exit_code=1, message="Error: Timeout while waiting for GET settings response")
        
        return settings


    # get the settings from the boot sequence of the device used for pre 1.6.2 firmware versions
    def get_settings_from_bootsequence(self):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")
        
        pattern=r'(\w+)=([^\s]+)'
        settings = {}
        line = ""
        start_time = time()
        #start monitoring for lines with "<variable_name>=<value>" and stop at the first "setpoint:" line.
        while (time() - start_time) < 10: #wait to boot and timeout after 10 seconds
            line = self.readline()
            if self.verbose: print(f"{datetime.now():%H:%M:%S} - Received: {line}")

            if line.startswith("setpoint:"): break 
            #use the regex to match "<variable_name>=<value>"
            match = re.fullmatch(pattern, line)
            if match:
                key = line.split("=")[0]
                value = line.split("=")[1]
                settings[key] = value
        
        if len(settings) == 0:
            Util.exit(exit_code=1, message="Error: unable to read settings during boot sequence")

        #if line contains "brew-state:idle", machine is commissioned so set commissioningDone=1 if not set already in settings
        if "commissioningDone" not in settings and "brew-state:idle" in line:
            settings["commissioningDone"] = "1"

        return settings

    def send_settings(self, settings):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")
        
        #make a copy of the settings to avoid modifying the original
        settings = settings.copy()

        #remove 'crc' and 'version' if they are present as those can not be set over serial
        if 'crc' in settings: del settings['crc']
        if 'version' in settings: del settings['version']

        if len(settings) == 0:
            return False

        #append the settings in single comma seperated string
        settings_str = ""
        for key, value in settings.items():
            settings_str += f"{key}={value},"
        
        # remove the last comma
        settings_str = settings_str[:-1]

        self.send_line(f"PUT settings {settings_str}")

        #wait for PUT settings OK or PUT settings NOK response
        start_time = time()
        while (time() - start_time) < TIMEOUT:
            if self.has_data():
                line = self.readline()

                if self.verbose: print(f"{datetime.now():%H:%M:%S} - Received: {line}")

                if line.startswith("PUT settings OK"):
                    return True
                elif line.startswith("PUT settings NOK"):
                    return False
            else:
                sleep(0.1)

        print(f"Timeout while waiting for PUT settings response")
        return False
        
        
    
    # use the 1200 baud trick to reset the device into bootloader mode
    def reset_into_bootloader(self):
        if self.is_connected():
            self.close()
        
        self.connect(baudrate=1200)
        sleep(0.1)
        self.close()
        sleep(3)

        # port can be changed (especially on Windows) also updates bootloader mode.
        self.set_port()


    def get_device_info(self):
        if not(self.is_connected()):
            Util.exit(exit_code=1, message="No connection established.")

        self.connection.write(b'GET info\n')

        #read lines unit timeout or "GET info OK" is received
        lines = []
        start_time = time()
        while (time() - start_time) < TIMEOUT:
            if self.has_data():
                line = self.readline()

                if self.verbose: print(f"{datetime.now():%H:%M:%S} - Received: {line}")

                #ignore lines starting with "setpoint:"
                if line.startswith("setpoint:"): continue

                lines.append(line)

                if line == "GET info OK":
                    break
            else:
                sleep(0.1)

        return lines
      
    def get_device_firmware_version(self):
        lines = self.get_device_info()

        version = "pre-1.6.2"

        #look for firmwareVerion=1.6.2-dev
        for line in lines:
            if line.startswith("firmwareVersion="):
                version = line.split("=")[1]
                break
        
        return version

    
    def set_port(self):
        self.port, self.bootloader_mode = Serial.get_port(verbose=self.verbose) #at least call once and if not found check in loop below.

        timeout = 20 # seconds
        start_time = time()
        while self.port is None:
            if (time() - start_time) > timeout:
                Util.exit(exit_code=1, message="Error: Timeout while waiting for device to be detected after reset")

            if self.verbose: print(f"Waiting for device to be detected...")
            sleep(3)
            self.port, self.bootloader_mode = Serial.get_port(verbose=self.verbose)
            
    
    @staticmethod
    def get_port(verbose=False):
        devices = Serial.get_devices()

        # Find the device with vendor id ('vid'): 9025 and product id ('pid'): 32852 in normal operation. 'pid': 84 in bootloader mode
        found_devices = [device for device in devices if device.vid == ARDUINO_VENDOR_ID and
                         (device.pid == ARDUINO_MKR_WIFI_1010_PRODUCT_ID or device.pid == ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER)]
        
        port = None
        bootloader_mode = False

        if len(found_devices) == 0:
            if verbose: print(f"diyPresso not found")
            pass
        elif len(found_devices) > 1:
            Util.exit(exit_code=1, message=f"Error: multiple devices found")
        else:
            port = found_devices[0].device
            if verbose: print(f"diyPresso found on: {port}")

            if (found_devices[0].pid == ARDUINO_MKR_WIFI_1010_PRODUCT_ID_BOOTLOADER):
                bootloader_mode = True
            else:
                bootloader_mode = False
            
            if verbose: print(f"In bootloader mode: {bootloader_mode}")
        
        return port, bootloader_mode


    @staticmethod
    def get_devices():
        devices = serial.tools.list_ports.comports()

        return devices
    
        