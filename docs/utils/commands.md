#### Encontrar IP:PORT del servidor:

```bash
sudo ss -ltnp 'sport = :6667'
```

#### Ejecuta servidor imponiendo max. 16 descriptores simultáneos

```bash
bash -c 'ulimit -n 16; exec ./ircserv 6667 secret'
```

`EMFILE` significa que ese proceso ha alcanzado su límite de descriptores. No debe confundirse con `ENFILE`, que indica que todo el sistema ha alcanzado su límite.

#### Abre 30 clientes TCP simultáneos y los mantiene conectados ~8 mins.

```bash
python3 -c 'import socket,time; clients=[socket.create_connection(("127.0.0.1",6667)) for connection_number in range(30)]; print(f"{len(clients)} clients connected"); time.sleep(450)'
```
