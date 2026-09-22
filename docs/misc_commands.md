## Helpful commands

### Connect to a tty (VSP) and print every received character as hex

stdbuf -o0 od -An -v -t x1 -w1 /dev/mhuxd/ptt1
