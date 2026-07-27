# Fase 9 — Registro del cliente

## Objetivo

Implementar el proceso mediante el cual una conexión TCP pasa a convertirse en un cliente IRC registrado.

Para completar el registro, el cliente debe haber enviado correctamente:

1. `PASS`
2. `NICK`
3. `USER`

El servidor debe guardar el estado de cada paso y comprobar después de cada comando si el registro ya puede completarse.

---

## Estado necesario en `Client`

Cada cliente debería almacenar, como mínimo:

```cpp
bool passwordAccepted;
bool nicknameReceived;
bool usernameReceived;
bool registered;

std::string nickname;
std::string username;
std::string realName;
```

Cada variable representa una parte independiente del registro:

- `passwordAccepted`: la contraseña enviada mediante `PASS` es correcta.
- `nicknameReceived`: el cliente tiene un nickname válido y disponible.
- `usernameReceived`: el cliente ha enviado correctamente `USER`.
- `registered`: el proceso de registro ya se completó.

No conviene sustituir estos estados por un único booleano como `authenticated`, porque el registro IRC depende de varias condiciones diferentes.

---

## Flujo general del registro

```text
Cliente conectado
    ↓
PASS correcto
    ↓
NICK válido y disponible
    ↓
USER válido
    ↓
tryRegisterClient()
    ↓
Cliente registrado
    ↓
Mensaje de bienvenida
```

Aunque normalmente los comandos se envían en el orden `PASS`, `NICK` y `USER`, el servidor debería comprobar el estado después de cada uno.

La función que completa el registro puede tener una estructura similar a:

```cpp
void Server::tryRegisterClient(Client &client);
```

Debe llamarse después de procesar correctamente:

- `PASS`
- `NICK`
- `USER`

---

# Comando `PASS`

## Formato

```irc
PASS secret
```

## Responsabilidad

El comando `PASS` permite comprobar que el cliente conoce la contraseña con la que se inició el servidor.

## Validaciones necesarias

El handler de `PASS` debe comprobar:

1. Que se ha proporcionado una contraseña.
2. Que el cliente todavía no está registrado.
3. Que la contraseña coincide con la contraseña del servidor.

## Comportamiento esperado

Si no se proporciona ningún parámetro:

```irc
PASS
```

El servidor debe responder:

```irc
461 PASS :Not enough parameters
```

Si el cliente ya está registrado:

```irc
462 :You may not reregister
```

Si la contraseña no coincide:

```irc
464 :Password incorrect
```

Si la contraseña es correcta:

```cpp
client.setPasswordAccepted(true);
```

Después debe comprobarse si el cliente ya puede registrarse:

```cpp
tryRegisterClient(client);
```

## Consideración importante

Una contraseña incorrecta nunca debe marcarse como aceptada:

```cpp
client.setPasswordAccepted(false);
```

El cliente no podrá completar el registro mientras `passwordAccepted` sea `false`.

---

# Comando `NICK`

## Formato

```irc
NICK roxana
```

## Responsabilidad

El comando `NICK` asigna un nickname visible al cliente.

También puede utilizarse después del registro para cambiar el nickname.

## Validaciones necesarias

El handler de `NICK` debe comprobar:

1. Que existe un parámetro.
2. Que el nickname no está vacío.
3. Que su formato es válido.
4. Que no está siendo utilizado por otro cliente.
5. Si el cliente ya tenía nickname, actualizar correctamente el índice global.
6. Si el cliente ya estaba registrado, comunicar el cambio a los clientes relacionados.

## Nickname ausente

Si el cliente envía:

```irc
NICK
```

El servidor debe responder:

```irc
431 :No nickname given
```

## Formato inválido

Si el nickname contiene caracteres no permitidos:

```irc
NICK rox@na
```

El servidor debe responder:

```irc
432 rox@na :Erroneous nickname
```

## Nickname ocupado

Si otro cliente ya utiliza el nickname:

```irc
433 roxana :Nickname is already in use
```

