Un servidor IRC necesita hacer funcionar dos capas distintas:

- La capa de red TCP: aceptar y mantener conexiones.
- La capa IRC: interpretar comandos y gestionar usuarios y canales.

## 1. Arrancar el servidor

El ejecutable normalmente se inicia así:

```bash
./ircserv \<port> \<password>
```

Por ejemplo:

```bash
./ircserv 6667 secret
```

El servidor debe:

- Validar el puerto.
- Guardar la contraseña.
- Crear el socket principal.
- Asociarlo al puerto.
- Empezar a escuchar conexiones.
- Entrar en un bucle de eventos.

## 2. Crear el socket que escucha

El servidor necesita un socket TCP principal:

```cpp
int serverSocketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
```

Después debe:

- Configurar el socket como no bloqueante.
- Asociarlo a una dirección y un puerto mediante bind().
- Ponerlo en modo escucha mediante listen().

Conceptualmente:

`socket() → fcntl() → bind() → listen()`

Este socket no representa a un cliente. Su función es exclusivamente recibir nuevas conexiones.

## 3. Esperar actividad con poll()

El servidor no puede quedarse bloqueado atendiendo exclusivamente a un cliente. Debe poder gestionar varios clientes simultáneamente.

Para eso se utiliza `poll()`:

```cpp
poll(socketDescriptors, descriptorCount, -1);
```

`poll()` informa de qué sockets tienen actividad:

- El socket principal tiene actividad: hay una conexión nueva.
- Un socket de cliente tiene actividad: ese cliente ha enviado datos.
- Un socket tiene un error o se ha cerrado: hay que desconectar al cliente.
- Un cliente puede recibir datos: hay mensajes pendientes de envío.

La estructura general:

```text
while (server is running)
    poll()
    accept new connections
    read client data
    process complete IRC commands
    send pending responses
    disconnect invalid or closed clients
```

## 4. Aceptar clientes

Cuando el socket principal tiene actividad, se llama a:

```cpp
int clientSocketFileDescriptor = accept(...);
```

El valor devuelto identifica la conexión concreta con ese cliente.

Por ejemplo:

```text
Socket 3 → socket principal del servidor
Socket 4 → primer cliente
Socket 5 → segundo cliente
Socket 6 → tercer cliente
```

Cada cliente necesita su propio estado:

```cpp
class Client
{
private:
    int socketFileDescriptor;
    std::string nickname;
    std::string username;
    std::string inputBuffer;
    std::string outputBuffer;
    bool passwordAccepted;
    bool registered;
};
```

El descriptor identifica la conexión, pero no contiene toda la información IRC del usuario. Por eso necesitas un objeto `Client`.

## 5. Recibir datos

Cuando `poll()` avisa de que un cliente ha enviado algo, se utiliza `recv()`:

```cpp
char buffer[4096];

ssize_t receivedBytes = recv(
    clientSocketFileDescriptor,
    buffer,
    sizeof(buffer),
    0
);
```

Pero hay una cuestión fundamental: TCP no conserva los límites de los mensajes.

El cliente puede enviar:

`NICK roxana\r\nUSER roxana 0 * :Roxana\r\n`

Y `recv()` podría entregártelo de formas diferentes:

```text
Primer recv:  NICK ro
Segundo recv: xana\r\nUSER rox
Tercer recv:  ana 0 * :Roxana\r\n
```

Por eso cada cliente necesita un `inputBuffer`.

Los datos recibidos se añaden:

```cpp
client.getInputBuffer().append(buffer, receivedBytes);
```

Solo se procesa una instrucción cuando aparece el final IRC:

`\r\n`

## 6. Parsear los comandos IRC

Cuando tienes una línea completa:

`PRIVMSG #general :Hola a todos\r\n`

El parser debe separarla en algo similar a:

```text
Command: PRIVMSG
Parameters:
    #general
    Hola a todos
```

Una estructura útil podría ser:

```cpp
class Message
{
public:
    std::string prefix;
    std::string command;
    std::vector<std::string> parameters;
};
```

El último parámetro puede comenzar con `:` y contener espacios:

`PRIVMSG #general :Este mensaje contiene espacios`

Aquí `Este mensaje contiene espacios` es un único parámetro.

## 7. Registrar al cliente

Una conexión TCP no convierte automáticamente al cliente en un usuario IRC registrado.

