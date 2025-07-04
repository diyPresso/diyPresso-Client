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

import sys
import platform
import os

if platform.system() == 'Windows':
    import msvcrt
else:
    import termios, tty

class Util:
    @staticmethod
    def exit(exit_code=0, message=""):
        if message != "": print(message)
        print("Press any key to exit...")

        if platform.system() == 'Windows':
            msvcrt.getch()
        else:
            # complex way to wait for a key press
            fd = sys.stdin.fileno()
            old_settings = termios.tcgetattr(fd)
            try:
                tty.setraw(fd)
                sys.stdin.read(1)
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        
        sys.exit(exit_code)

    @staticmethod
    def normalize_path(path):
        path = os.path.normpath(path)
        path = os.path.abspath(path)
        return path