El nickname anterior del cliente no debe modificarse si el nuevo nickname es rechazado.

---

## Validación del nickname

Conviene centralizar esta comprobación:

```cpp
bool Server::isValidNickname(const std::string &nickname) const;
```

Como criterio inicial, se puede exigir:

- Que no esté vacío.
- Que no empiece por un número.
- Que no contenga espacios.
- Que no contenga `:`.
- Que no contenga `,`.
- Que no contenga `*`.
- Que no contenga `?`.
- Que no contenga `!`.
- Que no contenga `@`.
- Que no contenga caracteres de control.

No es necesario implementar toda la especificación histórica de IRC si el subject no lo exige, pero la validación debe ser consistente.

---

# Índice global de nicknames

El servidor necesita localizar rápidamente qué cliente utiliza un nickname.

Una estructura posible es:

```cpp
std::map<std::string, Client *> clientsByNickname;
```

La relación almacenada será:

```text
nickname → Client
```

Por ejemplo:

```text
"roxana" → Client*
"alice"  → Client*
"bob"    → Client*
```

Esto evita recorrer todos los clientes cada vez que se procesa un `NICK`, `PRIVMSG`, `KICK`, `INVITE` u otro comando que necesita buscar usuarios.

---

## Comparación de nicknames

IRC normalmente trata los nicknames sin distinguir entre mayúsculas y minúsculas.

Por ejemplo, estos nombres deberían considerarse equivalentes:

```text
roxana
Roxana
ROXANA
```

Para conseguirlo, se puede generar una clave normalizada:

```cpp
std::string Server::normalizeNickname(
    const std::string &nickname
) const;
```

La clave normalizada se utiliza en el mapa:

```cpp
clientsByNickname[normalizeNickname(nickname)] = &client;
```

El objeto `Client` puede conservar el nickname original para mostrarlo en los mensajes.

---

## Asignación inicial de nickname

Cuando el nickname es válido y está disponible:

1. Se guarda en el cliente.
2. Se añade al índice global.
3. Se marca `nicknameReceived`.
4. Se intenta completar el registro.

Flujo:

```text
Validar nickname
    ↓
Comprobar disponibilidad
    ↓
Guardar nickname
    ↓
Añadirlo a clientsByNickname
    ↓
Marcar nicknameReceived
    ↓
tryRegisterClient()
```

---

## Cambio de nickname

Un cliente registrado puede cambiar su nickname:

```irc
NICK nuevaRoxana
```

La actualización debe realizarse de forma coherente:

1. Guardar temporalmente el nickname anterior.
2. Comprobar que el nuevo nickname es válido.
3. Comprobar que el nuevo nickname está disponible.
4. Eliminar el nickname anterior del índice.
5. Guardar el nuevo nickname en el cliente.
6. Añadir el nuevo nickname al índice.
7. Notificar el cambio a los clientes que comparten canales con él.

El mensaje debe utilizar el nickname anterior en el prefijo:

```irc
:roxana!username@hostname NICK :nuevaRoxana
```

No se debe eliminar el nickname anterior del índice hasta confirmar que el nuevo nickname puede utilizarse.

---

## Limpieza del nickname al desconectar

Cuando un cliente se desconecte, su nickname debe eliminarse del índice global:

```cpp
clientsByNickname.erase(normalizeNickname(client.getNickname()));
```

Si no se elimina, el servidor podría considerar permanentemente ocupado un nickname perteneciente a un cliente que ya no existe.

---

# Comando `USER`

## Formato

```irc
USER roxana 0 * :Roxana Example
```

Sus parámetros representan:

```text
USER <username> <mode> <unused> :<realname>
```

Para este proyecto, normalmente basta con guardar:

- Username.
- Real name.

Los campos intermedios pueden validarse y descartarse, o conservarse si resultan útiles.

---

## Validaciones necesarias

El handler de `USER` debe comprobar:

1. Que el cliente todavía no está registrado.
2. Que existen suficientes parámetros.
3. Que el username no está vacío.
4. Que existe el real name.

