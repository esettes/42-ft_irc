
import socket
import struct

connection_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
connection_socket.connect(("127.0.0.1", 6667))

linger_configuration = struct.pack("ii", 1, 0)
connection_socket.setsockopt(
    socket.SOL_SOCKET,
    socket.SO_LINGER,
    linger_configuration
)

connection_socket.close()

