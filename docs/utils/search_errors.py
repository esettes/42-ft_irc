import socket

for connection_number in range(1000):
    connection_socket = socket.create_connection(
        ("127.0.0.1", 6667)
    )
    connection_socket.close()
