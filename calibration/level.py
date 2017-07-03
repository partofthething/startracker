"""
Digital Level Reader.

Reads DXL360 digital level measurements coming in over the serial port.

Data comes in via UART, converted to USB with a FT232H or equiv.
Format is X+0000Y+0000

Original Purpose: Calibrate/verify star-tracking barn-door camera mount.
By: Nick Touran
"""
import serial
import re
import time


DEV = '/dev/ttyUSB0'
BAUD = 9600
MSG_LENGTH = 12


def read():
    with serial.Serial(DEV, BAUD, timeout=1) as port:
        # sync up when you find an 'X' character
        char = b''
        while char != b'X':
            char = port.read()
        port.read(MSG_LENGTH - 1)  # toss one datum since X was consumed
        start = time.time()
        seconds = 0.0
        # now read continuously and process into floats
        while True:
            datum = port.read(MSG_LENGTH).decode()
            match = re.search('X([+-]\d\d\d\d)Y([+-]\d\d\d\d)', datum)
            if not match:
                raise ValueError('Level did not provide valid data. Check connection.')
            x, y = match.groups()
            x = int(x) * 0.01
            y = int(y) * 0.01
            seconds = time.time() - start
            print('{:<10.1f} {: 6.2f} {: 6.2f}'.format(seconds, x, y))
            yield (seconds, x, y)

if __name__ == '__main__':
    read()

