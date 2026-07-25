```text
socket + poll
    ↓
recepción de bytes
    ↓
buffer por cliente
    ↓
extracción de líneas completas
    ↓
parser IRC
    ↓
validación y ejecución del comando
    ↓
generación de respuestas
    ↓
buffer de salida
    ↓
send cuando poll indique POLLOUT
```

## Fase 0 — Estudiar el protocolo y elegir cliente de referencia

- Elegir un cliente real para las pruebas: HexChat, irssi, WeeChat, etc.
- Observar qué comandos envía nada más conectarse.
- Definir exactamente qué comandos se van a soportar.
- Definir el formato interno de mensajes.
- Definir los códigos numéricos que se necesitarán.

El cliente probablemente enviará algo parecido a:

```text
CAP LS 302
PASS password
NICK roxana
USER roxana 0 * :Roxana
```
Aunque `CAP` no sea una funcionalidad principal del subject, es habitual que los clientes reales lo envíen. El servidor debería reconocerlo o ignorarlo correctamente, no desconectarse.

## Fase 1 — Esqueleto del proyecto
- Comprobar que existen exactamente dos argumentos.
- Validar el puerto.
- Guardar la contraseña.
- Construir el objeto Server.
- Gestionar la finalización mediante señales.
- Cerrar correctamente todos los file descriptors.

Ejecución:

```bash
./ircserv port password
```

## Fase 2 — Socket de escucha

- `socket()`.
- `setsockopt()` con `SO_REUSEADDR`.
- `fcntl()` para hacerlo no bloqueante.
- `bind()`.
- `listen()`.

```bash
nc 127.0.0.1 6667
``` 

Debe poder establecer una conexión TCP.

## Fase 3 — Bucle principal con un único poll()

El servidor debería mantener una colección similar a:

```cpp
std::vector<pollfd> pollDescriptors;
```

El primer descriptor será normalmente el socket de escucha:

```text
pollDescriptors[0] → listening socket
pollDescriptors[1] → client A
pollDescriptors[2] → client B
pollDescriptors[3] → client C
```

El bucle principal será conceptualmente:

```text
poll()
 ├── listener tiene POLLIN → accept()
 ├── cliente tiene POLLIN → recv()
 ├── cliente tiene POLLOUT → send()
 └── error/desconexión → eliminar cliente
 ```

 No se debe tener un poll() para aceptar, otro para leer y otro para escribir. Debe existir un único punto central que gestione todos los descriptores. El subject prohíbe hacer recv() o send() sin haber comprobado previamente la disponibilidad mediante poll() o equivalente.

 Objetivo:

- Aceptar varios clientes.
- No bloquearse.
- Detectar desconexiones.
- Eliminar correctamente sus descriptores.

## Fase 4 — Modelo básico de cliente

Cuando se acepte una conexión, se crea un objeto ``Client``.
```text
Client
 ├── socket file descriptor
 ├── input buffer
 ├── output buffer
 ├── nickname
 ├── username
 ├── real name
 ├── password accepted
 ├── registered
 └── channels joined
```

Estados recomendados:
```cpp
bool passwordAccepted;
bool nicknameReceived;
bool usernameReceived;
bool registered;
```

Un cliente estará registrado cuando se cumpla:

```text
PASS correcto
    +
NICK válido y disponible
    +
USER recibido
```

## Fase 5 — Reconstrucción del flujo TCP

Cada cliente necesita un buffer persistente: ``std::string inputBuffer;``

Cuando recv() devuelve datos:
1. datos recibidos
2. se añaden a inputBuffer
3. se buscan terminadores de línea
4. se extraen únicamente comandos completos
5. los restos permanecen en inputBuffer

Ejemplo de recepción fragmentada:
```text
Primer recv:   "PRIV"
Segundo recv:  "MSG #general :Hola"
Tercer recv:   "\r\n"
```
El parser no debe recibir esas tres partes. Debe recibir finalmente:

```text
PRIVMSG #general :Hola
```
También puede pasar lo contrario:
```text
PASS secret\r\nNICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```
Todo eso puede llegar en un único `recv()`, por lo que se debe extraer tres comandos.