Normalmente, el cliente debe enviar:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

El servidor debe verificar:

- Que la contraseña sea correcta.
- Que el nickname sea válido.
- Que el nickname no esté ocupado.
- Que haya recibido NICK.
- Que haya recibido USER.

Cuando se cumplen todas las condiciones:

```cpp
client.setRegistered(true);
```

Entonces se envía el mensaje de bienvenida, normalmente con respuestas numéricas IRC:

`:irc.local 001 roxana :Welcome to the IRC Network roxana`

Por eso Irssi puede mostrar:

```text
Connection established
Not connected to server
```

La conexión TCP se ha establecido, pero no se ha completado correctamente el registro IRC.

## 8. Ejecutar comandos

El servidor necesita asociar cada comando con su comportamiento.

Como mínimo:

- `PASS`: comprobar la contraseña.
- `NICK`: asignar o cambiar nickname.
- `USER`: guardar los datos del usuario.
- `PING/PONG`: mantener la conexión.
- `QUIT`: desconectar al usuario.
- `JOIN`: entrar en un canal.
- `PART`: salir de un canal.
- `PRIVMSG`: enviar mensajes.
- `KICK`: expulsar a un usuario.
- `INVITE`: invitar a un usuario.
- `TOPIC`: consultar o cambiar el tema.
- `MODE`: configurar modos del canal.

Un dispatcher puede tomar esta decisión:

```cpp
if (message.getCommand() == "NICK")
    handleNick(client, message);
else if (message.getCommand() == "JOIN")
    handleJoin(client, message);
else if (message.getCommand() == "PRIVMSG")
    handlePrivmsg(client, message);
```

Más adelante se puede sustituir esa cadena de if por un mapa de funciones.

## 9. Gestionar canales

El servidor necesita guardar los canales existentes:

```cpp
class Channel
{
private:
    std::string name;
    std::string topic;
    std::set<Client *> members;
    std::set<Client *> operators;
    std::set<Client *> invitedClients;
    std::string key;
    std::size_t userLimit;
    bool inviteOnly;
    bool topicRestricted;
};
```

Cuando alguien ejecuta:

`JOIN #general`

El servidor debe:

- Buscar el canal.
- Crearlo si no existe.
- Comprobar contraseña, invitación y límite.
- Añadir al cliente.
- Informar a los miembros.
- Enviar el tema.
- Enviar la lista de usuarios.

## 10. Enviar mensajes

Para contestar se utiliza `send()`:

```cpp
send(
    clientSocketFileDescriptor,
    response.c_str(),
    response.size(),
    0
);
```

Sin embargo, `send()` puede enviar solo una parte del mensaje. Por eso es recomendable que cada cliente tenga un `outputBuffer`.

Ejemplo:

```text
Mensaje pendiente: 120 bytes
send() envía:        70 bytes
Restan:              50 bytes
```

Esos 50 bytes deben mantenerse para enviarlos después.

Todos los mensajes IRC deben terminar en:

`\r\n`

Por ejemplo:

```cpp
std::string response =
    ":irc.local 001 roxana :Welcome to the IRC Network\r\n";
```

## 11. Respuestas y errores numéricos

IRC utiliza códigos numéricos para muchas respuestas:

```text
001 → bienvenida
331 → el canal no tiene tema
332 → tema del canal
353 → lista de usuarios
366 → fin de la lista de usuarios
401 → nickname inexistente
403 → canal inexistente
431 → falta nickname
433 → nickname ocupado
461 → faltan parámetros
464 → contraseña incorrecta
```

El servidor debe construirlos con el formato correcto. No basta con enviar simplemente:

`Error: nickname ocupado`

Irssi espera respuestas compatibles con el protocolo IRC.

## 12. Desconexión y limpieza

Un cliente puede desconectarse porque:

- Ejecuta QUIT.
- Cierra Irssi.
- `recv()` devuelve 0.
- Se produce un error de socket.
- El servidor rechaza la contraseña.
- Su conexión deja de ser válida.

Al desconectarlo, el servidor debe:

- Avisar a los usuarios afectados.
- Sacarlo de todos los canales.
- Eliminar canales vacíos cuando corresponda.
- Quitar su descriptor de poll().
- Cerrar el socket.
- Destruir el objeto Client.

Es importante no dejar punteros al cliente dentro de ningún canal.
