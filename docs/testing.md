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
| `ProtocolChannelHistoryTests.cpp` | Historial de `PRIVMSG` de canal reenviado a quien hace `JOIN` después. |
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
# Tres fragmentos: "NICK", " ro", "xy\n"
printf 'NICK' >&0
# Ctrl+D en la sesión nc, o:
python3 - <<'PY'
import socket, time
s = socket.create_connection(("127.0.0.1", 6667))
s.send(b"PASS secret\r\n")
time.sleep(0.05)
s.send(b"NICK")
time.sleep(0.05)
s.send(b" ro")
time.sleep(0.05)
s.send(b"xy\r\nUSER roxy 0 * :Roxy\r\n")
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
/nick roxy
```

Irssi envía `CAP LS` al conectar. El servidor debe responder (`CAP * LS`) y **no desconectar**. A continuación irssi envía `PASS`, `NICK` y `USER`. Debe aparecer la bienvenida (numeric `001` y siguientes).

### Segunda sesión (mensajes y canal)

```bash
irssi
```

```text
/set nick dani
/connect 127.0.0.1 6667 secret
```

Comprobar, como pide el enunciado:

| Acción | Comando irssi | Resultado esperado |
|---|---|---|
| Unirse a un canal | `/join #general` | El primer usuario es operador (`@`). El resto ve el `JOIN`. |
| Mensaje de canal | escribir en `#general` | Todos los miembros reciben el texto. El emisor no tiene por qué verse duplicado de forma anómala. |
| Mensaje privado | `/msg dani hola` | Solo `dani` lo recibe. |
| Ver/cambiar topic | `/topic Hola mundo` | Los miembros ven el topic nuevo. |
| Modos de canal | `/mode #general +i` | `JOIN` sin invitación falla. |
| Invitar | `/invite dani #general` | `dani` recibe `INVITE` y puede entrar con `+i`. |
| Kick | `/kick dani motivo` | `dani` sale del canal; el resto ve `KICK`. |
| Operador | `/mode #general +o dani` | `dani` pasa a operador. `/mode #general -o dani` se lo quita. |
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
NICK roxy
USER roxy 0 * :Roxy
JOIN #general
PRIVMSG #general :hola canal
PRIVMSG dani :hola privado
```

- Debe recibir el `JOIN` de `roxy` (si `roxy` entra después) o ver a `roxy` en `NAMES`.
- Debe recibir `PRIVMSG #general :hola canal`.
- Debe recibir el privado `hola privado`.
- `roxy` no debe recibir su propio privado dirigido a `dani`.

Cliente B (después de registrarse como `dani` y hacer `JOIN #general`):

```text
PASS secret
NICK dani
USER dani 0 * :Dani
JOIN #general
```

Registro incompleto o contraseña mala:

```text
PASS wrong
NICK roxy
USER roxy 0 * :Roxy
```

No debe enviarse `001`. Contraseña incorrecta → `464`. Faltan parámetros → `461`. Comando desconocido → `421`.

Operadores de canal (cliente A es `@` por haber creado `#general`):

```text
MODE #general +i
INVITE dani #general
TOPIC #general :tema
KICK #general dani :fuera
MODE #general +o dani
MODE #general +t
MODE #general +k secretkey
MODE #general +l 5
MODE #general -i-t-k-l-o dani
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

Servidor en marcha (`./ircserv 6667 secret`) y clientes netcat como en la sección 5. En cada caso, registrar primero a los clientes (`PASS` / `NICK` / `USER`). El primer usuario que entra en un canal vacío es operador (`@`).

### 1. Los mensajes llegan a los clientes que ya están en el canal

Cliente A:

```text
PASS secret
NICK roxy
USER roxy 0 * :Roxy
JOIN #general
PRIVMSG #general :hola canal
```

Cliente B:

```text
PASS secret
NICK dani
USER dani 0 * :Dani
JOIN #general
```

- B debe recibir `:roxy!… PRIVMSG #general :hola canal`.
- A no debe recibir su propio mensaje de canal.
- Un cliente C registrado, sin `JOIN #general`, no debe recibir ese `PRIVMSG`.

### 2. Un usuario regular no puede usar comandos de operador

