# ft_irc implementation to-do

Estado actual: parte obligatoria incompleta. Ya existen CLI, socket no bloqueante,
`poll()`, gestión básica de varios clientes, framing TCP, parser, buffer de salida y
envíos parciales. Quedan los siguientes trabajos.

## 1. Protocolo y respuestas

- [x] Completar la serialización única de `IrcMessage`: prefijo, comando, parámetros, trailing y `\r\n`.
- [x] Rechazar CR, LF y NUL internos en mensajes generados.
- [x] Definir un nombre estable para el servidor y guardar hostname o IP de cada cliente.
- [x] Centralizar los prefijos `:server` y `:nick!user@host`.
- [x] Centralizar las respuestas numéricas, su formato de tres cifras y el destinatario `*` cuando falte el nick.
- [x] Hacer que todos los handlers encolen respuestas y activen `POLLOUT`; nunca escribir directamente ni modificar el buffer sin actualizar `poll()`.
- [x] Devolver errores desde el dispatcher en vez de ignorar comandos desconocidos, parámetros ausentes o clientes no registrados.
- [x] Implementar los numerics documentados: `001–005`, `324`, `331`, `332`, `341`, `353`, `366`, `401`, `403`, `404`, `409`, `411`, `412`, `421`, `431`, `432`, `433`, `441`, `442`, `443`, `451`, `461`, `462`, `464`, `471`, `472`, `473`, `475` y `482`.
- [x] Limitar cada línea IRC a 512 bytes, incluyendo `\r\n`.
- [x] Limitar los buffers de entrada y salida; desconectar limpiamente clientes abusivos o demasiado lentos.
- [x] Implementar casemapping IRC para nicknames y canales: conservar el nombre original, pero buscar mediante una clave normalizada.

Referencias: [fase 0](phases/phase_0.md) y [fase 8](phases/phase_8.md).

## 2. Registro

- [ ] Implementar `PASS`: comprobar la contraseña real y gestionar `461`, `462` y `464`.
- [ ] Garantizar que una contraseña incorrecta nunca active `passwordAccepted`.
- [ ] Implementar `NICK`: validar formato, evitar duplicados y responder `431`, `432` o `433`.
- [ ] Crear un índice global normalizado `nickname -> Client`.
- [ ] Permitir cambios de nickname tras el registro; actualizar el índice y notificar una sola vez a usuarios relacionados.
- [ ] Liberar el nickname al desconectar al cliente.
- [ ] Implementar `USER`: exigir los parámetros necesarios, guardar username y realname y rechazar repeticiones mediante `462`.
- [ ] Completar el registro solo con `PASS + NICK + USER`; mantener la operación idempotente.
- [ ] Enviar la bienvenida una sola vez. Mínimo: `001`; contrato completo de la documentación: `001–005`.
- [ ] Permitir antes del registro únicamente `CAP`, `PASS`, `NICK`, `USER`, `PING`, `PONG` y `QUIT`.
- [ ] Responder `451` a comandos de mensajería o canales ejecutados antes del registro.

Referencia: [fase 9](phases/phase_9.md).

## 3. Conexión con un cliente real

- [ ] Implementar `PING`: responder `PONG` con el token exacto, también antes del registro; responder `409` si falta.
- [ ] Aceptar `PONG` sin producir `421`.
- [ ] Implementar `CAP LS` con una respuesta de capacidades vacía.
- [ ] Implementar `CAP LIST`.
- [ ] Implementar `CAP REQ` con respuesta `NAK`.
- [ ] Implementar `CAP END` sin bloquear el registro.
- [ ] Implementar `QUIT`: conservar el motivo, notificar una sola vez a usuarios relacionados y desconectar.
- [ ] Verificar una conexión completa con Irssi, cliente elegido en la documentación.

Referencia: [fase 10](phases/phase_10.md).

## 4. Modelo de canales

- [ ] Implementar el constructor y la API completa de `Channel`.
- [ ] Gestionar nombre, topic, miembros, operadores e invitados.
- [ ] Implementar el estado de los modos `+i`, `+t`, `+k` y `+l`.
- [ ] Mantener operadores por canal; el estado global `Client::isOperator` no representa correctamente el modelo IRC.
- [ ] Garantizar que todo operador sea miembro, que no haya miembros duplicados y que las invitaciones apunten a clientes válidos.
- [ ] Mantener clave y límite en estados coherentes al desactivar sus modos.
- [ ] Crear, buscar y destruir una única instancia de cada canal desde `Server`.
- [ ] Mantener sincronizados `Client::joinedChannels` y los miembros reales de cada canal.
- [ ] Eliminar los canales vacíos.

Referencia: [fase 11](phases/phase_11.md).

## 5. Comandos de canales y mensajería

- [ ] Implementar `JOIN`: validar el canal, crearlo, añadir al miembro y convertir al primer usuario en operador.
- [ ] Aplicar en `JOIN` los modos `+i`, `+k` y `+l`; consumir la invitación solo después de una entrada correcta.
- [ ] Difundir `JOIN` y enviar `331` o `332`, seguido de `353` y `366`.
- [ ] Evitar entradas duplicadas en un canal.
- [ ] Implementar `PART`: validar pertenencia, difundir la salida, limpiar estado y borrar el canal vacío.
- [ ] Implementar `NAMES`: devolver los miembros y marcar operadores con `@`.
- [ ] Implementar `PRIVMSG` a nickname: localizar el destinatario y entregarle el mensaje real.
- [ ] Implementar `PRIVMSG` a canal: exigir pertenencia y reenviar a todos los miembros salvo el emisor.
- [ ] Conservar exactamente el trailing, incluidos mensajes CTCP/DCC.
- [ ] Gestionar los errores `401`, `403`, `404`, `411` y `412`.
- [ ] Implementar `NOTICE` con el mismo routing que `PRIVMSG`, pero sin respuestas de error automáticas.
- [ ] Eliminar la respuesta ficticia `Message sent to...` existente.

