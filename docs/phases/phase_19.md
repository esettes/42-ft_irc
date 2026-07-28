# Fase 19 — Robustez y tests adversos

## Objetivo

En esta fase hay que comprobar que el servidor IRC se comporta correctamente ante entradas incompletas, comandos inválidos, escrituras parciales, desconexiones inesperadas y estados límite.

No se añaden nuevas funcionalidades principales. El objetivo es detectar:

- Pérdidas de datos.
- Estados inconsistentes.
- Accesos a memoria inválidos.
- Fugas de memoria.
- Descriptores sin cerrar.
- Errores provocados por el funcionamiento real de TCP.

---

## 1. Framing TCP

TCP transmite un flujo continuo de bytes. Una llamada a `recv()` no garantiza recibir un comando IRC completo.

Un comando puede llegar dividido en varios fragmentos:

```text
"PRIV"
"MSG #general :ho"
"la\r"
"\n"
```

El servidor debe acumularlos hasta poder reconstruir:

```text
PRIVMSG #general :hola
```

### Comportamiento necesario

Para cada cliente:

1. Añadir los bytes recibidos a su buffer de entrada.
2. Buscar comandos completos terminados en `\n`.
3. Extraer y procesar únicamente los comandos completos.
4. Mantener en el buffer cualquier fragmento incompleto.
5. Continuar procesando mientras queden líneas completas.
6. Eliminar el `\r` situado antes del `\n`, cuando exista.

Un fragmento incompleto nunca debe enviarse directamente al parser.

---

## 2. Varios comandos en una misma recepción

Una sola llamada a `recv()` también puede devolver varios comandos completos:

```text
"NICK one\r\nUSER one 0 * :One\r\nJOIN #a\r\n"
```

El servidor debe separar y procesar individualmente:

```text
NICK one
USER one 0 * :One
JOIN #a
```

No debe procesar todo el contenido como si fuese un único comando.

---

## 3. Terminadores de línea

Hay que probar los dos terminadores siguientes:

```text
\r\n
\n
```

El servidor puede ser tolerante y aceptar `\n` como final de comando.

Sin embargo, todos los mensajes enviados por el servidor deben finalizar obligatoriamente con:

```text
\r\n
```

Ejemplo:

```text
:irc.server 001 roxana :Welcome to the IRC Network\r\n
```

---

## 4. Escrituras parciales

Una llamada a `send()` puede enviar menos bytes de los solicitados, especialmente cuando:

- El socket es no bloqueante.
- El buffer del sistema está lleno.
- Se intenta enviar una gran cantidad de información.
- El cliente recibe datos lentamente.

Ejemplo conceptual:

```cpp
const ssize_t bytesSent = send(
    clientFileDescriptor,
    outputBuffer.data(),
    outputBuffer.size(),
    0
);
```

Si `bytesSent` es menor que `outputBuffer.size()`, no se debe eliminar todo el mensaje.

Solo deben retirarse del buffer los bytes enviados correctamente:

```cpp
outputBuffer.erase(0, bytesSent);
```

Los bytes restantes deben mantenerse para intentar enviarlos de nuevo cuando `poll()` indique que el socket está preparado para escritura mediante `POLLOUT`.

### Casos que deben gestionarse

- `send()` devuelve un número positivo menor que el tamaño solicitado.
- `send()` devuelve `-1` con `errno == EAGAIN`.
- `send()` devuelve `-1` con `errno == EWOULDBLOCK`.
- `send()` devuelve `-1` con `errno == EINTR`.
- `send()` devuelve un error definitivo.
- El cliente se desconecta mientras todavía tiene mensajes pendientes.

### Resultado esperado

- No se pierden bytes.
- No se duplican fragmentos.
- Se mantiene el orden de los mensajes.
- Los mensajes pendientes se eliminan al destruir al cliente.
- `POLLOUT` solo se solicita mientras existan datos pendientes.

---

## 5. Errores de protocolo

Hay que probar comandos incompletos, parámetros inválidos y recursos inexistentes.