Cliente A crea el canal y restringe el topic. Cliente B entra como miembro normal. Cliente C se registra (`charlie`) y no entra al canal.

Cliente A:

```text
JOIN #ops
MODE #ops +t
```

Cliente B:

```text
JOIN #ops
KICK #ops roxy :fuera
INVITE charlie #ops
TOPIC #ops :no deberia
MODE #ops +i
```

- Cada comando de B debe responder `482` (`ERR_CHANOPRIVSNEEDED`).
- A no debe ver `KICK`, `INVITE`, `TOPIC` ni `MODE`.
- C no debe recibir `INVITE`.
- `MODE #ops` (consulta, sin flags) sí puede usarlo B: numeric `324`.

### 3. Un operador puede usar los comandos de operador en cada canal que ha creado

Cliente A crea dos canales. Cliente B entra en ambos. Cliente C se registra (`charlie`) y no entra.

Cliente A:

```text
JOIN #alpha
JOIN #bravo
MODE #alpha +t
MODE #bravo +t
TOPIC #alpha :tema alpha
TOPIC #bravo :tema bravo
INVITE charlie #alpha
INVITE charlie #bravo
MODE #alpha +o dani
MODE #bravo +o dani
KICK #alpha dani :fuera
KICK #bravo dani :fuera
```

- En `#alpha` y en `#bravo`, B debe ver `MODE`, `TOPIC` y `KICK`.
- C debe recibir ambos `INVITE`.
- A debe recibir `341` (`RPL_INVITING`) por cada invitación.
- Privilegio por canal: si B crea `#charlie` y A entra después, A es miembro normal. `MODE #charlie +i` desde A debe responder `482`.

### 4. Cliente A envía un mensaje; B lo recibe al unirse

Cliente A:

```text
JOIN #general
PRIVMSG #general :antes de que entre b
```

- B, todavía fuera del canal, no debe recibir ese `PRIVMSG` en tiempo real.

Cliente B:

```text
JOIN #general
```

- Tras `JOIN`, `331`/`332`, `353` y `366`, B debe recibir `:roxy!… PRIVMSG #general :antes de que entre b`.

Cliente A:

```text
PRIVMSG #general :despues de que entre b
```

- B debe recibir `:roxy!… PRIVMSG #general :despues de que entre b`.

### 5. Canal solo por invitación (`+i`)

Cliente A:

```text
JOIN #invite
MODE #invite +i
```

Cliente B:

```text
JOIN #invite
```

- B debe recibir `473` (`ERR_INVITEONLYCHAN`) y no entrar.
- A no debe ver un `JOIN` de B.

Opcional, para confirmar que la invitación desbloquea la entrada:

```text
INVITE dani #invite
```

Cliente B:

```text
JOIN #invite
```

- B entra. A y B ven el `JOIN` de B.

### 6. Canal con clave (`+k`)

Cliente A:

```text
JOIN #keyed
MODE #keyed +k secretkey
```

Cliente B:

```text
JOIN #keyed
JOIN #keyed wrong
```

- Ambos `JOIN` deben responder `475` (`ERR_BADCHANNELKEY`). B no entra.

Cliente B, con la clave correcta:

```text
JOIN #keyed secretkey
```

- B entra. A y B ven el `JOIN` de B.

### 7. Límite de usuarios (`+l`)

Cliente A (único miembro; el límite debe ser `1` para que B no quepa):

```text
JOIN #limited
MODE #limited +l 1
```

Cliente B:

```text
JOIN #limited
```

- B debe recibir `471` (`ERR_CHANNELISFULL`) y no entrar.
- A no debe ver un `JOIN` de B.

Opcional: `MODE #limited -l` o `MODE #limited +l 2` y repetir el `JOIN` de B; ahora debe entrar.

---

## 12. Más tests (irssi)

Los mismos casos que la sección 11, con el cliente de referencia. Servidor en marcha (`./ircserv 6667 secret`) y una sesión irssi por cliente, como en la sección 4. Los numerics de error (`482`, `473`, `475`, `471`, `324`, `341`) aparecen en la ventana de estado.

Cliente A:

```text
/set nick roxy
/connect 127.0.0.1 6667 secret
```

Cliente B:

```text
/set nick dani
/connect 127.0.0.1 6667 secret
```

Cuando haga falta un tercer cliente (casos 2 y 3):