Si faltan parámetros:

```irc
461 USER :Not enough parameters
```

Si el cliente ya está registrado:

```irc
462 :You may not reregister
```

---

## Datos que deben guardarse

Para este mensaje:

```irc
USER roxana 0 * :Roxana Example
```

El cliente debería almacenar:

```text
username = "roxana"
realName = "Roxana Example"
```

El parámetro final puede contener espacios porque el parser ya debe haberlo reconstruido como un único parámetro.

Después de almacenar los datos:

```cpp
client.setUsernameReceived(true);
tryRegisterClient(client);
```

---

# Función `tryRegisterClient()`

## Responsabilidad

Esta función centraliza la comprobación de los requisitos del registro.

```cpp
void Server::tryRegisterClient(Client &client);
```

Debe registrar al cliente solamente cuando se cumplan todas estas condiciones:

```text
passwordAccepted == true
nicknameReceived == true
usernameReceived == true
registered == false
```

Una posible lógica es:

```cpp
void Server::tryRegisterClient(Client &client)
{
    if (client.isRegistered())
        return;

    if (!client.isPasswordAccepted())
        return;

    if (!client.hasNickname())
        return;

    if (!client.hasUsername())
        return;

    client.setRegistered(true);
    sendWelcomeMessages(client);
}
```

Esta función debe ser idempotente: llamarla varias veces no debe volver a registrar al cliente ni repetir el mensaje de bienvenida.

---

# Mensaje de bienvenida

Cuando el registro se completa, el servidor debe enviar el mensaje de bienvenida una única vez.

Respuesta mínima:

```irc
:server.name 001 roxana :Welcome to the IRC Network roxana
```

El prefijo completo del cliente también puede incluirse:

```irc
:server.name 001 roxana :Welcome to the IRC Network roxana!username@hostname
```

Conviene centralizar el envío:

```cpp
void Server::sendWelcomeMessages(Client &client);
```

La respuesta debe añadirse al buffer de salida mediante el sistema implementado en las fases anteriores.

Por ejemplo:

```cpp
queueNumericReply(
    client,
    1,
    "Welcome to the IRC Network " + buildClientPrefix(client)
);
```

No se debe llamar directamente a `send()` desde el handler.

---

## Evitar mensajes de bienvenida duplicados

La primera comprobación de `tryRegisterClient()` debe ser:

```cpp
if (client.isRegistered())
    return;
```

Esto evita que el servidor vuelva a enviar `001` si el cliente utiliza posteriormente:

```irc
NICK nuevoNombre
```

Un cambio de nickname después del registro no es un nuevo registro.

---

# Restricción de comandos antes del registro

Antes de completar el registro, el cliente solamente debería poder utilizar los comandos necesarios para conectarse, como:

- `CAP`
- `PASS`
- `NICK`
- `USER`
- `PING`
- `QUIT`

Si intenta ejecutar un comando que requiere registro:

```irc
JOIN #general
```

El servidor debe responder:

```irc
451 :You have not registered
```

Esta comprobación puede centralizarse en el dispatcher o al comienzo de los handlers protegidos.

---

# Separación de responsabilidades

La lógica debería dividirse aproximadamente así:

```text
CommandDispatcher
    ├── identifica PASS, NICK o USER
    └── llama al handler correspondiente

Server
    ├── handlePass()
    ├── handleNick()
    ├── handleUser()
    ├── tryRegisterClient()
    ├── sendWelcomeMessages()
    ├── isValidNickname()
    ├── isNicknameAvailable()
    └── normalizeNickname()

Client
    ├── almacena nickname
    ├── almacena username
    ├── almacena real name
    └── almacena el estado del registro
```

Los handlers deben validar y modificar el estado, pero la decisión final de registrar al cliente debe permanecer centralizada en `tryRegisterClient()`.

---

# Errores numéricos necesarios