### Registro

```text
PASS
PASS incorrectPassword
NICK
NICK existingNick
USER
USER roxana
```

### Canales

```text
JOIN
JOIN invalid
JOIN #missingKey
PART
PART #nonexistent
```

### Mensajes

```text
PRIVMSG
PRIVMSG roxana
PRIVMSG nobody :hello
PRIVMSG #nonexistent :hello
```

### Comandos de operador

```text
MODE
MODE #channel
MODE #channel +k
MODE #channel +l
MODE #channel +o
MODE #channel +o nobody
KICK
KICK #channel
KICK #channel nobody
INVITE
INVITE nobody #channel
TOPIC
TOPIC #nonexistent
```

### Comportamiento esperado

Para cada comando inválido, el servidor debe:

- No cerrarse inesperadamente.
- No acceder a parámetros inexistentes.
- No modificar el estado parcialmente.
- Enviar una respuesta numérica coherente.
- Mantener al resto de clientes funcionando.
- Permitir que el cliente continúe enviando comandos.

---

## 6. Desconexiones problemáticas

Hay que probar desconexiones en diferentes estados.

### Casos de prueba

- Desconectar un cliente no registrado.
- Desconectar un usuario registrado sin canales.
- Desconectar un usuario presente en varios canales.
- Desconectar a un operador.
- Desconectar al único operador de un canal.
- Desconectar al último miembro de un canal.
- Desconectar un usuario que aparece en listas de invitados.
- Desconectar un cliente con mensajes pendientes.
- Recibir `POLLHUP`.
- Recibir `POLLERR`.
- Recibir `POLLNVAL`.
- Recibir `recv() == 0`.
- Recibir un error definitivo de `recv()`.
- Ejecutar el comando `QUIT`.
- Cerrar el programa mientras existen clientes conectados.

### Comportamiento esperado

Toda desconexión debe pasar por una única función:

```cpp
void Server::disconnectClient(
    int clientFileDescriptor,
    const std::string &reason
);
```

Esta función debe encargarse de:

1. Notificar el `QUIT` a los usuarios afectados.
2. Eliminar al cliente de todos los canales.
3. Eliminarlo de las colecciones de operadores.
4. Eliminarlo de las listas de invitados.
5. Eliminar su nickname del índice global.
6. Eliminar su descriptor de `poll()`.
7. Cerrar el descriptor.
8. Eliminar el objeto `Client`.
9. Eliminar los canales que hayan quedado vacíos.

La función debe evitar enviar varias veces el mismo `QUIT` a usuarios que compartían más de un canal con el cliente desconectado.

---

## 7. Casos límite relacionados con canales

También hay que comprobar:

- El primer usuario crea el canal y se convierte en operador.
- Un canal se elimina cuando sale su último miembro.
- Un operador expulsado deja de aparecer en `operators`.
- Un usuario invitado que se desconecta deja de aparecer en `invited`.
- Un usuario no puede estar duplicado en `members`.
- Un usuario no puede ser operador sin pertenecer al canal.
- El límite `+l` se respeta exactamente.
- La clave `+k` se comprueba correctamente.
- El modo `+i` permite entrar a usuarios invitados.
- La invitación se consume después de un `JOIN` correcto.
- `KICK`, `PART`, `QUIT` y una desconexión inesperada mantienen el mismo estado final.

---

## 8. Protección frente a mensajes demasiado grandes

Conviene limitar el tamaño del buffer de entrada de cada cliente.

Si un cliente envía datos indefinidamente sin ningún terminador, el buffer no debe crecer sin límite.

Se debe definir un tamaño máximo razonable y:

- Rechazar mensajes excesivamente grandes.
- Limpiar el estado correspondiente.
- Desconectar al cliente si está enviando datos inválidos de forma continuada.
- Evitar un consumo ilimitado de memoria.

El protocolo IRC tradicional limita cada mensaje a `512` bytes incluyendo `\r\n`, aunque el comportamiento exacto puede adaptarse a los requisitos del proyecto.