- recv() no llama directamente al parser con lo que acaba de recibir.
- recv() añade bytes al buffer.
- El sistema de framing extrae líneas.
- El parser recibe líneas completas.

## Fase 6 — Parser IRC

El parser debería transformar:

`PRIVMSG #general :Hola a todo el mundo`

en algo asi:

```text
Command
 ├── name: "PRIVMSG"
 ├── parameters:
 │    └── "#general"
 └── trailing: "Hola a todo el mundo"
 ```

Estructura sencilla:
```cpp
class Command
{
private:
    std::string commandName;
    std::vector<std::string> parameters;
};
```

#### Reglas fundamentales del parser

La línea:
`COMMAND param1 param2 :texto con espacios`

contiene:
- Un nombre de comando.
- Parámetros separados por espacios.
- Un parámetro final opcional que empieza por `:`.
- El parámetro final puede contener espacios.

Ejemplo:

`USER roxana 0 * :Roxana Example`

Debe producir:

```text
command = USER
parameters[0] = roxana
parameters[1] = 0
parameters[2] = *
parameters[3] = Roxana Example
```

## Fase 7 — Buffer de salida y escritura no bloqueante

No dar por hecho que una llamada a send() enviará todo el mensaje.

Cada cliente debe tener:

```cpp
std::string outputBuffer;
```

Cuando se quiera responder:

```text
respuesta IRC
    ↓
se añade a outputBuffer
    ↓
se activa POLLOUT para ese cliente
    ↓
poll informa de que puede escribirse
    ↓
send intenta enviar
    ↓
se eliminan solo los bytes realmente enviados
```

Ejemplo:

```text
outputBuffer tiene 200 bytes
send devuelve 80
quedan 120 bytes pendientes
```

No se deben borrar los 200 bytes.

Cuando el buffer quede vacío, se deja de solicitar POLLOUT, porque de lo contrario poll() puede despertarse constantemente y consumir CPU.

## Fase 8 — Sistema de respuestas IRC

Centralizar la construcción de mensajes.

Formato habitual:

`:server.name 001 roxana :Welcome to the IRC Network`

Mensaje emitido por un usuario:

`:roxana!username@hostname PRIVMSG #general :Hola`

Conviene crear funciones separadas:

```text
buildNumericReply()
buildClientPrefix()
sendReply()
queueMessage()
```

Implementar respuestas de error básicas:

```text
431 ERR_NONICKNAMEGIVEN
432 ERR_ERRONEUSNICKNAME
433 ERR_NICKNAMEINUSE
451 ERR_NOTREGISTERED
461 ERR_NEEDMOREPARAMS
462 ERR_ALREADYREGISTERED
464 ERR_PASSWDMISMATCH
421 ERR_UNKNOWNCOMMAND
```

## Fase 9 — Registro del cliente

`PASS`

```text
PASS secret
```

Debe comprobar:

- Que tiene parámetro.
- Que el usuario todavía no está registrado.
- Que la contraseña coincide.

`NICK`

```text
NICK roxana
```

Debe comprobar:

- Que existe el parámetro.
- Que el nickname tiene un formato válido.
- Que no está siendo usado.
- Que el cambio se propaga si el cliente ya estaba registrado.

Se necesita búsqueda global eficiente:

`nickname -> Client`

`USER`

```text
USER roxana 0 * :Roxana
```

Debe guardar:

- Username.
- Real name.
- Otros campos que decidáis conservar.

Después de `PASS`, `NICK` o `USER`, llamar a una función similar a:

`void Server::tryRegisterClient(Client &client);`

Cuando se completen los requisitos, se envía el mensaje de bienvenida una sola vez.

## Fase 10 — Comandos auxiliares de conexión

Un cliente real puede enviar:

`PING :token`

El servidor debe responder:

`PONG :token`

---

`QUIT`

Debe:

- Notificar la salida a los clientes afectados.
- Eliminar al usuario de todos sus canales.
- Eliminarlo de listas de operadores e invitados.
- Cerrar su descriptor.
- Eliminar su nickname de los índices globales.

