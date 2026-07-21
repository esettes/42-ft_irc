# Fase 0

## Primeros comandos soportados por el servidor

```text
Negociación y registro	    CAP, PASS, NICK, USER
Conexión	                PING, PONG, QUIT
Mensajería	                PRIVMSG, NOTICE
Canales básicos	            JOIN, PART, NAMES
Requeridos por el subject	KICK, INVITE, TOPIC, MODE
```

## `CAP`

Aunque no se implementen capacidades IRCv3, hay que responder. Ignorarlo sin respuesta puede dejar al cliente esperando.

```text
Entrada del cliente	    Respuesta
CAP LS 302	            :irc.local CAP * LS :
CAP LIST	            :irc.local CAP * LIST :
CAP REQ :algo	        :irc.local CAP * NAK :algo
CAP END	                Ninguna
```

`CAP` no debe bloquear el registro: el usuario queda registrado cuando haya enviado correctamente `PASS`, `NICK` y `USER`.

## Reglas de registro

Cada cliente mantiene este estado:

`CONNECTED → REGISTERING → REGISTERED → QUITTING`

Y estos datos:

```text
passwordAccepted
nickname
username
realname
```

El registro se completa una única vez cuando:

```text
passwordAccepted == true
nickname no está vacío
username no está vacío
```

Y entonces se envía numerics de bienvenida.

Reglas importantes:

- `PASS \<password>`: valida la contraseña del servidor.
- `NICK \<nick>`: obligatorio; puede cambiarse tras el registro.
- `USER \<user> 0 * :Nombre real`: obligatorio; solo se acepta una vez.
- Los comandos de canal o mensajería antes del registro responden `451 ERR_NOTREGISTERED`.
- `CAP` puede recibirse antes de `PASS`.
- `PING` debe poder responderse incluso antes de completar el registro.
- Un nick ya usado responde `433 ERR_NICKNAMEINUSE`.

## Formato interno del mensaje 

```cpp
struct IrcMessage
{
    bool hasPrefix;
    std::string prefix;
    std::string command;
    std::vector<std::string> parameters;
};
```

Ejemplo:

`:roxana!roxana@localhost PRIVMSG #general :Hola a todas`

Resultado:

```text
hasPrefix: true
prefix: "roxana!roxana@localhost"
command: "PRIVMSG"
parameters[0]: "#general"
parameters[1]: "Hola a todas"
```

El `:` indica que ese último parámetro puede contener espacios.

El parser debe:

- Acumular datos por cliente hasta encontrar `\r\n`.
- Rechazar o cerrar limpiamente líneas que superen el límite IRC tradicional: 512 bytes incluyendo `\r\n`.
- Extraer opcionalmente el prefijo inicial `:prefijo`.
- Convertir el comando a mayúsculas.
- Separar parámetros normales por espacios.
- Tratar `:texto con espacios` como un único último parámetro.
- No permitir `\r` o `\n` dentro de valores generados por el servidor.

Y para enviar respuestas, usar un único serializador. Nunca concatenar mensajes IRC repartidos por handlers.

`[:prefix ]COMMAND [param1] [param2] [:last parameter with spaces]\r\n`

## Mensajes y prefijos que emitirá el servidor

Los mensajes del servidor deben tener un nombre estable, por ejemplo:

`irc.local`

Los mensajes de un usuario usarán:

`:nick!username@hostname`

Ejemplos:

```text
:roxana!roxana@localhost JOIN :#general
:roxana!roxana@localhost PRIVMSG #general :Hola
:irc.local 332 roxana #general :Tema actual
```

No hardcodear roxana: en los numerics, el segundo parámetro suele ser siempre el nick actual del cliente receptor.

## Numerics mínimos en este punto

