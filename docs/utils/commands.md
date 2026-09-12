#### Find the server IP:PORT:

```bash
sudo ss -ltnp 'sport = :6667'
```

#### Run the server with a maximum of 16 simultaneous descriptors

```bash
bash -c 'ulimit -n 16; exec ./ircserv 6667 secret'
```

`EMFILE` means that process has reached its descriptor limit. It must not be confused with `ENFILE`, which means the whole system has reached its limit.

#### Open 30 simultaneous TCP clients and keep them connected for ~8 minutes.

```bash
python3 -c 'import socket,time; clients=[socket.create_connection(("127.0.0.1",6667)) for connection_number in range(30)]; print(f"{len(clients)} clients connected"); time.sleep(450)'
```
