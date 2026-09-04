# Guía de comprobación

Este documento explica cómo verificar que `ircserv` cumple las especificaciones del enunciado (resumidas en `README.md`): varios clientes simultáneos, I/O no bloqueante, un único `poll()`, registro IRC, canales, mensajes privados y comandos de operador.

La comprobación combina **tests automáticos** (`make test`) y **pruebas manuales** con netcat y el cliente de referencia (**irssi**).

## 1. Compilar y arrancar el servidor

Desde la raíz del repositorio:

```bash
make
./ircserv 6667 secret
```
Uso:

```text
./ircserv <port> <password>
```

- El puerto debe ser un entero entre `1` y `65535`.
- La contraseña no puede estar vacía.
- Con un número de argumentos distinto de 2, el programa imprime `Usage` y termina.

Para ver las reglas del Makefile:

```bash
make help
```

El binario se compila con AddressSanitizer (`-fsanitize=address`). Si el servidor aborta durante las pruebas, revisar primero el informe de sanitizer.

Detener el servidor: `Ctrl+C` (SIGINT) o `SIGTERM`. Debe cerrar los descriptores y terminar de forma limpia.

---

## 2. Tests automáticos

Los tests de protocolo arrancan `./ircserv` en un puerto libre, con contraseña `secret`, y hablan IRC por TCP. Hay que ejecutarlos **desde la raíz del repositorio**, donde está el binario.

```bash
make test
```

Esa regla ejecuta, en este orden:

| Objetivo | Qué comprueba |
|---|---|
| `make test-parser` | Parser de mensajes IRC (comando en mayúsculas, trailing con espacios, `:` vacío). |
| `make test-message` | Serialización de `IrcMessage` y casemap RFC 1459. |
| `make test-protocol` | Checklist de protocolo (registro, PING, CAP, PRIVMSG, JOIN/PART) más las suites independientes. |
| `make test-channel` | Modelo `Channel`: miembros, operadores, invitaciones, modos, topic y clave. |

Si todo va bien, cada suite imprime un mensaje de éxito y el proceso termina con código 0. Un fallo imprime `expected` / `actual` y sale con código distinto de 0.

### Suites de protocolo (mapeo al enunciado)

`make test-protocol` construye `ircserv` si hace falta y lanza `ProtocolTests.cpp` más todos los `src/tests/Protocol*Tests.cpp` independientes.

| Suite | Requisito del enunciado |
|---|---|
| `ProtocolTests.cpp` | Autenticación (`PASS`/`NICK`/`USER`), bienvenida, `JOIN`, `PRIVMSG` de canal y privado, `PING`/`PONG`, `CAP`, `QUIT`. |
| `ProtocolFramingTests.cpp` | Reensamblado de comandos partidos (el test de `nc` + `Ctrl+D` del enunciado). |
| `ProtocolMultiClientTests.cpp` | Varios clientes a la vez: registro, canal, privado, operadores. |
| `ProtocolPartialWriteTests.cpp` | Escrituras TCP parciales (cliente lento); el servidor no se bloquea. |
| `ProtocolKickCommandTests.cpp` / `ProtocolKickErrorTests.cpp` | `KICK`. |
| `ProtocolInviteCommandTests.cpp` / `ProtocolInviteErrorTests.cpp` / `ProtocolInviteJoinTests.cpp` | `INVITE` y canal `+i`. |
| `ProtocolTopicQueryTests.cpp` / `ProtocolTopicSetTests.cpp` / `ProtocolTopicErrorTests.cpp` | `TOPIC` y modo `+t`. |
| `ProtocolModeFlagTests.cpp` | Modos `i` y `t`. |
| `ProtocolModeKeyLimitTests.cpp` | Modos `k` y `l`. |
| `ProtocolModeOperatorTests.cpp` | Modo `o` (dar/quitar operador). |
| `ProtocolModeQueryTests.cpp` / `ProtocolModeCombinationTests.cpp` / `ProtocolModeErrorTests.cpp` | Consulta, combinaciones y errores de `MODE`. |
| `ProtocolDisconnectTests.cpp` / `ProtocolChannelEdgeTests.cpp` / `ProtocolOversizedInputTests.cpp` / `ProtocolErrorRobustnessTests.cpp` | Desconexiones, último miembro, líneas > 512 bytes, numerics de error. |

Las suites independientes se descubren solas: un archivo nuevo `src/tests/Protocol*Tests.cpp` (salvo `ProtocolTests.cpp`) entra en `make test-protocol` sin tocar el Makefile.

---

## 3. Test de framing del enunciado (netcat)

El enunciado pide comprobar que el servidor **agrega fragmentos TCP** antes de parsear un comando. Con el servidor en marcha:

```bash
nc -C 127.0.0.1 6667
```

Escribir `com`, pulsar `Ctrl+D`, escribir `man`, pulsar `Ctrl+D`, escribir `d` y pulsar `Enter`. El servidor debe reconstruir `command` (más el salto de línea) y no tratar cada fragmento como un comando distinto.

Comprobación equivalente, más explícita:


```bash
# Tres fragmentos: "NICK", " ali", "ce\n"
printf 'NICK' >&0
# Ctrl+D en la sesión nc, o:
python3 - <<'PY'
import socket, time
s = socket.create_connection(("127.0.0.1", 6667))
s.send(b"PASS secret\r\n")
time.sleep(0.05)
s.send(b"NICK")
time.sleep(0.05)
s.send(b" ali")
time.sleep(0.05)
s.send(b"ce\r\nUSER alice 0 * :Alice\r\n")
print(s.recv(4096).decode("utf-8", "replace"))
s.close()
PY
```

El cliente no debe registrarse hasta que llegue la línea completa. `ProtocolFramingTests.cpp` cubre este caso de forma automática.

---

## 4. Cliente de referencia: irssi

El enunciado exige un cliente de referencia que se conecte **sin error** y se comporte de forma similar a un servidor IRC real. En este proyecto el cliente de referencia es **irssi**.

```bash
sudo apt install irssi
./ircserv 6667 secret
```

En otra terminal:

```bash
irssi
```

Dentro de irssi:

```text
/connect 127.0.0.1 6667 secret
```

Si el nick por defecto está ocupado:


```text
/nick alice
```

Irssi envía `CAP LS` al conectar. El servidor debe responder (`CAP * LS`) y **no desconectar**. A continuación irssi envía `PASS`, `NICK` y `USER`. Debe aparecer la bienvenida (numeric `001` y siguientes).

### Segunda sesión (mensajes y canal)

```bash
irssi
```

```text
/set nick bob
/connect 127.0.0.1 6667 secret
```

Comprobar, como pide el enunciado:

| Acción | Comando irssi | Resultado esperado |
|---|---|---|
| Unirse a un canal | `/join #general` | El primer usuario es operador (`@`). El resto ve el `JOIN`. |
| Mensaje de canal | escribir en `#general` | Todos los miembros reciben el texto. El emisor no tiene por qué verse duplicado de forma anómala. |
| Mensaje privado | `/msg bob hola` | Solo `bob` lo recibe. |
| Ver/cambiar topic | `/topic Hola mundo` | Los miembros ven el topic nuevo. |
| Modos de canal | `/mode #general +i` | `JOIN` sin invitación falla. |
| Invitar | `/invite bob #general` | `bob` recibe `INVITE` y puede entrar con `+i`. |
| Kick | `/kick bob motivo` | `bob` sale del canal; el resto ve `KICK`. |
| Operador | `/mode #general +o bob` | `bob` pasa a operador. `/mode #general -o bob` se lo quita. |
| Clave | `/mode #general +k clave` | `JOIN` exige la clave. |
| Límite | `/mode #general +l 1` | Un tercer usuario no puede entrar. |

Salir:

```text
/quit
```

El resto de miembros del canal debe ver `QUIT`. El servidor sigue aceptando conexiones.

HexChat u otro cliente gráfico es opcional: sirve para confirmar interoperabilidad, pero la evaluación se centra en irssi.

---

## 5. Pruebas manuales con netcat (comandos IRC)

Útil para ver numerics exactos sin la capa de irssi. Terminal 1: el servidor. Terminales 2 y 3: clientes.

Cliente A:

```text
PASS secret
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :hola canal
PRIVMSG bob :hola privado
```

- Debe recibir el `JOIN` de `alice` (si `alice` entra después) o ver a `alice` en `NAMES`.
- Debe recibir `PRIVMSG #general :hola canal`.
- Debe recibir el privado `hola privado`.
- `alice` no debe recibir su propio privado dirigido a `bob`.

Cliente B (después de registrarse como `bob` y hacer `JOIN #general`):

```text
PASS secret
NICK bob
USER bob 0 * :Bob
JOIN #general
```

Registro incompleto o contraseña mala:

```text
PASS wrong
NICK alice
USER alice 0 * :Alice
```

No debe enviarse `001`. Contraseña incorrecta → `464`. Faltan parámetros → `461`. Comando desconocido → `421`.

Operadores de canal (cliente A es `@` por haber creado `#general`):

```text
MODE #general +i
INVITE bob #general
TOPIC #general :tema
KICK #general bob :fuera
MODE #general +o bob
MODE #general +t
MODE #general +k secretkey
MODE #general +l 5
MODE #general -i-t-k-l-o bob
```

---

## 6. Lista de comprobación del enunciado

Marcar cada punto durante la evaluación o antes de entregar.

- [ ] El programa se llama `ircserv` y se arranca con `port` y `password`.
- [ ] `make` / `make all` / `clean` / `fclean` / `re` funcionan.
- [ ] Varios clientes se atienden a la vez sin que el servidor se quede colgado.
- [ ] No se usa `fork` para clientes. Toda la I/O es no bloqueante (`fcntl`).
- [ ] Un único `poll()` (o equivalente) cubre listen, lectura y escritura.

### Cliente de referencia

- [ ] irssi conecta a `ircserv` sin error.
- [ ] El flujo se parece al de un servidor IRC habitual (bienvenida, canales, mensajes).