| Código | Nombre | Situación |
|---:|---|---|
| `431` | `ERR_NONICKNAMEGIVEN` | `NICK` no contiene nickname |
| `432` | `ERR_ERRONEUSNICKNAME` | El formato del nickname no es válido |
| `433` | `ERR_NICKNAMEINUSE` | El nickname ya está ocupado |
| `451` | `ERR_NOTREGISTERED` | Se utiliza un comando protegido antes del registro |
| `461` | `ERR_NEEDMOREPARAMS` | Faltan parámetros en `PASS` o `USER` |
| `462` | `ERR_ALREADYREGISTERED` | Se intenta repetir `PASS` o `USER` después del registro |
| `464` | `ERR_PASSWDMISMATCH` | La contraseña es incorrecta |
| `001` | `RPL_WELCOME` | El registro se ha completado correctamente |

Estas respuestas deben construirse utilizando el sistema centralizado de mensajes de la fase 8.

---

# Casos de prueba recomendados

## Registro correcto

Entrada:

```irc
PASS secret
NICK roxana
USER roxana 0 * :Roxana Example
```

Resultado esperado:

```irc
:server.name 001 roxana :Welcome to the IRC Network roxana!roxana@hostname
```

El cliente debe quedar marcado como registrado.

---

## Orden diferente de `NICK` y `USER`

Entrada:

```irc
PASS secret
USER roxana 0 * :Roxana Example
NICK roxana
```

Resultado esperado:

- El registro se completa al recibir `NICK`.
- El mensaje `001` se envía una sola vez.

---

## Contraseña incorrecta

Entrada:

```irc
PASS incorrecta
NICK roxana
USER roxana 0 * :Roxana Example
```

Resultado esperado:

```irc
464 :Password incorrect
```

El cliente no debe registrarse.

---

## `PASS` sin parámetro

Entrada:

```irc
PASS
```

Resultado esperado:

```irc
461 PASS :Not enough parameters
```

---

## `NICK` sin parámetro

Entrada:

```irc
NICK
```

Resultado esperado:

```irc
431 :No nickname given
```

---

## Nickname inválido

Entrada:

```irc
NICK rox@na
```

Resultado esperado:

```irc
432 rox@na :Erroneous nickname
```

---

## Nickname ocupado

Primer cliente:

```irc
NICK roxana
```

Segundo cliente:

```irc
NICK roxana
```

Resultado esperado para el segundo cliente:

```irc
433 roxana :Nickname is already in use
```

---

## Comando protegido antes del registro

Entrada:

```irc
JOIN #general
```

Resultado esperado:

```irc
451 :You have not registered
```

---

## Intento de repetir `USER`

Después de completar el registro:

```irc
USER otro 0 * :Otro nombre
```

Resultado esperado:

```irc
462 :You may not reregister
```

Los datos originales del cliente no deben modificarse.

---

## Cambio de nickname después del registro

Entrada:

```irc
NICK nuevaRoxana
```

Resultado esperado:

```irc
:roxana!username@hostname NICK :nuevaRoxana
```

Además:

- El nickname anterior debe quedar disponible.
- El nuevo nickname debe aparecer en el índice global.
- No debe volver a enviarse el mensaje `001`.

---

## Desconexión y liberación del nickname

Después de desconectar a un cliente:

- Su nickname debe eliminarse del índice global.
- Otro cliente debe poder utilizar ese nickname.
- No deben quedar punteros inválidos en `clientsByNickname`.

---

# Resultado esperado de la fase

Al finalizar esta fase, el servidor debe ser capaz de:

- Validar la contraseña del servidor.
- Asignar nicknames válidos y únicos.
- Guardar el username y el real name.
- Mantener un índice global de nicknames.
- Detectar cuándo se cumplen todos los requisitos del registro.
- Registrar al cliente una sola vez.
- Enviar correctamente la respuesta `001`.
- Rechazar comandos protegidos antes del registro.
- Permitir cambios de nickname después del registro.
- Liberar el nickname cuando el cliente se desconecta.
- Responder con los errores numéricos apropiados.