# Fase 8 — Sistema centralizado de respuestas IRC

## Objetivo

Implementar un sistema común para construir y enviar respuestas IRC de forma coherente.

Los distintos handlers de comandos no deben construir manualmente mensajes completos ni llamar directamente a `send()`. Su responsabilidad será decidir qué respuesta corresponde y añadirla al buffer de salida del cliente.

El flujo recomendado es:

```text
handler del comando
        ↓
construcción de la respuesta IRC
        ↓
añadir "\r\n"
        ↓
guardar en outputBuffer
        ↓
activar POLLOUT
        ↓
enviar desde el bucle de poll()
```

Esta fase se apoya en el buffer de salida implementado en la fase 7.

---

## 1. Formato general de un mensaje IRC

Un mensaje IRC puede tener la siguiente estructura:

```text
[:prefijo] COMANDO [parámetros] [:trailing]\r\n
```

Ejemplo de respuesta numérica enviada por el servidor:

```text
:server.name 001 roxana :Welcome to the IRC Network\r\n
```

Sus partes son:

```text
:server.name
```

Prefijo del servidor que origina el mensaje.

```text
001
```

Código numérico de la respuesta.

```text
roxana
```

Nickname del cliente al que se dirige la respuesta.

```text
:Welcome to the IRC Network
```

Último parámetro o `trailing`. Puede contener espacios porque comienza con `:`.

---

## 2. Mensajes originados por clientes

Cuando un comando realizado por un cliente debe enviarse a otros usuarios, el mensaje debe incluir el prefijo completo del cliente.

Formato habitual:

```text
:nickname!username@hostname COMMAND parámetros :trailing\r\n
```

Ejemplo:

```text
:roxana!username@hostname PRIVMSG #general :Hola\r\n
```

El prefijo permite identificar quién originó el mensaje.

Sus componentes son:

- `nickname`: nickname actual del cliente.
- `username`: username recibido mediante `USER`.
- `hostname`: dirección o nombre asociado a la conexión.

---

## 3. Centralizar la construcción de mensajes

No se debe repetir la construcción de prefijos y respuestas en todos los handlers.

En lugar de hacer esto:

```cpp
client.queueOutput(
    ":" + serverName + " 461 " + client.getNickname()
    + " PRIVMSG :Not enough parameters\r\n"
);
```

en cada comando, conviene disponer de funciones reutilizables como:

```cpp
std::string buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &parameters,
    const std::string &message
) const;
```

```cpp
std::string buildClientPrefix(const Client &client) const;
```

```cpp
void sendReply(
    Client &client,
    int numericCode,
    const std::string &parameters,
    const std::string &message
);
```

```cpp
void queueMessage(
    Client &client,
    const std::string &message
);
```

Los nombres y responsabilidades exactas pueden adaptarse a la arquitectura del proyecto.

---

## 4. Construcción de respuestas numéricas

La función `buildNumericReply()` debe crear respuestas con un formato uniforme.

Ejemplo conceptual:

```cpp
std::string Server::buildNumericReply(
    int numericCode,
    const Client &client,
    const std::string &parameters,
    const std::string &message
) const;
```

Debe encargarse de:

1. Añadir el prefijo del servidor.
2. Convertir el código numérico a tres cifras.
3. Añadir el destinatario.
4. Añadir los parámetros específicos de la respuesta.
5. Añadir el mensaje final precedido por `:`.
6. Finalizar la respuesta con `\r\n`.

Formato resultante:

```text
:<server-name> <numeric-code> <target> [parameters] :<message>\r\n
```

Por ejemplo:

```text
:irc.example.net 433 roxana roxy :Nickname is already in use\r\n
```

El código numérico debe conservar siempre tres cifras:

```text
001
421
433
464
```

No debe enviarse como:

```text
1
21
33
64
```

---

## 5. Destinatario de una respuesta numérica

Normalmente, una respuesta numérica utiliza como destinatario el nickname del cliente:

```text
:server.name 421 roxana TEST :Unknown command\r\n
```

Sin embargo, el cliente puede no tener todavía un nickname válido. En ese caso se puede utilizar `*` como destinatario:

```text
:server.name 431 * :No nickname given\r\n
```

Conviene centralizar esta decisión:

```text
si el cliente tiene nickname
    usar su nickname
si todavía no tiene nickname
    usar "*"
```

Esto evita que cada handler tenga que comprobarlo por separado.

---

## 6. Construcción del prefijo de cliente

La función `buildClientPrefix()` debe generar el prefijo de un usuario:

```text
:nickname!username@hostname
```

Ejemplo:

```text
:roxana!username@127.0.0.1
```

Este prefijo se utilizará en mensajes como:

- `PRIVMSG`
- `JOIN`
- `PART`
- `QUIT`
- `NICK`
- `KICK`
- `INVITE`
- `TOPIC`

Ejemplo:

```text
:roxana!username@127.0.0.1 JOIN #general\r\n
```

Centralizar este prefijo es importante porque cualquier cambio futuro en su formato solo tendrá que hacerse en un único lugar.

---

## 7. Añadir mensajes al buffer de salida

La función `queueMessage()` debe añadir el mensaje al `outputBuffer` del cliente.

Ejemplo conceptual:

```cpp
void Server::queueMessage(
    Client &client,
    const std::string &message
);
```

Sus responsabilidades deberían ser:

1. Recibir el mensaje IRC ya construido.
2. Comprobar o garantizar que termina en `\r\n`.
3. Añadirlo al buffer de salida del cliente.
4. Activar `POLLOUT` para su socket.

No debe asumir que el mensaje será enviado inmediatamente.

```text
queueMessage()
      ↓
outputBuffer
      ↓
POLLOUT
      ↓
send()
```

La escritura real debe seguir realizándose desde el bucle de eventos cuando `poll()` indique que el socket permite escribir.

---

## 8. Diferencia entre `sendReply()` y `queueMessage()`

Una posible separación de responsabilidades es:

### `buildNumericReply()`

Construye el texto de una respuesta numérica, pero no modifica el cliente.

```text
datos de respuesta
        ↓
std::string con formato IRC
```

### `buildClientPrefix()`

Construye el prefijo correspondiente a un cliente.

```text
Client
  ↓
:nickname!username@hostname
```

### `sendReply()`

Solicita la creación y el encolado de una respuesta numérica.

```text
código + parámetros + mensaje
              ↓
      buildNumericReply()
              ↓
        queueMessage()
```

Aunque se llame `sendReply()`, no debería llamar directamente a `send()` si el servidor utiliza escritura no bloqueante.

Un nombre más explícito también podría ser:

```cpp
queueNumericReply();
```

### `queueMessage()`

Añade cualquier mensaje IRC al buffer de salida del cliente.

Puede utilizarse tanto para respuestas numéricas como para mensajes normales:

```text
001
433
PRIVMSG
JOIN
QUIT
PING
```

---

## 9. Terminación correcta de mensajes

Todos los mensajes IRC enviados por el servidor deben terminar en:

```text
\r\n
```

No se debe utilizar solamente:

```text
\n
```

Ejemplo correcto:

```cpp
":server.name 001 roxana :Welcome to the IRC Network\r\n"
```

Conviene decidir una única responsabilidad:

- Las funciones de construcción añaden `\r\n`.
- O `queueMessage()` añade `\r\n`.

No se deben mezclar ambas estrategias, porque podrían producirse mensajes con terminadores duplicados:

```text
\r\n\r\n
```

Una opción sencilla es que todas las funciones `build...()` devuelvan mensajes completos, incluyendo `\r\n`.

---

# Respuestas de error básicas

## 10. `431 ERR_NONICKNAMEGIVEN`

Se utiliza cuando el comando `NICK` no incluye un nickname.

Comando recibido:

```text
NICK
```