Referencias: [fase 12](phases/phase_12.md) y [fase 13](phases/phase_13.md).

## 6. Comandos de operador

- [ ] Implementar `TOPIC`: consultar, establecer y eliminar el topic; aplicar `+t` y difundir los cambios.
- [ ] Implementar `INVITE`: comprobar usuario, canal, pertenencia y privilegios; almacenar la invitación; enviar `341` y notificar al invitado.
- [ ] Consumir una invitación únicamente después de un `JOIN` correcto y limpiarla si el cliente se desconecta.
- [ ] Implementar `KICK`: validar operador, objetivo y membresía; notificar antes de retirar al usuario.
- [ ] Mantener abierta la conexión del usuario expulsado, quitar su membresía y privilegios y borrar el canal si queda vacío.
- [ ] Implementar la consulta `MODE #canal` mediante `324`.
- [ ] Implementar `+i/-i`, `+t/-t`, `+k/-k`, `+l/-l` y `+o/-o`.
- [ ] Validar que el límite de usuarios sea positivo y no produzca overflow.
- [ ] Procesar combinaciones y cambios de signo, como `+it`, `+kl` y `+o-l`.
- [ ] Consumir correctamente los parámetros requeridos por cada modo.
- [ ] Validar todo el comando antes de modificar el estado para evitar cambios parciales.
- [ ] Difundir los cambios de modo a los miembros del canal.
- [ ] Integrar los modos con `JOIN`, `TOPIC`, `INVITE` y `KICK`.

Referencias: [fase 14](phases/phase_14.md), [fase 15](phases/phase_15.md), [fase 16](phases/phase_16.md) y [fase 17](phases/phase_17.md).

## 7. Desconexión y robustez

- [ ] Unificar toda desconexión en una función idempotente.
- [ ] Usarla para `QUIT`, `recv() == 0`, errores definitivos, `POLLHUP`, `POLLERR`, `POLLNVAL` y errores de escritura.
- [ ] Recopilar los destinatarios antes de destruir el cliente y evitar mensajes `QUIT` duplicados.
- [ ] Retirar al cliente de `poll`, mapa principal, índice de nicknames, canales, operadores e invitaciones.
- [ ] Cerrar cada descriptor una sola vez y no usar un cliente después de eliminarlo.
- [ ] Evitar iteradores invalidados al borrar clientes o canales.
- [ ] Recuperarse de errores de clientes individuales sin detener el servidor completo.
- [ ] Gestionar el agotamiento de descriptores y los fallos de `accept()` sin corromper el estado.
- [ ] Evitar registrar líneas completas que incluyan `PASS`.
- [ ] Completar o retirar declaraciones incompletas como `stop()`, `getPort()` y los helpers de búsqueda y prefijos.

Referencia: [fase 18](phases/phase_18.md).

## 8. Tests pendientes

- [ ] Ampliar los tests del parser: actualmente solo existe un caso feliz.
- [ ] Probar comandos fragmentados, comandos agrupados, trailing vacío, espacios, prefijos y entradas inválidas.
- [ ] Probar el límite de 512 bytes y buffers que nunca reciben terminador.
- [ ] Probar escrituras parciales, clientes lentos y desconexiones con salida pendiente.
- [ ] Probar todos los numerics y comandos incompletos.
- [ ] Probar varios clientes, nicknames únicos, canales, mensajes, topics, invitaciones, expulsiones y modos.
- [ ] Probar todas las rutas de desconexión y limpieza.
- [ ] Ejecutar Irssi y guardar un transcript completo de conexión y comandos.
- [ ] Ejecutar Valgrind con seguimiento de descriptores.
- [ ] Ejecutar ASan y UBSan.
- [ ] Confirmar ausencia de fugas, dobles cierres, referencias colgantes y canales vacíos.

Referencia: [fase 19](phases/phase_19.md).

## 9. Entrega y documentación

- [ ] Crear `PROTOCOL.md` con comandos soportados, registro, formato, numerics, canales, modos y transcript de Irssi.
- [ ] Añadir al README el comando real de ejecución.
- [ ] Añadir al README una descripción explícita del uso de IA, obligatoria por el subject.
- [ ] Añadir referencias técnicas clásicas y corregir erratas del README.
- [ ] Implementar `LIST` o dejar de anunciarlo en el README.
- [ ] Sacar `-fsanitize=address` de la compilación final y conservarlo en un target de depuración.
- [ ] Verificar un build limpio, `clean`, `fclean`, `re`, ausencia de relink innecesario y compatibilidad C++98 final.

Referencias: [subject](en.subject.pdf), [README](../README.md) y [Makefile](../Makefile).

## 10. Bonus

Solo después de completar y verificar toda la parte obligatoria.

- [ ] Implementar transferencia DCC: retransmitir CTCP exactamente mediante `PRIVMSG`; los bytes del archivo circulan entre clientes.
- [ ] Implementar un bot integrado como usuario IRC sin socket o mediante una abstracción separada.
- [ ] Añadir tests específicos de DCC y bot.

Referencia: [arquitectura](architecture.md).
