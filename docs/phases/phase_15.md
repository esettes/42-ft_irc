# Fase 15 — Implementación de `INVITE`

## Objetivo

Implementar el comando `INVITE`, que permite invitar a un usuario a un canal.

La invitación es especialmente importante para los canales que tienen activado el modo `+i`, ya que solamente los usuarios invitados podrán entrar en ellos.

---

## Sintaxis del comando

```irc
INVITE <nickname> <canal>
```

Ejemplo:

```irc
INVITE roxana #privado
```

En este ejemplo, el usuario que ejecuta el comando invita a `roxana` al canal `#privado`.

---

## Estado necesario en `Channel`

Cada canal debe mantener una colección de usuarios invitados.

Una posible representación sería:

```cpp
std::set<Client *> invitedClients;
```

También se puede utilizar un identificador estable del cliente, como su descriptor de archivo:

```cpp
std::set<int> invitedClientDescriptors;
```

No conviene almacenar copias completas de los clientes.

La clase `Channel` debería proporcionar métodos similares a los siguientes:

```cpp
void inviteClient(Client &client);
bool isClientInvited(const Client &client) const;
void removeInvitation(const Client &client);
```

El uso de un `std::set` evita almacenar varias veces la misma invitación.

---

## Comprobaciones iniciales

Antes de procesar la invitación, el servidor debe comprobar:

1. Que el cliente emisor está registrado.
2. Que se han recibido el nickname y el canal.
3. Que el canal existe.
4. Que el usuario objetivo existe.
5. Que el emisor pertenece al canal.
6. Que el usuario objetivo no pertenece ya al canal.
7. Que el emisor tiene permisos para invitar.
8. Que el usuario objetivo todavía no está invitado, si se quiere controlar explícitamente este caso.

---

## Orden recomendado de validación

El flujo del comando puede seguir este orden:

```text
¿El cliente está registrado?
    ↓
¿Se recibieron nickname y canal?
    ↓
¿Existe el canal?
    ↓
¿Existe el usuario objetivo?
    ↓
¿El emisor pertenece al canal?
    ↓
¿El objetivo pertenece ya al canal?
    ↓
¿El emisor tiene permisos?
    ↓
Añadir al objetivo a la colección de invitados
    ↓
Confirmar la invitación al emisor
    ↓
Notificar la invitación al usuario objetivo
```

Mantener siempre el mismo orden de validación hace que los errores sean predecibles y facilita las pruebas.

---

## Permisos para invitar

Como mínimo, cuando el canal tenga activado el modo `+i`, solamente un operador debería poder invitar usuarios.

Para `ft_irc`, la opción más segura es considerar `INVITE` una acción de operador y exigir que el emisor sea operador del canal.

```cpp
if (!channel.isOperator(client))
{
    // Send ERR_CHANOPRIVSNEEDED.
    return;
}
```

La regla elegida debe aplicarse de forma consistente en todo el servidor.

---

## Invitación correcta

Cuando todas las comprobaciones sean válidas, el servidor debe:

1. Añadir al usuario objetivo a la colección de invitados del canal.
2. Enviar `RPL_INVITING` al usuario que ejecutó el comando.
3. Enviar un mensaje `INVITE` al usuario invitado.

### Confirmación al emisor

La respuesta numérica habitual es:

```irc
:irc.local 341 operador roxana #privado
```

El código `341` corresponde a:

```text
RPL_INVITING
```

### Notificación al usuario invitado

El usuario objetivo debe recibir un mensaje con el prefijo completo del emisor:

```irc
:operador!username@localhost INVITE roxana :#privado
```

La invitación no debería notificarse a todos los miembros del canal. Solamente necesitan recibirla:

- El usuario que envía la invitación, mediante `341 RPL_INVITING`.
- El usuario invitado, mediante el mensaje `INVITE`.

---

## Integración con `JOIN`

La implementación de `JOIN` debe consultar si el usuario está invitado cuando el canal tenga activado el modo `+i`.

Una comprobación simplificada sería:

```cpp
if (channel.isInviteOnly()
    && !channel.isClientInvited(client))
{
    // Send ERR_INVITEONLYCHAN.
    return;
}
```

Si el usuario aparece en la colección de invitados, puede superar la restricción `+i`.

La invitación solamente debe evitar el error:

```text
473 ERR_INVITEONLYCHAN
```

No debería permitir saltarse otras restricciones, salvo que se haya decidido expresamente lo contrario.

Por ejemplo, el usuario invitado todavía debe cumplir:

- La contraseña del canal establecida mediante `+k`.
- El límite de usuarios establecido mediante `+l`.

---

## Consumo de la invitación

La invitación debe eliminarse después de que el usuario entre correctamente en el canal:

```cpp
channel.addMember(client);
channel.removeInvitation(client);
```

Es importante eliminarla después de completar correctamente el `JOIN`.

No debe consumirse si la entrada falla debido a:

- Una contraseña incorrecta.
- El límite de usuarios.
- Otro error de validación.

De esta manera, el usuario puede corregir el problema y volver a intentar entrar sin necesitar una nueva invitación.

---

## Limpieza de invitaciones

