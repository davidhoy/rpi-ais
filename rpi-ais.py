# rpi-ais.py: Simple AIS forwarder for MarineTraffic.com
#
# Should run on an machine with a Python 3 interpreter, but
# tested extensively on a Raspberry Pi Zero W.
#
# This assumes that you have already configured you Pi
# to connect to your local WiFi network.

import socket
import sys

# Create a TCP socket, and connect to AIS data stream
ais_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ais_address = ('192.168.50.37', 39150)
print('Connecting to AIS at %s port %s' % ais_address)
ais_sock.connect(ais_address)
ais_file = ais_sock.makefile()

# Create socket for UDP connection to MarineTraffic.com server
# MT will assign you an IP address and port number when you
# register your station with them.
mt_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
mt_address = ('5.9.207.224', 10170)
print('Connecting to MarineTraffic at %s port %s' % mt_address)

# Loop forever
try:
    while True:
        # Read a NMEA 0183 formatted string from the AIS transponder
        str = ais_file.readline()
        # Filter out any messages that do not start with !AIVDM or !AIVDO
        if str.startswith( '!AIVDM') or str.startswith( '!AIVDO'):
            # Echo to console, and send the string to the UDP port
            print(str, end="")
            data = bytes(str, 'utf-8')
            mt_sock.sendto(data, mt_address)

finally:
    print('Closing sockets')
    ais_sock.close()
    mt_sock.close()