`CAP`

Se puede implementar una respuesta mínima o finalizar correctamente la negociación. Como mínimo, no debe romper el registro.

## Fase 11 — Modelo de canal

Crear clase `Channel`.

```text
Channel
 ├── name
 ├── topic
 ├── members
 ├── operators
 ├── invited clients
 ├── invite-only mode
 ├── topic-restricted mode
 ├── key
 └── user limit
 ```

Estados correspondientes a los modos obligatorios:

```cpp
bool inviteOnly;
bool topicRestricted;
bool keyEnabled;
bool limitEnabled;
std::string channelKey;
std::size_t userLimit;
```

El primer usuario que crea o entra en un canal vacío debería convertirse en operador.

El servidor debería ser propietario de los canales:

`channel name → Channel`

No conviene que cada cliente posea copias independientes del mismo canal.

## Fase 12 — `JOIN`

`JOIN #general`

El flujo debería ser:


```text
¿Cliente registrado?
    ↓
¿Nombre de canal válido?
    ↓
¿Existe?
 ├── no → crearlo y hacer operador al primer usuario
 └── sí → comprobar restricciones
              ├── modo +i
              ├── clave +k
              └── límite +l
    ↓
añadir cliente
    ↓
notificar JOIN
    ↓
enviar topic
    ↓
enviar lista de miembros
```

Respuestas habituales:

```text
331 RPL_NOTOPIC
332 RPL_TOPIC
353 RPL_NAMREPLY
366 RPL_ENDOFNAMES
```

Errores relevantes:

```text
403 ERR_NOSUCHCHANNEL
471 ERR_CHANNELISFULL
473 ERR_INVITEONLYCHAN
475 ERR_BADCHANNELKEY
```

Aunque `PART` no aparezca como una de las funciones centrales del subject, implementarlo en este punto simplifica las pruebas y ofrece un comportamiento más natural con clientes reales.

## Fase 13 — `PRIVMSG`

Primero implementar mensajes privados entre users:

`PRIVMSG roxana :Hola`

Después mensajes a canales:

`PRIVMSG #general :Hola a todos`

Para un canal:

- El emisor debe pertenecer al canal.
- El mensaje se reenvía a todos los demás miembros.
- Normalmente no se reenvía al propio emisor.
- Debe conservarse el prefijo del emisor.

Ejemplo enviado a los receptores:

`:roxana!roxana@localhost PRIVMSG #general :Hola a todos`

El subject exige mensajes privados y que los mensajes dirigidos a un canal se distribuyan a los demás miembros.

Errores relevantes:

```text
401 ERR_NOSUCHNICK
403 ERR_NOSUCHCHANNEL
404 ERR_CANNOTSENDTOCHAN
411 ERR_NORECIPIENT
412 ERR_NOTEXTTOSEND
```

## Fase 14 — `TOPIC`

Implementar primero la consulta:

`TOPIC #general`

Y luego la modificación:

`TOPIC #general :Nuevo tema`

Reglas:

- El canal debe existir.
- El usuario debe pertenecer al canal.
- Si está activo el modo +t, solo un operador puede cambiarlo.
- Consultarlo no debería requerir ser operador.
- El cambio debe notificarse al canal.

## Fase 15 — `INVITE`

Formato:

`INVITE roxana #privado`

Comprobaciones:

- El canal existe.
- El usuario objetivo existe.
- El emisor pertenece al canal.
- El objetivo no pertenece ya al canal.
- Cuando corresponda, el emisor debe ser operador.
- El usuario invitado se añade a la colección de invitados.

Después, un usuario invitado puede superar la restricción `+i` al hacer `JOIN`.

Una vez consumida la invitación, conviene eliminarla.

## Fase 16 — `KICK`

Formato:

`KICK #general roxana :Motivo`

Debe comprobar:

- Canal existente.
- Usuario objetivo existente.
- Emisor perteneciente al canal.
- Emisor operador.
- Objetivo perteneciente al canal.

Después:

