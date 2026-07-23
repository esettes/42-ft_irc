### Server
 - listening socket
 - poll descriptors
 - clients
 - channels
 - command dispatcher
 - main event loop

### Client
 - connection state
 - registration state
 - identity
 - input buffer
 - output buffer
 - joined channels

### Channel
 - members
 - operators
 - invited clients
 - topic
 - key
 - limit
 - modes

### Message
 - command
 - parameters

## Flujo general

```text
socket → Client → MessageParser → CommandDispatcher → CommandHandler
                                        ↓
                                    MessageRouter
                                ↙        ↓        ↘
                            Client    Channel      Bot
```

Responsabilidades de cada parte:

- `Client`: conexión, buffers y estado del usuario.
- `MessageParser`: convierte texto IRC en IrcMessage.
- `CommandDispatcher`: selecciona el manejador de `NICK`, `JOIN`, `PRIVMSG`, etc.
- `MessageRouter`: decide quién recibe un mensaje.
- `Bot`: procesa únicamente los mensajes dirigidos a él.
- `Channel`: administra miembros, operadores e invitaciones.

### Bot

Si queremos que el bot aparezca como miembro real de canales, en `NAMES`, `WHO`,` pueda recibir mensajes del canal, etc., entonces sí merece la pena separar desde ahora:

- `ClientConnection`: descriptor, buffers y operaciones de red.
- `IrcUser`: nickname, username, canales y estado IRC.

Así un usuario normal tendría conexión, mientras que el bot podría ser un IrcUser sin socket.

### Transferencia de archivos

En IRC normalmente el archivo no pasa por el servidor IRC. Se utiliza DCC:

- El emisor abre un socket TCP para el archivo.
- Envía al receptor un mensaje CTCP mediante PRIVMSG.
- El servidor IRC retransmite ese mensaje sin modificarlo.
- El receptor se conecta directamente al socket del emisor.
- Los bytes del archivo circulan directamente entre ambos clientes.

Un mensaje se parece a:

`PRIVMSG Roxana :\x01DCC SEND example.txt 2130706433 5000 1200\x01`

Los caracteres `\x01` delimitan un mensaje CTCP. Para el servidor sigue siendo un `PRIVMSG` normal.

Por tanto, para soportar DCC hay que asegurar que:

- El parser conserva completo el trailing parameter después de `:`.
- No dividir el contenido de `PRIVMSG por` espacios.
- No eliminar el carácter `\x01`.
- No reinterpretar el mensaje `DCC SEND`.
- Retransmitir el contenido exactamente al destinatario.
- Los sockets IRC son no bloqueantes y soportan envíos parciales mediante el buffer de salida.
- Solo rechazar los caracteres realmente inválidos para una línea IRC, como CR/LF internos o NUL.

```text
command: "PRIVMSG"

parameters[0]:
"Roxana"

parameters[1]:
"\x01DCC SEND example.txt 2130706433 5000 1200\x01"
```
