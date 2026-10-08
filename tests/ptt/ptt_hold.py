# Connect to a TCP PTT connector, send 0x01 to trigger PTT, wait for given amount of seconds

import select, socket, sys, time

def hold_ptt(seconds, host='localhost', port=9001):
    s = socket.create_connection((host, port))
    s.sendall(b'\x01')
    deadline = time.monotonic() + seconds
    try:
        while (left := deadline - time.monotonic()) > 0:
            if not select.select([s], [], [], left)[0]:
                break                       # hold time over, connection still alive
            if not s.recv(1024):            # mhuxd closed the connection
                sys.exit("connection closed by mhuxd (rejected / maxcon reached?)")
    except ConnectionResetError:
        sys.exit("connection reset by mhuxd (rejected / maxcon reached?)")
    finally:
        s.close()

if __name__ == '__main__':
    hold_ptt(float(sys.argv[1]), port=int(sys.argv[2]) if len(sys.argv) > 2 else 9001)