```text
Bienvenida	                001 RPL_WELCOME
Información del servidor	002, 003, 004
Capacidades del servidor	005 RPL_ISUPPORT
Falta de parámetros	        461 ERR_NEEDMOREPARAMS
Ya registrado	            462 ERR_ALREADYREGISTRED
Contraseña incorrecta	    464 ERR_PASSWDMISMATCH
Sin nick	                431 ERR_NONICKNAMEGIVEN
Nick inválido / ocupado	    432, 433
No registrado	            451 ERR_NOTREGISTERED
Comando desconocido	        421 ERR_UNKNOWNCOMMAND
Usuario inexistente	        401 ERR_NOSUCHNICK
Canal inexistente	        403 ERR_NOSUCHCHANNEL
No se puede enviar al canal	404 ERR_CANNOTSENDTOCHAN
Sin destinatario / texto	411, 412
No está en el canal	        442 ERR_NOTONCHANNEL
Usuario ya está en el canal	443 ERR_USERONCHANNEL
Canal lleno	                471 ERR_CHANNELISFULL
Canal por invitación	    473 ERR_INVITEONLYCHAN
Clave incorrecta	        475 ERR_BADCHANNELKEY
Sin privilegios de operador	482 ERR_CHANOPRIVSNEEDED
Sin tema / tema actual	    331, 332
Lista de usuarios	        353, 366
Modos actuales del canal	324
Invitación confirmada	    341
```

Tras registrar correctamente:

```text
:irc.local 001 roxana :Welcome to the ft_irc network roxana
:irc.local 002 roxana :Your host is irc.local
:irc.local 003 roxana :This server was created today
:irc.local 004 roxana irc.local 1.0 o itk
:irc.local 005 roxana CHANTYPES=# PREFIX=(o)@ CHANMODES=,k,l,it :are supported by this server
```

El `005` debe reflejar la implementación real.

## Contrato concreto de los canales

Definir estas reglas ahora; evitará contradicciones más adelante.

- Los canales empiezan por `#`.
- La primera persona que entra crea el canal y recibe operador (`+o`).
- Al hacer `JOIN`, difundir el `JOIN` a todos los miembros.
- Después enviar al usuario que entra:
    - `331` o `332` para el tema;
    - `353` con los miembros;
    - `366` al terminar la lista.
- `PART` y `QUIT` se difunden a los usuarios que compartan canal.
- `PRIVMSG` a canal se envía a sus miembros, excepto al emisor.
- `PRIVMSG` a nick se entrega solo al usuario destino.
- Un `NOTICE` se comporta como `PRIVMSG`, pero no genera mensajes de error automáticos.
- `+i`: solo se entra con invitación.
- `+t`: solo operadores cambian el tema.
- `+k \<clave>`: exige clave al hacer `JOIN #canal clave`.
- `+l \<número>`: limita miembros.
- `+o \<nick>` y `-o \<nick>`: dan o retiran operador.
- Solo operadores pueden usar `KICK`, `INVITE` y modificar modos.
- `TOPIC #canal` sin texto consulta el tema; con texto lo modifica.
- `MODE #canal` sin argumentos devuelve `324`.

No anunciar modos que no existen: si no se implementa voz, bans o canales secretos, no devolver `+v`, `+b`, `+s`, etc.

## Comparación de nicks y canales

IRC no trata los nicks como texto estrictamente sensible a mayúsculas. Definir desde ya una función de normalización para buscar usuarios y canales.

Con `CASEMAPPING=rfc1459`, por ejemplo, `Roxana`, `roxana` y `ROXANA` son el mismo nick. También se consideran equivalentes ciertos caracteres como `[` y `{`.

Guardar nombre original para mostrarlo, pero usar una versión normalizada como clave en los mapas.

## Resultado final en fase 0

Un `PROTOCOL.md` breve con:

- La lista anterior de comandos soportados y no soportados.
- El ciclo de registro.
- El formato `IrcMessage`.
- La tabla de numerics.
- Las reglas de canales y modos.
- Un transcript real de irssi al conectar y al ejecutar `/join`, `/msg`, `/topic`, `/mode`, `/invite`, `/kick` y `/quit`.