- Notificar el KICK a todos los miembros.
- Eliminar el objetivo del canal.
- Eliminar sus privilegios de operador si los tenía.
- Eliminar el canal si queda vacío.

## Fase 17 — `MODE`

El subject exige:

```text
+i / -i    invite only
+t / -t    topic restricted
+k / -k    channel key
+o / -o    channel operator
+l / -l    user limit
```

El subject exige expresamente `KICK`, `INVITE`, `TOPIC` y estos cinco modos de canal.

##### Orden recomendado

Primero:

`MODE #channel`

Para consultar los modos actuales.

Después:

```text
MODE #channel +i
MODE #channel -i
MODE #channel +t
MODE #channel -t
```

Luego modos con argumentos:

```text
MODE #channel +k secret
MODE #channel -k
MODE #channel +l 10
MODE #channel -l
MODE #channel +o roxana
MODE #channel -o roxana
```

Finalmente, combinaciones:

```text
MODE #channel +it
MODE #channel +kl secret 10
MODE #channel -it
MODE #channel +o-l roxana
```

Es necesario recorrer la cadena de modos y consumir parámetros solo cuando el modo lo requiera.

Ejemplo:

`MODE #general +kol password roxana 10`

Cada letra tiene una semántica y un parámetro asociado distinto. Conviene implementar un parser específico de modos, separado del parser general de mensajes IRC.

## Fase 18 — Limpieza de estado y desconexiones

Al desconectarse un cliente hay que:

```text
eliminarlo del poll
eliminarlo del mapa de clientes
eliminar su nickname
eliminarlo de todos los canales
eliminarlo de operators
eliminarlo de invited
notificar QUIT
cerrar el fd
borrar canales vacíos
```

Hay que evitar:

- Iteradores invalidados.
- Punteros colgantes.
- Clientes presentes en un canal después de ser destruidos.
- Operadores que ya no son miembros.
- Invitaciones a clientes inexistentes.
- Canales vacíos que siguen almacenados.

Es recomendable que toda desconexión pase por una única función:

```cpp
void Server::disconnectClient(int clientFileDescriptor, const std::string &reason);
```

Nunca repartir parcialmente esta lógica entre `recv()`, `QUIT`, `KICK` y el destructor.

## Fase 19 — Robustez y tests adversos

#### Framing TCP

```text
"PRIV"
"MSG #general :ho"
"la\r"
"\n"
```

#### Varios comandos juntos

`"NICK one\r\nUSER one 0 * :One\r\nJOIN #a\r\n"`

#### Terminadores

```text
\r\n
\n
```

Internamente se puede ser tolerante con `\n`, pero al enviar respuestas IRC se debe usar `\r\n`.

#### Escrituras parciales

Simular un buffer de salida grande y verificar que no se pierden bytes cuando `send()` devuelve menos bytes de los solicitados.

#### Errores de protocolo

Probar:

```text
NICK
NICK existingNick
JOIN
JOIN invalid
PRIVMSG
PRIVMSG nobody :hello
MODE #channel +k
KICK #channel nobody
```

#### Desconexiones

- Desconectar un usuario que está en varios canales.
- Desconectar al único operador.
- Desconectar al último miembro.
- Cerrar el cliente mientras tiene mensajes pendientes.
- Recibir `POLLHUP`, `POLLERR` o `recv() == 0`.

#### Memoria

```bash
valgrind --leak-check=full --track-fds=yes ./ircserv 6667 secret
```

## Clientes

Irssi como cliente principal.

Es ligero, funciona en terminal y obliga a entender los comandos IRC reales, sin que una interfaz gráfica esconda fallos de el servidor. Además es muy cómodo para abrir varias conexiones y probar canales, operadores, kicks, invites, etc.

```bash
sudo apt install irssi
irssi
```

Después levantar escucha:

```bash
nc -lv 127.0.0.1 6667
```

Recomendaciones:

- Irssi: cliente principal para desarrollar y evaluar comportamiento IRC.
- netcat (`nc`): pruebas de bajo nivel del parser, comandos incompletos, CRLF, errores y casos límite.
- HexChat: opcional, al final, para comprobar que también funciona con un cliente gráfico real.