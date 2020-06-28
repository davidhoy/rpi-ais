import socket
import sys

# Create a TCP/IP socket
ais_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect the socket to the port where the server is listening
ais_address = ('192.168.1.24', 39150)
print('Connecting to AIS at %s port %s' % ais_address)
ais_sock.connect(ais_address)
ais_file = ais_sock.makefile()

mt_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
mt_address = ('5.9.207.224', 10170)
print('Connecting to MarineTraffic at %s port %s' % mt_address)

try:
    while True:
        str = ais_file.readline()
        if str.startswith( '!AIVDM') or str.startswith( '!AIVDO'):
            print(str, end="")
            data = bytes(str, 'utf-8')
            mt_sock.sendto(data, mt_address)

finally:
    print('Closing sockets')
    ais_sock.close()
