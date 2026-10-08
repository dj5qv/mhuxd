### Helpful commands

### Connect to a tty (VSP) and print every received character as hex

stdbuf -o0 od -An -v -t x1 -w1 /dev/mhuxd/ptt1


### Connect to a tty (VSP) mapped to a PTT channel and send 0x01 byte to toggle PTT on.
# Sleep for 2 seconds. Releasing the port will automaticall toggle PTT off.

python3 -c "import serial; import time; s=serial.Serial('/dev/mhuxd/ptt1_2', 9600); s.write(b'\x01'); time.sleep(2);"

### Same for a TCP port:
python3 -c "import socket; import time; s=socket.create_connection(('localhost', 9001)); s.sendall(b'\x01'); time.sleep(2)"