---

## 9. Pruebas con varios clientes

Las pruebas no deben realizarse únicamente con un cliente.

Hay que conectar varios clientes simultáneamente y comprobar:

- Registro independiente.
- Nicknames únicos.
- Entrada simultánea en canales.
- Mensajes privados.
- Mensajes a canales.
- Cambios de topic.
- Invitaciones.
- Expulsiones.
- Modos de canal.
- Desconexiones inesperadas.
- Cierre de un cliente mientras otro continúa conectado.

Se pueden abrir varias terminales:

```bash
nc 127.0.0.1 6667
```

También conviene probar el servidor con el cliente de referencia elegido, por ejemplo `irssi`.

---

## 10. Comprobación de memoria y descriptores

Ejecutad el servidor con Valgrind:

```bash
valgrind --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --track-fds=yes \
    ./ircserv 6667 secret
```

Durante la ejecución:

1. Conectad varios clientes.
2. Registradlos.
3. Cread varios canales.
4. Enviad mensajes.
5. Ejecutad `JOIN`, `PART`, `KICK`, `INVITE`, `TOPIC` y `MODE`.
6. Desconectad clientes de distintas formas.
7. Cerrad el servidor.

### Resultado esperado

Al finalizar no deberían quedar:

- Bloques de memoria definitivamente perdidos.
- Clientes sin destruir.
- Canales sin destruir.
- Buffers pendientes sin liberar.
- Descriptores de clientes abiertos.
- El descriptor del socket de escucha abierto.
- Accesos a memoria liberada.
- Lecturas o escrituras fuera de los límites.

---

## 11. Pruebas con sanitizers

Si el entorno lo permite, también conviene compilar temporalmente con sanitizers:

```bash
-fsanitize=address,undefined -g3
```

Ejemplo:

```bash
c++ -Wall -Wextra -Werror -std=c++98 \
    -fsanitize=address,undefined \
    -g3 \
    src/*.cpp \
    -o ircserv
```

Estas herramientas ayudan a detectar:

- Desbordamientos de buffer.
- Uso de memoria después de liberarla.
- Accesos fuera de rango.
- Dobles liberaciones.
- Comportamiento indefinido.

Estas opciones son para depuración y no tienen por qué formar parte de la compilación final del proyecto.

---

## 12. Lista final de comprobación

Antes de dar la fase por terminada, verificad que:

- [ ] Los comandos fragmentados se reconstruyen correctamente.
- [ ] Los comandos agrupados se separan correctamente.
- [ ] Se aceptan correctamente los terminadores elegidos.
- [ ] Todas las respuestas usan `\r\n`.
- [ ] Las escrituras parciales conservan los bytes pendientes.
- [ ] `EAGAIN`, `EWOULDBLOCK` y `EINTR` se gestionan correctamente.
- [ ] Los comandos inválidos no provocan cierres inesperados.
- [ ] Los errores producen respuestas numéricas coherentes.
- [ ] Todas las desconexiones pasan por una única función.
- [ ] No quedan clientes en canales después de desconectarse.
- [ ] No quedan operadores que ya no sean miembros.
- [ ] No quedan invitaciones a clientes inexistentes.
- [ ] Los canales vacíos se eliminan.
- [ ] No se envía el mismo `QUIT` varias veces al mismo usuario.
- [ ] Los buffers tienen un tamaño máximo.
- [ ] El servidor funciona con varios clientes simultáneos.
- [ ] Valgrind no detecta fugas de memoria.
- [ ] Valgrind no detecta descriptores abiertos.
- [ ] Los sanitizers no detectan accesos inválidos.
- [ ] El servidor sigue funcionando después de recibir entradas problemáticas.

---

## Resultado de la fase

Al terminar esta fase, el servidor debe ser capaz de soportar tráfico TCP real, entradas malformadas, varios clientes simultáneos y desconexiones inesperadas sin perder datos, dejar estados inconsistentes ni producir errores de memoria.