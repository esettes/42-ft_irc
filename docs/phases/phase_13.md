# Fase 13 — Comando `PRIVMSG`

## Objetivo

Implementar el envío de mensajes:

- Entre dos usuarios.
- Desde un usuario hacia un canal.
- Sin bloquear el servidor.
- Conservando el prefijo completo del emisor.

El comando tiene estas dos formas principales:

```text
PRIVMSG roxana :Hola
PRIVMSG #general :Hola a todos
```

---

## 1. Requisitos previos

Antes de procesar `PRIVMSG`, el servidor debe comprobar que el cliente está registrado.

Si todavía no ha completado `PASS`, `NICK` y `USER`, debe responder:

```text
451 ERR_NOTREGISTERED
```

El comando también depende de que ya existan:

- La búsqueda global de clientes mediante nickname.
- El modelo de canales.
- La lista de miembros de cada canal.
- El sistema centralizado de respuestas IRC.
- El buffer de salida no bloqueante de cada cliente.

---

## 2. Parámetros del comando

Un mensaje privado necesita dos datos:

1. El destinatario.
2. El texto del mensaje.

Por ejemplo:

```text
PRIVMSG roxana :Hola
```

El parser debería producir algo equivalente a:

```text
commandName = "PRIVMSG"
parameters[0] = "roxana"
parameters[1] = "Hola"
```

En un mensaje dirigido a un canal:

```text
PRIVMSG #general :Hola a todos
```

El resultado debería ser:

```text
commandName = "PRIVMSG"
parameters[0] = "#general"
parameters[1] = "Hola a todos"
```

El texto introducido después de `:` debe mantenerse como un único parámetro, aunque contenga espacios.

---

## 3. Orden recomendado de validación

El handler de `PRIVMSG` debería realizar las comprobaciones en este orden:

```text
¿Cliente registrado?
    ↓
¿Existe destinatario?
    ↓
¿Existe texto?
    ↓
¿El destinatario es un canal o un usuario?
    ├── usuario → buscar nickname
    └── canal   → buscar canal y comprobar pertenencia
    ↓
construir mensaje con el prefijo del emisor
    ↓
añadir el mensaje al buffer de salida de los receptores
```

Este orden permite devolver un único error coherente y detener el procesamiento en cuanto se detecta un problema.

---

## 4. Mensajes privados entre usuarios

Ejemplo recibido:

```text
PRIVMSG roxana :Hola
```

El servidor debe:

1. Comprobar que existe el parámetro del destinatario.
2. Comprobar que existe el texto.
3. Buscar globalmente al cliente cuyo nickname sea `roxana`.
4. Construir el mensaje con el prefijo completo del emisor.
5. Añadir el mensaje al buffer de salida del destinatario.

Mensaje enviado al receptor:

```text
:alice!alice@localhost PRIVMSG roxana :Hola
```

El prefijo debe corresponder al usuario que envió el mensaje:

```text
:nickname!username@hostname
```

El servidor no debe sustituir ese prefijo por su propio nombre.

El mensaje normalmente solo se envía al destinatario. No es necesario reenviarlo al emisor.

---

## 5. Mensajes dirigidos a canales

Ejemplo recibido:

```text
PRIVMSG #general :Hola a todos
```

El servidor debe:

1. Comprobar que el canal existe.
2. Comprobar que el emisor pertenece al canal.
3. Construir el mensaje con el prefijo completo del emisor.
4. Recorrer los miembros del canal.
5. Enviar el mensaje a todos los miembros excepto al emisor.

Mensaje recibido por los demás miembros:

```text
:alice!alice@localhost PRIVMSG #general :Hola a todos
```

El mensaje no debe reenviarse al propio emisor.

La distribución sería equivalente a:

```text
Canal #general
├── alice    ← emisor, no recibe copia
├── roxana   ← recibe el mensaje
├── bob      ← recibe el mensaje
└── carol    ← recibe el mensaje
```

Todos los receptores deben recibir exactamente el mismo mensaje.

---

## 6. Identificación del tipo de destinatario

Una forma sencilla de distinguir un canal de un nickname es comprobar el primer carácter:

```cpp
bool isChannelTarget(const std::string &target)
{
    return !target.empty() && target[0] == '#';
}
```

El flujo del handler puede dividirse en dos funciones:

```cpp
void handlePrivateMessage(Client &sender, const Command &command);
void sendMessageToUser(
    Client &sender,
    const std::string &nickname,
    const std::string &message
);
void sendMessageToChannel(
    Client &sender,
    const std::string &channelName,
    const std::string &message
);
```

Esto evita concentrar toda la lógica en una sola función.

---

## 7. Construcción del mensaje

Conviene reutilizar la función encargada de construir el prefijo del cliente.

Ejemplo conceptual:

```cpp
std::string message =
    buildClientPrefix(sender)
    + " PRIVMSG "
    + target
    + " :"
    + messageText
    + "\r\n";
```

El resultado debe respetar el formato IRC:

```text
:alice!alice@localhost PRIVMSG #general :Hola a todos\r\n
```

No debe olvidarse la terminación `\r\n`.

---

## 8. Envío no bloqueante

`PRIVMSG` no debe llamar a `send()` directamente desde el handler.

El mensaje debe añadirse al buffer de salida del receptor:

```text
mensaje construido
    ↓
se añade al outputBuffer del receptor
    ↓
se activa POLLOUT
    ↓
poll() informa de que el socket permite escribir
    ↓
se envían los bytes disponibles
```

Para un canal, el mismo mensaje se añade al buffer de salida de cada miembro receptor.

