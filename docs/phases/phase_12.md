# Fase 12 — Implementación de `JOIN`

## Objetivo

Implementar el comando `JOIN`, encargado de permitir que un cliente registrado entre en un canal.

Esta fase conecta el modelo de cliente con el modelo de canal desarrollado anteriormente. El servidor deberá:

- Crear canales cuando todavía no existan.
- Comprobar las restricciones de acceso.
- Añadir clientes a los canales.
- Asignar operadores.
- Notificar la entrada a los miembros.
- Enviar el topic y la lista de usuarios del canal.

---

## Sintaxis del comando

La forma básica es:

```text
JOIN #general
```

Si el canal requiere contraseña mediante el modo `+k`:

```text
JOIN #general contraseña
```

También se puede admitir la entrada en varios canales:

```text
JOIN #general,#programacion claveGeneral,claveProgramacion
```

Para comenzar, se puede implementar primero la entrada en un único canal y ampliar posteriormente el handler para aceptar listas separadas por comas.

---

## Validaciones iniciales

Antes de intentar añadir al cliente, el servidor debe comprobar:

1. Que el cliente está registrado.
2. Que el comando contiene el nombre de un canal.
3. Que el nombre del canal tiene un formato válido.
4. Que el cliente todavía no pertenece al canal.
5. Que se cumplen las restricciones de acceso del canal.

Ejemplo de validación inicial:

```text
¿Cliente registrado?
    ↓
¿Se recibió un nombre de canal?
    ↓
¿El nombre del canal es válido?
    ↓
¿El cliente ya pertenece al canal?
```

Si el cliente ya pertenece al canal, el servidor puede ignorar silenciosamente el comando para evitar añadirlo dos veces.

---

## Validación del nombre del canal

Como mínimo, el nombre debería:

- Comenzar por `#`.
- Contener al menos un carácter después del prefijo.
- No contener espacios.
- No contener comas.
- No contener caracteres de control.
- No superar el límite de longitud decidido por el servidor.

Ejemplos válidos:

```text
#general
#programacion
#cpp
```

Ejemplos inválidos:

```text
general
#
#canal con espacios
#canal,otro
```

Conviene centralizar esta comprobación en una función:

```cpp
bool isValidChannelName(const std::string &channelName);
```

---

## Flujo principal de `JOIN`

```text
¿Cliente registrado?
    ↓
¿Nombre de canal válido?
    ↓
¿El canal existe?
 ├── No
 │    ↓
 │   Crear el canal
 │    ↓
 │   Añadir al cliente
 │    ↓
 │   Convertirlo en operador
 │
 └── Sí
      ↓
     Comprobar restricciones
      ├── modo +i
      ├── modo +k
      └── modo +l
           ↓
         Añadir al cliente
           ↓
         Notificar JOIN
           ↓
         Enviar topic
           ↓
         Enviar lista de miembros
```

---

## Creación de un canal

Si el canal solicitado no existe, el servidor debe:

1. Crear un nuevo objeto `Channel`.
2. Guardarlo en la colección global de canales.
3. Añadir al cliente como miembro.
4. Convertir al cliente en operador del canal.

Ejemplo conceptual:

```cpp
std::map<std::string, Channel> channels;
```

El servidor debe ser el propietario de los canales. Los clientes únicamente deben guardar una referencia, identificador o nombre de los canales a los que pertenecen.

El primer usuario de un canal nuevo debe convertirse automáticamente en operador:

```text
JOIN #general
```

Resultado:

```text
Miembros: roxana
Operadores: roxana
```

En la respuesta de `NAMES`, un operador normalmente se representa mediante `@`:

```text
@roxana
```

---

## Comprobación de restricciones

Si el canal ya existe, deben comprobarse sus modos antes de añadir al cliente.

El orden recomendado es:

1. Comprobar si el cliente ya está dentro.
2. Comprobar el límite de usuarios `+l`.
3. Comprobar el modo de invitación `+i`.
4. Comprobar la contraseña `+k`.

No se debe modificar ningún estado del canal hasta que todas las validaciones hayan terminado correctamente.

---

## Modo `+i` — Canal solo para invitados

Cuando el canal tenga activo el modo `+i`, solamente podrán entrar los clientes incluidos en su lista de invitados.

```text
MODE #general +i
```

Si el cliente no está invitado:

```text
473 ERR_INVITEONLYCHAN
```

Formato aproximado:

```text
:server.name 473 roxana #general :Cannot join channel (+i)
```

Si la entrada tiene éxito, el cliente debería eliminarse de la lista de invitados, porque la invitación ya ha sido consumida.