```text
/set nick charlie
/connect 127.0.0.1 6667 secret
```

El primer usuario que entra en un canal vacío es operador (`@`).

### 1. Los mensajes llegan a los clientes que ya están en el canal

Cliente A:

```text
/join #general
```

Cliente B:

```text
/join #general
```

Cliente A, en la ventana de `#general`:

```text
hola canal
```

- B debe ver `hola canal` en `#general`.
- A no debe ver su propio mensaje duplicado de forma anómala.
- Un cliente C conectado, sin `/join #general`, no debe recibir ese texto.

### 2. Un usuario regular no puede usar comandos de operador

Cliente A crea el canal y restringe el topic. Cliente B entra como miembro normal. Cliente C (`charlie`) no entra al canal.

Cliente A:

```text
/join #ops
/mode #ops +t
```

Cliente B:

```text
/join #ops
/kick roxy fuera
/invite charlie #ops
/topic no deberia
/mode #ops +i
```

- Cada comando de B debe mostrar `482` (`ERR_CHANOPRIVSNEEDED`) en la ventana de estado.
- A no debe ver `KICK`, `INVITE`, `TOPIC` ni cambio de modo.
- C no debe recibir invitación.
- `/mode #ops` (consulta, sin flags) sí puede usarlo B: numeric `324`.

### 3. Un operador puede usar los comandos de operador en cada canal que ha creado

Cliente A crea dos canales. Cliente B entra en ambos. Cliente C (`charlie`) no entra.

Cliente A:

```text
/join #alpha
/join #bravo
/mode #alpha +t
/mode #bravo +t
/topic #alpha tema alpha
/topic #bravo tema bravo
/invite charlie #alpha
/invite charlie #bravo
/mode #alpha +o dani
/mode #bravo +o dani
/kick #alpha dani fuera
/kick #bravo dani fuera
```

- En `#alpha` y en `#bravo`, B debe ver el cambio de modo, el topic nuevo y el `KICK`.
- C debe recibir ambas invitaciones (aviso de `INVITE` en la ventana de estado).
- A debe recibir `341` (`RPL_INVITING`) por cada invitación.
- Privilegio por canal: si B crea `#charlie` (`/join #charlie`) y A entra después, A es miembro normal. `/mode #charlie +i` desde A debe mostrar `482`.

### 4. Cliente A envía un mensaje; B lo recibe al unirse

Cliente A:

```text
/join #general
```

En la ventana de `#general`:

```text
antes de que entre b
```

- B, todavía fuera del canal, no debe ver ese texto en tiempo real.

Cliente B:

```text
/join #general
```

- B debe ver `antes de que entre b` en `#general` al entrar.

Cliente A, otra vez en `#general`:

```text
despues de que entre b
```

- B debe ver `despues de que entre b` en `#general`.

### 5. Canal solo por invitación (`+i`)

Cliente A:

```text
/join #invite
/mode #invite +i
```

Cliente B:

```text
/join #invite
```

- B debe ver `473` (`ERR_INVITEONLYCHAN`) y no entrar.
- A no debe ver un `JOIN` de B.

Opcional, para confirmar que la invitación desbloquea la entrada.

Cliente A:

```text
/invite dani #invite
```

Cliente B:

```text
/join #invite
```

- B entra. A y B ven el `JOIN` de B.

### 6. Canal con clave (`+k`)

Cliente A:

```text
/join #keyed
/mode #keyed +k secretkey
```

Cliente B:

```text
/join #keyed
/join #keyed wrong
```

- Ambos `/join` deben mostrar `475` (`ERR_BADCHANNELKEY`). B no entra.

Cliente B, con la clave correcta:

```text
/join #keyed secretkey
```

- B entra. A y B ven el `JOIN` de B.

### 7. Límite de usuarios (`+l`)

Cliente A (único miembro; el límite debe ser `1` para que B no quepa):

```text
/join #limited
/mode #limited +l 1
```

Cliente B:

```text
/join #limited
```

- B debe ver `471` (`ERR_CHANNELISFULL`) y no entrar.
- A no debe ver un `JOIN` de B.

Opcional: `/mode #limited -l` o `/mode #limited +l 2` y repetir el `/join #limited` de B; ahora debe entrar.