Respuesta:

```text
:server.name 431 * :No nickname given\r\n
```

Condición:

```text
el comando es NICK
    +
no contiene el parámetro del nickname
```

---

## 11. `432 ERR_ERRONEUSNICKNAME`

Se utiliza cuando el nickname solicitado tiene un formato inválido.

Ejemplo:

```text
NICK invalid@name
```

Respuesta:

```text
:server.name 432 * invalid@name :Erroneous nickname\r\n
```

Se debe validar, como mínimo:

- Que no esté vacío.
- Que no empiece por un número.
- Que no contenga espacios.
- Que no contenga caracteres incompatibles con las reglas de nickname elegidas.
- Que no supere el límite definido por el servidor.

La validación del nickname debería estar centralizada en una función como:

```cpp
bool isValidNickname(const std::string &nickname) const;
```

---

## 12. `433 ERR_NICKNAMEINUSE`

Se utiliza cuando otro cliente ya está utilizando el nickname solicitado.

Comando recibido:

```text
NICK roxana
```

Respuesta:

```text
:server.name 433 * roxana :Nickname is already in use\r\n
```

Si el cliente ya tenía un nickname anterior, puede utilizarse como destinatario:

```text
:server.name 433 previousNick roxana :Nickname is already in use\r\n
```

La búsqueda debe ser coherente con la comparación de nicknames utilizada por el servidor.

---

## 13. `451 ERR_NOTREGISTERED`

Se utiliza cuando un cliente intenta ejecutar un comando que requiere registro antes de completar el proceso de conexión.

Ejemplo:

```text
PRIVMSG #general :Hola
```

Respuesta:

```text
:server.name 451 * :You have not registered\r\n
```

El servidor debe comprobar si el cliente ha completado:

```text
PASS válido
    +
NICK válido
    +
USER recibido
```

Esta comprobación debería hacerse antes de ejecutar comandos que requieren un cliente registrado.

---

## 14. `461 ERR_NEEDMOREPARAMS`

Se utiliza cuando un comando no contiene todos los parámetros obligatorios.

Ejemplo:

```text
PRIVMSG
```

Respuesta:

```text
:server.name 461 roxana PRIVMSG :Not enough parameters\r\n
```

Otros ejemplos que pueden producir este error:

```text
PASS
USER roxana
JOIN
KICK #general
```

Cada handler debe comprobar su cantidad mínima de parámetros antes de acceder al vector de parámetros.

Ejemplo conceptual:

```cpp
if (command.getParameters().size() < requiredParameterCount)
{
    queueNumericReply(
        client,
        461,
        command.getName(),
        "Not enough parameters"
    );
    return;
}
```

Esto también evita accesos fuera de rango.

---

## 15. `462 ERR_ALREADYREGISTERED`

Se utiliza cuando un cliente registrado intenta repetir una operación que solo pertenece al proceso de registro.

Ejemplo:

```text
USER anotherUser 0 * :Another Name
```

Respuesta:

```text
:server.name 462 roxana :You may not reregister\r\n
```

Debe utilizarse especialmente cuando un cliente ya registrado intenta volver a enviar:

```text
USER
```

También puede aplicarse a otros comandos de registro según las decisiones del servidor.

No debe impedirse necesariamente que un cliente cambie su nickname mediante `NICK`, porque IRC permite cambiarlo después del registro.

---

## 16. `464 ERR_PASSWDMISMATCH`

Se utiliza cuando la contraseña recibida mediante `PASS` no coincide con la contraseña del servidor.

Comando recibido:

```text
PASS wrong-password
```

Respuesta:

```text
:server.name 464 * :Password incorrect\r\n
```

Después de esta respuesta, el servidor debe mantener al cliente como no autenticado.

Debe definirse claramente si:

- Se permite volver a intentar `PASS`.
- Se desconecta inmediatamente al cliente.
- Se desconecta después de varios intentos.