---

## Modo `+k` — Canal protegido por contraseña

Cuando el canal tenga activo el modo `+k`, el cliente debe proporcionar la contraseña correcta:

```text
JOIN #general contraseña
```

El servidor debe comprobar:

- Que se proporcionó una contraseña.
- Que coincide exactamente con la contraseña del canal.

Si no existe o es incorrecta:

```text
475 ERR_BADCHANNELKEY
```

Formato aproximado:

```text
:server.name 475 roxana #general :Cannot join channel (+k)
```

---

## Modo `+l` — Límite de usuarios

Cuando el canal tenga activo el modo `+l`, el servidor debe comprobar que todavía existe espacio disponible.

Ejemplo:

```text
MODE #general +l 10
```

La comprobación debe realizarse antes de añadir al cliente:

```text
número de miembros >= límite
```

Si el canal está lleno:

```text
471 ERR_CHANNELISFULL
```

Formato aproximado:

```text
:server.name 471 roxana #general :Cannot join channel (+l)
```

---

## Añadir al cliente

Cuando todas las validaciones hayan tenido éxito:

1. Añadir el cliente a la colección de miembros del canal.
2. Registrar el canal entre los canales del cliente.
3. Eliminar al cliente de la lista de invitados, si estaba incluido.
4. Asignarle el estado de operador si es el primer miembro.

Es importante mantener sincronizados ambos lados de la relación:

```text
Channel → contiene al Client
Client  → conoce el Channel
```

No debe ocurrir que el canal contenga al cliente pero el cliente no tenga registrado el canal, o viceversa.

Conviene centralizar esta operación:

```cpp
void Server::addClientToChannel(
    Client &client,
    Channel &channel
);
```

---

## Notificación de entrada

Después de añadir al cliente, debe enviarse el mensaje `JOIN` a todos los miembros del canal, incluido el propio cliente.

Formato:

```text
:nickname!username@hostname JOIN :#general
```

Ejemplo:

```text
:roxana!roxana@127.0.0.1 JOIN :#general
```

La notificación debe usar el prefijo completo del cliente:

```cpp
std::string buildClientPrefix(const Client &client);
```

Y debería enviarse mediante una función de difusión:

```cpp
void Server::broadcastToChannel(
    const Channel &channel,
    const std::string &message
);
```

El cliente debe añadirse al canal antes de realizar el broadcast para que también reciba la confirmación de su propio `JOIN`.

---

## Envío del topic

Después de confirmar la entrada, el servidor debe informar del topic actual.

### Canal sin topic

Si el canal no tiene topic:

```text
331 RPL_NOTOPIC
```

Ejemplo:

```text
:server.name 331 roxana #general :No topic is set
```

### Canal con topic

Si el canal tiene topic:

```text
332 RPL_TOPIC
```

Ejemplo:

```text
:server.name 332 roxana #general :Canal general del servidor
```

---

## Envío de la lista de miembros

Después del topic, el servidor debe enviar la lista de miembros mediante dos respuestas.

### `353 RPL_NAMREPLY`

Contiene los usuarios presentes en el canal:

```text
:server.name 353 roxana = #general :@roxana usuario2 usuario3
```

Los operadores deben aparecer con el prefijo `@`:

```text
@roxana
```

Los usuarios normales aparecen sin prefijo:

```text
usuario2
```

### `366 RPL_ENDOFNAMES`

Indica que la lista ha terminado:

```text
:server.name 366 roxana #general :End of /NAMES list
```

El orden completo debe ser:

```text
JOIN
331 o 332
353
366
```

Ejemplo completo:

```text
:roxana!roxana@127.0.0.1 JOIN :#general
:server.name 331 roxana #general :No topic is set
:server.name 353 roxana = #general :@roxana
:server.name 366 roxana #general :End of /NAMES list
```

---

## Respuestas numéricas necesarias

| Código | Nombre | Situación |
|---:|---|---|
| `331` | `RPL_NOTOPIC` | El canal no tiene topic |
| `332` | `RPL_TOPIC` | El canal tiene un topic |
| `353` | `RPL_NAMREPLY` | Lista de miembros del canal |
| `366` | `RPL_ENDOFNAMES` | Fin de la lista de miembros |
| `403` | `ERR_NOSUCHCHANNEL` | El nombre del canal no puede utilizarse |
| `451` | `ERR_NOTREGISTERED` | El cliente todavía no está registrado |
| `461` | `ERR_NEEDMOREPARAMS` | No se proporcionó un canal |
| `471` | `ERR_CHANNELISFULL` | El canal ha alcanzado su límite |
| `473` | `ERR_INVITEONLYCHAN` | El canal requiere invitación |
| `475` | `ERR_BADCHANNELKEY` | La contraseña es incorrecta |

