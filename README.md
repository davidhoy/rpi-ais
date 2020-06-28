# rpi-ais
AIS Forwarder for MarineTraffic

This is a very simple AIS forwarder for MarineTraffic.com.  It connects to an AIS transponder that sends NMEA 0183 formatted AIS messages over a TCP port, filters the data stream to remove any non-AIS messages, and forwards the resulting stream to a UDP port on a MarineTraffic server.  

The script will run on any machine that has a Python 3 interpreter, though I run it on a dedicated Raspberry Pi Zero W.  You will need to get your own IP address and port number from MarineTraffic, and modify the appropriate line in the script to use the info they provide for you.  DO NOT use the script unmodified.

I have only tested this with a Vesper XB-8000 AIS transponder, though it should work with any other device that sends NMEA 0183 message over a TCP connection.  It should be pretty simple to modify to work over a UART or USB serial connection.

If you have any problems or questions, please email me at david@thehoys.com.

Regards,
David Hoy
S/V Rising Sun
Catalina 470, hull #17