Para una primera implementación, se puede responder con `464` y mantener la conexión abierta para permitir otro intento.

---

## 17. `421 ERR_UNKNOWNCOMMAND`

Se utiliza cuando el servidor recibe un comando que no reconoce o no soporta.

Comando recibido:

```text
TEST something
```

Respuesta:

```text
:server.name 421 roxana TEST :Unknown command\r\n
```

Debe generarse desde el sistema encargado de despachar comandos cuando el nombre del comando no está registrado.

Flujo recomendado:

```text
comando recibido
       ↓
buscar handler
       ↓
handler encontrado → ejecutarlo
handler no encontrado → enviar 421
```

El parámetro `TEST` permite informar al cliente de qué comando fue rechazado.

---

# Tabla de errores mínimos

| Código | Nombre | Cuándo se utiliza |
|---:|---|---|
| `421` | `ERR_UNKNOWNCOMMAND` | El comando no existe o no está soportado |
| `431` | `ERR_NONICKNAMEGIVEN` | `NICK` no contiene nickname |
| `432` | `ERR_ERRONEUSNICKNAME` | El nickname tiene un formato inválido |
| `433` | `ERR_NICKNAMEINUSE` | El nickname ya está siendo utilizado |
| `451` | `ERR_NOTREGISTERED` | El comando requiere que el cliente esté registrado |
| `461` | `ERR_NEEDMOREPARAMS` | Faltan parámetros obligatorios |
| `462` | `ERR_ALREADYREGISTERED` | Un cliente registrado intenta registrarse otra vez |
| `464` | `ERR_PASSWDMISMATCH` | La contraseña recibida es incorrecta |

---

# Respuesta de bienvenida

Cuando el cliente complete correctamente el registro, el servidor debe enviar al menos la respuesta de bienvenida:

```text
:server.name 001 roxana :Welcome to the IRC Network roxana\r\n
```

El registro debe completarse una sola vez cuando se cumplan todas las condiciones:

```text
passwordAccepted == true
nicknameReceived == true
usernameReceived == true
registered == false
```

Después de enviar la bienvenida:

```text
registered = true
```

Esto evita enviar `001` varias veces si el cliente vuelve a ejecutar alguno de los comandos relacionados.

---

# Organización recomendada

Una separación posible es:

```text
Server
├── buildNumericReply()
├── buildClientPrefix()
├── queueNumericReply()
├── queueMessage()
├── updateClientPollEvents()
└── tryRegisterClient()
```

Responsabilidades:

| Función | Responsabilidad |
|---|---|
| `buildNumericReply()` | Construir una respuesta numérica IRC |
| `buildClientPrefix()` | Construir `nickname!username@hostname` |
| `queueNumericReply()` | Construir y encolar una respuesta numérica |
| `queueMessage()` | Añadir un mensaje al buffer de salida |
| `updateClientPollEvents()` | Activar o desactivar `POLLOUT` |
| `tryRegisterClient()` | Comprobar el estado de registro y enviar `001` |

También se pueden guardar los códigos numéricos en un archivo separado:

```text
NumericReplies.hpp
```

Ejemplo:

```cpp
#ifndef NUMERIC_REPLIES_HPP
#define NUMERIC_REPLIES_HPP

namespace NumericReply
{
    const int RPL_WELCOME = 1;
    const int ERR_UNKNOWNCOMMAND = 421;
    const int ERR_NONICKNAMEGIVEN = 431;
    const int ERR_ERRONEUSNICKNAME = 432;
    const int ERR_NICKNAMEINUSE = 433;
    const int ERR_NOTREGISTERED = 451;
    const int ERR_NEEDMOREPARAMS = 461;
    const int ERR_ALREADYREGISTERED = 462;
    const int ERR_PASSWDMISMATCH = 464;
}

#endif
```

Al convertir los códigos a texto, deben rellenarse con ceros por la izquierda:

```text
1 → 001
```

---

# Reglas arquitectónicas importantes