---

## Estructura recomendada del handler

El handler puede dividirse en operaciones pequeñas:

```cpp
void CommandDispatcher::handleJoin(
    Client &client,
    const IrcMessage &message
);

bool Server::canJoinChannel(
    const Client &client,
    const Channel &channel,
    const std::string &providedKey
);

void Server::addClientToChannel(
    Client &client,
    Channel &channel
);

void Server::sendChannelTopic(
    Client &client,
    const Channel &channel
);

void Server::sendChannelNames(
    Client &client,
    const Channel &channel
);
```

Responsabilidades del handler:

```text
handleJoin()
    ├── validar registro y parámetros
    ├── validar nombre
    ├── localizar o crear canal
    ├── comprobar restricciones
    ├── añadir cliente
    ├── emitir JOIN
    ├── enviar topic
    └── enviar NAMES
```

---

## Implementación recomendada de `PART`

Aunque `PART` no sea uno de los comandos centrales del subject, implementarlo en esta fase facilita considerablemente las pruebas.

Sintaxis:

```text
PART #general
```

Con mensaje opcional:

```text
PART #general :Hasta luego
```

El servidor debe:

1. Comprobar que el cliente está registrado.
2. Comprobar que el canal existe.
3. Comprobar que el cliente pertenece al canal.
4. Notificar la salida a los miembros.
5. Eliminar al cliente de la lista de miembros.
6. Eliminarlo de la lista de operadores.
7. Eliminar el canal de la colección del cliente.
8. Eliminar el canal del servidor si queda vacío.

Mensaje de salida:

```text
:roxana!roxana@127.0.0.1 PART #general :Hasta luego
```

La notificación debe enviarse antes de eliminar al cliente, para que este también pueda recibir su propio mensaje `PART`.

Errores útiles para `PART`:

| Código | Nombre | Situación |
|---:|---|---|
| `403` | `ERR_NOSUCHCHANNEL` | El canal no existe |
| `442` | `ERR_NOTONCHANNEL` | El cliente no pertenece al canal |
| `461` | `ERR_NEEDMOREPARAMS` | No se proporcionó un canal |

---

## Casos de prueba mínimos

### Crear un canal nuevo

```text
JOIN #general
```

Debe:

- Crear `#general`.
- Añadir al cliente.
- Convertirlo en operador.
- Emitir el mensaje `JOIN`.
- Enviar `331` o `332`.
- Enviar `353`.
- Enviar `366`.

### Entrar en un canal existente

```text
JOIN #general
```

Debe:

- Mantener a los miembros anteriores.
- Añadir al nuevo cliente.
- Notificar el `JOIN` a todos los miembros.
- Enviar al nuevo cliente el topic y la lista de nombres.

### Entrar dos veces en el mismo canal

```text
JOIN #general
JOIN #general
```

El cliente no debe aparecer duplicado.

### Canal lleno

```text
MODE #general +l 1
JOIN #general
```

El segundo cliente debe recibir:

```text
471 ERR_CHANNELISFULL
```

### Canal solo para invitados

```text
MODE #general +i
JOIN #general
```

Un cliente no invitado debe recibir:

```text
473 ERR_INVITEONLYCHAN
```

### Contraseña incorrecta

```text
MODE #general +k secreto
JOIN #general incorrecta
```

Debe responder:

```text
475 ERR_BADCHANNELKEY
```

### Contraseña correcta

```text
JOIN #general secreto
```

El cliente debe entrar normalmente.

### Abandonar el canal

```text
PART #general :Hasta luego
```

Debe:

- Notificar el `PART`.
- Eliminar al cliente del canal.
- Eliminar su estado de operador.
- Eliminar el canal si queda vacío.

---

## Resultado esperado de la fase

Al terminar esta fase, el servidor debe ser capaz de:

- Crear canales dinámicamente.
- Añadir clientes a canales existentes.
- Convertir al primer miembro en operador.
- Aplicar correctamente los modos `+i`, `+k` y `+l`.
- Mantener sincronizada la relación entre clientes y canales.
- Notificar las entradas a todos los miembros.
- Enviar el topic del canal.
- Enviar la lista de miembros con sus prefijos.
- Evitar miembros duplicados.
- Gestionar la salida mediante `PART`.
- Eliminar los canales que queden vacíos.