### Funcionalidad obligatoria

- [ ] Autenticación con la contraseña del servidor (`PASS`).
- [ ] Nickname (`NICK`) y username (`USER`).
- [ ] `JOIN` a un canal.
- [ ] Mensajes privados (`PRIVMSG` a un nick).
- [ ] Un `PRIVMSG` a un canal llega a **todos** los demás miembros.
- [ ] Hay operadores de canal y usuarios normales.
- [ ] `KICK` expulsa a un cliente del canal.
- [ ] `INVITE` invita a un cliente a un canal.
- [ ] `TOPIC` consulta o cambia el topic.
- [ ] `MODE +i` / `-i`: canal solo por invitación.
- [ ] `MODE +t` / `-t`: solo operadores pueden cambiar el topic.
- [ ] `MODE +k` / `-k`: clave del canal.
- [ ] `MODE +o` / `-o`: dar o quitar privilegio de operador.
- [ ] `MODE +l` / `-l`: límite de usuarios.

### Robustez (ejemplo del enunciado)

- [ ] Un comando partido en varios `recv` se reensambla (`nc` + `Ctrl+D` o `make test-protocol`).
- [ ] Tras un cliente problemático (desconexión brusca, línea enorme, comando inválido), el servidor sigue vivo.

---

## 7. Varios clientes y límites

El enunciado exige no colgarse con varios clientes. Comprobaciones rápidas:

```bash
# Servidor con pocos descriptores (el proceso no debe abortar de forma sucia)
bash -c 'ulimit -n 16; exec ./ircserv 6667 secret'
```

En otra terminal, muchas conexiones:

```bash
python3 -c 'import socket,time; c=[socket.create_connection(("127.0.0.1",6667)) for _ in range(30)]; print(len(c),"clients"); time.sleep(10)'
```

Si el límite de fds se agota, el servidor debe seguir el bucle de `poll` con los clientes que ya tenía; no debe bloquearse.

Scripts auxiliares en `docs/utils/`:

| Script | Uso |
|---|---|
| `docs/utils/force_close_tcp.py` | Cierre TCP con `SO_LINGER` (RST). El servidor debe limpiar al cliente. |
| `docs/utils/search_errors.py` | Abre y cierra 1000 conexiones seguidas. |
| `docs/utils/monitoring.sh` | Estadísticas del proceso `ircserv` (CPU, memoria, fds). |
| `docs/utils/commands.md` | Cómo localizar `IP:PORT` con `ss`. |

Ver también `docs/slow_client.md` (buffer de salida y cliente lento) y `docs/phases/phase_19.md` (framing, desconexiones, líneas oversized).

---

## 8. Memoria y descriptores

La build por defecto lleva AddressSanitizer. `make test` ya detecta muchos accesos inválidos.

Para fugas y file descriptors, **recompilar sin sanitizer** (ASan y Valgrind no se combinan bien):

```bash
make fclean
# Compilar temporalmente sin -fsanitize=address, o usar una build de depuración equivalente
valgrind --leak-check=full --track-fds=yes ./ircserv 6667 secret
```

Conectar y desconectar varios clientes (irssi o netcat), unirse a canales y salir. Al parar el servidor, Valgrind no debe reportar leaks de clientes ni sockets sin cerrar (salvo descriptores estándar).

---

## 9. Parte bonus (si está implementada)

El enunciado marca como bonus:

- Transferencia de archivos (habitualmente DCC sobre `PRIVMSG` CTCP; el servidor retransmite el mensaje).
- Un bot.

Y como comando extra: `LIST`.

No forman parte de la checklist obligatoria. Si existen, probarlos con irssi (`/dcc send`, mensaje al bot, `/list`) además de `make test`.

---

## 10. Orden de trabajo recomendado

1. `make test` — si falla, el protocolo o el framing no están listos.
2. Arrancar `./ircserv 6667 secret` e irssi: conectar, `JOIN`, mensajes de canal y privados.
3. Segunda sesión irssi: operadores (`KICK`, `INVITE`, `TOPIC`, `MODE i/t/k/o/l`).
4. Test de `nc` + `Ctrl+D` del enunciado.
5. Desconexiones bruscas y varios clientes (`docs/utils/`).
6. Recorrer la lista de la sección 6.

---

## 11. Más tests

1. Comprobar que los mensajes se envían a los clientes después de que se unan a un canal.

2. Comprobar que los usuarios regulares no pueden usar comandos que solo están disponibles para operadores.

3. Comprobar que los operadores pueden usar todos los comandos en todos los canales que han creado.

4. Cliente A se une a un canal y envía un mensaje al canal. Cliente B se une al canal y debe recibir el mensaje.

5. Cliente A se une a un canal, actualiza el canal para que sea de invitación. Cliente B intenta unirse al canal y no puede.

6. Cliente A se une a un canal, actualiza su contraseña. Cliente B intenta unirse al canal sin contraseña y no puede.

7. Cliente A se une a un canal, actualiza el límite de usuarios del canal. Cliente B intenta unirse al canal y no puede.