## Los handlers no deben llamar directamente a `send()`

Los handlers deben limitarse a preparar o solicitar la respuesta:

```text
handleNick()
    ↓
queueNumericReply()
    ↓
outputBuffer
```

La escritura real pertenece al sistema de salida no bloqueante:

```text
poll() detecta POLLOUT
        ↓
send()
        ↓
eliminar únicamente los bytes enviados
```

---

## No duplicar formatos

No se debe construir repetidamente:

```text
":" + serverName + " " + code + " " + nickname
```

Tampoco se debe repetir:

```text
":" + nickname + "!" + username + "@" + hostname
```

Toda esa lógica debe estar centralizada.

---

## Separar construcción y transporte

Construir un mensaje y enviarlo son responsabilidades diferentes:

```text
construcción
    ↓
mensaje IRC completo
    ↓
encolado
    ↓
envío no bloqueante
```

Esta separación facilita:

- Reutilizar formatos.
- Probar las respuestas.
- Evitar mensajes inconsistentes.
- Gestionar envíos parciales.
- Añadir nuevos códigos numéricos.
- Enviar el mismo mensaje a varios clientes.

---

## No cerrar automáticamente por cualquier error

Errores como los siguientes normalmente deben producir una respuesta, no provocar que el servidor termine:

- Comando desconocido.
- Parámetros insuficientes.
- Nickname inválido.
- Nickname ocupado.
- Cliente todavía no registrado.
- Contraseña incorrecta.

El servidor debe continuar funcionando y mantener conectados al resto de clientes.

La desconexión de un cliente debe decidirse de forma explícita según el tipo de error.

---

# Pruebas recomendadas

## Nickname no proporcionado

Entrada:

```text
NICK
```

Salida esperada:

```text
:server.name 431 * :No nickname given
```

---

## Nickname inválido

Entrada:

```text
NICK 123roxana
```

Salida esperada:

```text
:server.name 432 * 123roxana :Erroneous nickname
```

---

## Nickname ocupado

Primer cliente:

```text
NICK roxana
```

Segundo cliente:

```text
NICK roxana
```

Salida esperada para el segundo cliente:

```text
:server.name 433 * roxana :Nickname is already in use
```

---

## Comando sin parámetros suficientes

Entrada:

```text
PRIVMSG
```

Salida esperada:

```text
:server.name 461 roxana PRIVMSG :Not enough parameters
```

---

## Cliente no registrado

Entrada antes de completar `PASS`, `NICK` y `USER`:

```text
PRIVMSG #general :Hola
```

Salida esperada:

```text
:server.name 451 * :You have not registered
```

---

## Contraseña incorrecta

Entrada:

```text
PASS incorrect
```

Salida esperada:

```text
:server.name 464 * :Password incorrect
```

---

## Comando desconocido

Entrada:

```text
TEST something
```

Salida esperada:

```text
:server.name 421 roxana TEST :Unknown command
```

---

## Registro correcto

Entrada:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana Example
```

Salida esperada:

```text
:server.name 001 roxana :Welcome to the IRC Network roxana
```

La respuesta `001` debe enviarse una sola vez.

---

# Resultado esperado de la fase

Al finalizar esta fase, el servidor debe:

- Construir todas las respuestas IRC desde funciones centralizadas.
- Generar correctamente prefijos de servidor y de cliente.
- Terminar todos los mensajes con `\r\n`.
- Formatear los códigos numéricos con tres cifras.
- Utilizar el nickname del cliente o `*` cuando todavía no exista.
- Añadir las respuestas al `outputBuffer`.
- Activar `POLLOUT` cuando existan datos pendientes.
- Evitar llamadas directas a `send()` desde los handlers.
- Enviar la respuesta `001` al completar el registro.
- Responder con errores coherentes ante comandos incorrectos.
- Mantener separadas la construcción, el encolado y la escritura de mensajes.
- Evitar duplicar prefijos y formatos en distintos handlers.