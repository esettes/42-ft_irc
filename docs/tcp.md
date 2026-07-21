El servidor IRC seguirá aproximadamente este proceso:

```text
socket()
   ↓
bind()
   ↓
listen()
   ↓
poll()
   ↓
accept()
   ↓
recv() / send()
   ↓
close()
```

`socket()`

Crea el socket principal del servidor:

```cpp
socket(AF_INET, SOCK_STREAM, 0);
```

- `AF_INET`: usar direcciones IPv4.
- `SOCK_STREAM`: utilizar TCP.
- `0`: seleccionar automáticamente el protocolo correspondiente.

`bind()`

Asocia el socket a una dirección y un puerto:

`0.0.0.0:6667`

Es como decir:

Este programa será responsable de las conexiones que lleguen al puerto 6667.

`listen()`

Coloca el socket en modo escucha. Desde ese momento puede recibir solicitudes de conexión.

`accept()`

Acepta una conexión pendiente.

Muy importante: `accept()` crea un socket nuevo para ese cliente.

```text
Socket servidor
    │
    ├── socket del cliente 1
    ├── socket del cliente 2
    └── socket del cliente 3
```

El socket principal sigue escuchando. Los sockets creados por `accept()` se usan para hablar con cada cliente.

`recv()`

Recibe bytes enviados por un cliente.

Los datos pueden representar comandos IRC completos, varios comandos o solamente una parte.

`send()`

Envía bytes al cliente.

Por ejemplo:

`:server 001 roxana :Welcome to the IRC server\r\n`

`close()`

Cierra un socket cuando el cliente se desconecta o se produce un error.

---

Para TCP todo son bytes:

`4e 49 43 4b 20 72 6f 78 61 6e 61 0d 0a`

Es tu servidor quien interpreta esos bytes como:

`NICK roxana\r\n`

Tampoco cifra la información. Una contraseña enviada mediante TCP normal no está cifrada; para eso haría falta TLS.

---

TCP crea un flujo fiable y ordenado de bytes entre Irssi y `ft_irc`. El servidor IRC debe convertir ese flujo de bytes en comandos IRC completos y responderlos correctamente.