Esto mantiene el comportamiento no bloqueante implementado en la fase 7.

---

## 9. Errores relevantes

### `401 ERR_NOSUCHNICK`

Se devuelve cuando el nickname destinatario no existe.

Ejemplo:

```text
PRIVMSG usuarioInexistente :Hola
```

Respuesta:

```text
:server.name 401 alice usuarioInexistente :No such nick
```

---

### `403 ERR_NOSUCHCHANNEL`

Se devuelve cuando el canal destinatario no existe.

Ejemplo:

```text
PRIVMSG #inexistente :Hola
```

Respuesta:

```text
:server.name 403 alice #inexistente :No such channel
```

---

### `404 ERR_CANNOTSENDTOCHAN`

Se devuelve cuando el usuario no puede enviar mensajes al canal.

En esta fase, debe utilizarse cuando el emisor no pertenece al canal.

Ejemplo:

```text
PRIVMSG #general :Hola
```

Respuesta:

```text
:server.name 404 alice #general :Cannot send to channel
```

---

### `411 ERR_NORECIPIENT`

Se devuelve cuando no se ha indicado ningún destinatario.

Ejemplo:

```text
PRIVMSG
```

Respuesta:

```text
:server.name 411 alice :No recipient given (PRIVMSG)
```

---

### `412 ERR_NOTEXTTOSEND`

Se devuelve cuando existe un destinatario, pero no hay texto para enviar.

Ejemplos:

```text
PRIVMSG roxana
PRIVMSG roxana :
```

Respuesta:

```text
:server.name 412 alice :No text to send
```

También conviene considerar vacío el trailing de `PRIVMSG roxana :`.

---

## 10. Resumen de errores

| Situación | Código | Nombre |
|---|---:|---|
| Cliente no registrado | `451` | `ERR_NOTREGISTERED` |
| Falta el destinatario | `411` | `ERR_NORECIPIENT` |
| Falta el texto | `412` | `ERR_NOTEXTTOSEND` |
| Nickname inexistente | `401` | `ERR_NOSUCHNICK` |
| Canal inexistente | `403` | `ERR_NOSUCHCHANNEL` |
| El usuario no puede escribir en el canal | `404` | `ERR_CANNOTSENDTOCHAN` |

---

## 11. Estructura recomendada del handler

```cpp
void CommandDispatcher::handlePrivmsg(
    Client &sender,
    const Command &command
)
{
    if (!sender.isRegistered())
    {
        _server.queueNumericReply(sender, 451);
        return;
    }

    if (command.getParameters().empty())
    {
        _server.queueNoRecipientError(sender, "PRIVMSG");
        return;
    }

    if (command.getParameters().size() < 2
        || command.getParameters()[1].empty())
    {
        _server.queueNoTextToSendError(sender);
        return;
    }

    const std::string &target = command.getParameters()[0];
    const std::string &messageText = command.getParameters()[1];

    if (isChannelTarget(target))
    {
        sendMessageToChannel(sender, target, messageText);
        return;
    }

    sendMessageToUser(sender, target, messageText);
}
```

Los nombres concretos pueden adaptarse a la arquitectura del proyecto.

---

## 12. Casos de prueba mínimos

### Mensaje correcto entre usuarios

```text
PRIVMSG roxana :Hola
```

Resultado esperado:

```text
:alice!alice@localhost PRIVMSG roxana :Hola
```

Solo `roxana` recibe el mensaje.

---

### Mensaje correcto a un canal

```text
PRIVMSG #general :Hola a todos
```

Todos los miembros de `#general`, excepto el emisor, reciben:

```text
:alice!alice@localhost PRIVMSG #general :Hola a todos
```

---

### Nickname inexistente

```text
PRIVMSG nadie :Hola
```

Resultado esperado:

```text
401 ERR_NOSUCHNICK
```

---

### Canal inexistente

```text
PRIVMSG #inexistente :Hola
```

Resultado esperado:

```text
403 ERR_NOSUCHCHANNEL
```

---

### Emisor fuera del canal

```text
PRIVMSG #general :Hola
```

Si el emisor no pertenece a `#general`:

```text
404 ERR_CANNOTSENDTOCHAN
```

---

### Falta el destinatario

```text
PRIVMSG
```

Resultado esperado:

```text
411 ERR_NORECIPIENT
```

---

### Falta el texto

```text
PRIVMSG roxana
```

Resultado esperado:

```text
412 ERR_NOTEXTTOSEND
```

---

### Texto vacío

```text
PRIVMSG roxana :
```

Resultado esperado:

```text
412 ERR_NOTEXTTOSEND
```

---

## 13. Criterios para completar la fase

La fase estará terminada cuando:

- `PRIVMSG` rechace a los clientes no registrados.
- Se puedan enviar mensajes privados entre usuarios.
- Se puedan enviar mensajes a canales.
- Los mensajes de canal lleguen a todos los miembros excepto al emisor.
- Un usuario externo no pueda escribir en un canal al que no pertenece.
- Los mensajes conserven el prefijo completo del emisor.
- Los nicknames y canales inexistentes produzcan el error correspondiente.
- La ausencia de destinatario o texto produzca el error correspondiente.
- Todos los mensajes terminen en `\r\n`.
- El envío utilice los buffers de salida no bloqueantes.
- El servidor continúe funcionando correctamente después de recibir comandos `PRIVMSG` inválidos.

> El objetivo principal de esta fase es completar el sistema básico de comunicación exigido por el proyecto: mensajes privados entre usuarios y distribución de mensajes entre los miembros de un canal.