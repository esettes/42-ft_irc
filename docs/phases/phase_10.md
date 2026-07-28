# Fase 10 — Comandos auxiliares de conexión

## Objetivo

Implementar los comandos auxiliares necesarios para mantener, negociar y cerrar correctamente una conexión IRC:

- `PING` / `PONG`
- `QUIT`
- `CAP`

Estos comandos facilitan que clientes IRC reales, como Irssi, puedan conectarse y funcionar correctamente.

---

## 1. Comando `PING`

Los clientes utilizan `PING` para comprobar que la conexión continúa activa.

Ejemplo recibido:

```irc
PING :token
```

El servidor debe responder utilizando el mismo token:

```irc
PONG :token
```

### Comprobaciones necesarias

- Verificar que se ha recibido un parámetro.
- Conservar el token recibido.
- Permitir `PING` antes de completar el registro.
- Añadir la respuesta al buffer de salida.
- Activar `POLLOUT` para enviar la respuesta de forma no bloqueante.

Si falta el parámetro, puede enviarse:

```irc
:server.name 409 nickname :No origin specified
```

No debe llamarse directamente a `send()` desde el handler si el servidor ya dispone de un sistema centralizado de escritura.

---

## 2. Comando `PONG`

El servidor debe generar la respuesta mediante un handler similar a:

```cpp
void Server::handlePing(Client &client, const Command &command);
```

El flujo debe ser:

```text
PING recibido
    ↓
construir PONG
    ↓
añadirlo al outputBuffer
    ↓
activar POLLOUT
    ↓
enviar los datos pendientes
```

---

## 3. Comando `QUIT`

`QUIT` permite que un cliente cierre voluntariamente su conexión.

Ejemplos:

```irc
QUIT
```

```irc
QUIT :Leaving
```

Si no se proporciona un motivo, puede utilizarse uno predeterminado:

```text
Client Quit
```

### Notificación de salida

Los usuarios que compartan algún canal con el cliente deben recibir:

```irc
:nickname!username@hostname QUIT :Leaving
```

Cada usuario debe recibir la notificación una sola vez, aunque comparta varios canales con el cliente.

Para evitar duplicados puede utilizarse:

```cpp
std::set<Client *> recipients;
```

---

## 4. Limpieza completa del cliente

Después de recibir `QUIT`, debe realizarse la limpieza en este orden:

1. Guardar el prefijo y el motivo de salida.
2. Obtener los destinatarios de la notificación.
3. Enviar el mensaje `QUIT`.
4. Eliminar al cliente de todos sus canales.
5. Eliminarlo de las listas de operadores.
6. Eliminarlo de las listas de invitados.
7. Eliminar los canales que hayan quedado vacíos.
8. Eliminar su nickname del índice global.
9. Eliminar su descriptor de la estructura utilizada por `poll()`.
10. Cerrar el descriptor.
11. Destruir o eliminar el objeto `Client`.

Si existe un índice global como:

```cpp
std::map<std::string, Client *> clientsByNickname;
```

debe eliminarse la entrada correspondiente. De lo contrario, el nickname permanecería ocupado después de la desconexión.

---

## 5. Centralización de las desconexiones

La misma limpieza será necesaria cuando:

- El cliente envíe `QUIT`.
- `recv()` devuelva `0`.
- Se produzca un error fatal de lectura.
- Se produzca un error fatal de escritura.
- El cliente pierda la conexión.
- El servidor expulse al cliente.

Conviene centralizar el proceso:

```cpp
void Server::disconnectClient(int clientFileDescriptor,
                              const std::string &reason);
```

El handler de `QUIT` solamente debería obtener el motivo y solicitar la desconexión:

```cpp
void Server::handleQuit(Client &client, const Command &command);
```

Esto evita duplicar la lógica de limpieza.

---

## 6. Evitar invalidar iteradores