También deben eliminarse las referencias a un usuario invitado cuando:

- El usuario se desconecta mediante `QUIT`.
- Su conexión se cierra inesperadamente.
- El canal se elimina.
- El usuario entra correctamente y consume la invitación.

Esto evita referencias inválidas a clientes que ya no existen.

Si las invitaciones se almacenan mediante nickname, también será necesario actualizarlas cuando el usuario cambie de nickname. Por este motivo, resulta preferible utilizar un identificador estable.

---

## Errores relevantes

### Cliente no registrado

```text
451 ERR_NOTREGISTERED
```

Se utiliza si el cliente intenta ejecutar `INVITE` antes de completar su registro.

Ejemplo:

```irc
:irc.local 451 * :You have not registered
```

### Faltan parámetros

```text
461 ERR_NEEDMOREPARAMS
```

Se utiliza cuando falta el nickname o el canal.

Ejemplos incorrectos:

```irc
INVITE
INVITE roxana
```

Respuesta:

```irc
:irc.local 461 operador INVITE :Not enough parameters
```

### Usuario inexistente

```text
401 ERR_NOSUCHNICK
```

Se utiliza cuando el nickname objetivo no existe.

```irc
:irc.local 401 operador desconocido :No such nick
```

### Canal inexistente

```text
403 ERR_NOSUCHCHANNEL
```

Se utiliza cuando el canal especificado no existe.

```irc
:irc.local 403 operador #desconocido :No such channel
```

### El emisor no pertenece al canal

```text
442 ERR_NOTONCHANNEL
```

```irc
:irc.local 442 operador #privado :You're not on that channel
```

### El objetivo ya pertenece al canal

```text
443 ERR_USERONCHANNEL
```

```irc
:irc.local 443 operador roxana #privado :is already on channel
```

### El emisor no es operador

```text
482 ERR_CHANOPRIVSNEEDED
```

```irc
:irc.local 482 usuario #privado :You're not channel operator
```

---

## Funciones recomendadas

La lógica puede dividirse en funciones pequeñas y claramente diferenciadas:

```cpp
void Server::handleInvite(Client &client, const Command &command);
Client *Server::findClientByNickname(const std::string &nickname);
Channel *Server::findChannel(const std::string &channelName);
```

La clase `Channel` puede encargarse de gestionar el estado de las invitaciones:

```cpp
void Channel::inviteClient(Client &client);
bool Channel::isClientInvited(const Client &client) const;
void Channel::removeInvitation(const Client &client);
```

El `Server` debe validar el comando y enviar las respuestas, mientras que `Channel` debe gestionar el estado interno del canal.

---

## Casos de prueba mínimos

### Invitación válida

1. Crear un canal.
2. Hacer que el emisor sea operador.
3. Invitar a otro usuario.
4. Comprobar que el emisor recibe `341 RPL_INVITING`.
5. Comprobar que el usuario objetivo recibe el mensaje `INVITE`.

### Entrada en un canal `+i`

1. Activar el modo `+i`.
2. Intentar entrar sin invitación.
3. Comprobar que se recibe `473 ERR_INVITEONLYCHAN`.
4. Invitar al usuario.
5. Repetir el `JOIN`.
6. Comprobar que ahora puede entrar.

### Consumo de la invitación

1. Invitar a un usuario.
2. Hacer que entre correctamente.
3. Comprobar que la invitación ha sido eliminada.
4. Hacer que abandone el canal.
5. Intentar entrar otra vez sin una nueva invitación.
6. Comprobar que recibe `473 ERR_INVITEONLYCHAN`.

### Invitación sin permisos

1. Entrar en el canal con un usuario que no sea operador.
2. Intentar invitar a otro usuario.
3. Comprobar que se recibe `482 ERR_CHANOPRIVSNEEDED`.
4. Comprobar que la invitación no se almacena.

### Usuario ya presente

1. Invitar a un usuario que ya pertenece al canal.
2. Comprobar que se recibe `443 ERR_USERONCHANNEL`.
3. Comprobar que no se añade ninguna invitación.

### Restricciones adicionales

1. Activar `+i` y `+k`.
2. Invitar a un usuario.
3. Intentar entrar con una contraseña incorrecta.
4. Comprobar que el `JOIN` falla.
5. Comprobar que la invitación no se consume.
6. Entrar con la contraseña correcta.
7. Comprobar que la invitación se elimina después del `JOIN`.

---

## Resultado esperado de la fase

Al terminar esta fase, el servidor debe ser capaz de:

- Procesar correctamente `INVITE <nickname> <canal>`.
- Validar la existencia del usuario y del canal.
- Comprobar que el emisor pertenece al canal.
- Comprobar los permisos del emisor.
- Evitar invitar a usuarios que ya pertenecen al canal.
- Almacenar las invitaciones sin duplicados.
- Enviar `341 RPL_INVITING` al emisor.
- Notificar la invitación al usuario objetivo.
- Permitir que un usuario invitado supere el modo `+i`.
- Consumir la invitación solamente después de un `JOIN` correcto.
- Limpiar las invitaciones cuando un cliente se desconecta.
- Responder con códigos numéricos coherentes ante cada error.