No se debe eliminar al cliente de una colección mientras se recorre esa misma colección si la operación puede invalidar los iteradores.

Una estrategia segura consiste en:

1. Copiar la lista de canales del cliente.
2. Recorrer la copia.
3. Eliminar al cliente de cada canal.
4. Eliminar posteriormente los canales vacíos.

Tampoco debe accederse al objeto `Client` después de eliminarlo de la colección principal o destruirlo.

---

## 7. Comando `CAP`

`CAP` se utiliza para negociar capacidades entre el cliente y el servidor.

Un cliente real puede enviar:

```irc
CAP LS 302
```

Aunque el servidor no implemente capacidades adicionales, debe responder para que el cliente no se quede esperando.

Una respuesta mínima sería:

```irc
:server.name CAP * LS :
```

El cliente puede finalizar la negociación enviando:

```irc
CAP END
```

### Subcomandos mínimos

#### `CAP LS`

Informa de las capacidades disponibles:

```irc
:server.name CAP * LS :
```

#### `CAP END`

Finaliza la negociación de capacidades.

No necesita producir una respuesta, pero debe permitir que el registro continúe.

---

## 8. Estado de negociación de capacidades

Cada cliente puede almacenar:

```cpp
bool capNegotiationActive;
```

Al recibir:

```irc
CAP LS 302
```

se activa la negociación:

```text
capNegotiationActive = true
```

Al recibir:

```irc
CAP END
```

se finaliza:

```text
capNegotiationActive = false
```

Después de `CAP END`, debe comprobarse nuevamente el registro:

```cpp
tryRegisterClient(client);
```

Si la negociación `CAP` bloquea temporalmente el registro, los requisitos serán:

```text
contraseña aceptada
    +
nickname válido y disponible
    +
USER recibido
    +
negociación CAP finalizada
```

También puede implementarse una versión más sencilla en la que `CAP` no bloquee el registro, pero el servidor debe responder como mínimo a `CAP LS`.

---

## 9. Registro de los handlers

Los comandos deben añadirse al sistema de despacho:

```text
PING → handlePing()
QUIT → handleQuit()
CAP  → handleCap()
```

Los siguientes comandos deben permitirse antes de que el cliente complete el registro:

- `PING`
- `QUIT`
- `CAP`

Por tanto, no deben responder con:

```irc
451 ERR_NOTREGISTERED
```

---

## 10. Pruebas recomendadas

### Probar `PING`

Entrada:

```irc
PING :12345
```

Resultado esperado:

```irc
PONG :12345
```

### Probar `PING` sin parámetro

Entrada:

```irc
PING
```

Resultado esperado:

```irc
:server.name 409 nickname :No origin specified
```

### Probar `QUIT`

Entrada:

```irc
QUIT :Goodbye
```

Resultado esperado:

- Los usuarios relacionados reciben el mensaje `QUIT`.
- El cliente desaparece de todos sus canales.
- El nickname vuelve a quedar disponible.
- El descriptor desaparece de `poll()`.
- La conexión se cierra correctamente.
- El objeto `Client` deja de estar almacenado en el servidor.

### Probar `CAP`

Entrada:

```irc
CAP LS 302
```

Resultado esperado:

```irc
:server.name CAP * LS :
```

Después:

```irc
CAP END
```

El cliente debe poder completar el registro normalmente.

---

## Resultado esperado

Al terminar esta fase, el servidor debe ser capaz de:

- Mantener conexiones activas mediante `PING` y `PONG`.
- Procesar cierres voluntarios mediante `QUIT`.
- Notificar la salida a los usuarios afectados.
- Evitar notificaciones duplicadas.
- Eliminar todas las referencias de un cliente desconectado.
- Liberar correctamente su nickname.
- Eliminar canales vacíos.
- Responder mínimamente a la negociación `CAP`.
- Permitir la conexión de clientes IRC reales.
- Reutilizar el mismo proceso para desconexiones voluntarias e inesperadas.