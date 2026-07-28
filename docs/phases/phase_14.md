# Fase 14 — Comando `TOPIC`

## Objetivo

Implementar el comando `TOPIC`, encargado de gestionar el tema de un canal.

Debe permitir:

- Consultar el tema actual.
- Establecer un nuevo tema.
- Modificar el tema existente.
- Eliminar el tema.
- Respetar la restricción del modo `+t`.
- Notificar los cambios a todos los miembros del canal.

---

## Sintaxis

### Consultar el tema

```irc
TOPIC #general
```

### Establecer o modificar el tema

```irc
TOPIC #general :Nuevo tema del canal
```

El texto situado después de `:` constituye un único parámetro y puede contener espacios.

### Eliminar el tema

```irc
TOPIC #general :
```

Un parámetro final vacío indica que el canal debe quedarse sin tema.

---

## Estado necesario en `Channel`

Cada canal debe almacenar:

```cpp
class Channel
{
private:
    std::string topic;
    bool topicRestricted;
};
```

El atributo `topicRestricted` representa el modo `+t`:

- `false`: cualquier miembro del canal puede modificar el tema.
- `true`: solamente los operadores del canal pueden modificarlo.

Una cadena vacía puede representar que el canal no tiene ningún tema establecido.

---

## Consultar el tema

Cuando un cliente envía:

```irc
TOPIC #general
```

El servidor debe comprobar:

1. Que el cliente está registrado.
2. Que se ha indicado el nombre del canal.
3. Que el canal existe.
4. Que el cliente pertenece al canal.

Si el canal tiene un tema establecido, debe responder con:

```irc
:server.name 332 roxana #general :Tema actual del canal
```

El código `332` corresponde a:

```text
RPL_TOPIC
```

Si el canal no tiene ningún tema establecido, debe responder con:

```irc
:server.name 331 roxana #general :No topic is set
```

El código `331` corresponde a:

```text
RPL_NOTOPIC
```

Consultar el tema no requiere que el usuario sea operador del canal.

---

## Modificar el tema

Cuando un cliente envía:

```irc
TOPIC #general :Nuevo tema
```

El servidor debe comprobar:

1. Que el cliente está registrado.
2. Que se ha indicado el nombre del canal.
3. Que el canal existe.
4. Que el cliente pertenece al canal.
5. Si está activo el modo `+t`, que el cliente sea operador.

Si todas las comprobaciones son correctas:

1. Se actualiza el tema almacenado en `Channel`.
2. Se construye el mensaje con el prefijo completo del usuario.
3. Se envía el cambio a todos los miembros del canal, incluido el emisor.

Ejemplo:

```irc
:roxana!roxana@localhost TOPIC #general :Nuevo tema
```

---

## Restricción del modo `+t`

El modo `+t` controla quién puede modificar el tema:

```text
+t activado
    ↓
solo los operadores pueden modificar el tema
```

```text
+t desactivado
    ↓
cualquier miembro del canal puede modificar el tema
```

El modo `+t` solamente afecta a la modificación del tema. Un miembro normal puede seguir consultándolo.

---

## Eliminación del tema

Cuando se recibe:

```irc
TOPIC #general :
```

El servidor debe:

1. Realizar las mismas comprobaciones que para modificar el tema.
2. Guardar una cadena vacía como tema.
3. Notificar el cambio a todos los miembros.

Ejemplo de notificación:

```irc
:roxana!roxana@localhost TOPIC #general :
```

Después de eliminarlo, una nueva consulta debe producir:

```irc
:server.name 331 roxana #general :No topic is set
```

---

## Errores relevantes

### `461 ERR_NEEDMOREPARAMS`

Se utiliza cuando no se proporciona el nombre del canal:

```irc
TOPIC
```

Respuesta:

```irc
:server.name 461 roxana TOPIC :Not enough parameters
```

### `403 ERR_NOSUCHCHANNEL`

Se utiliza cuando el canal no existe:

```irc
TOPIC #inexistente
```

Respuesta:

```irc
:server.name 403 roxana #inexistente :No such channel
```

### `442 ERR_NOTONCHANNEL`

Se utiliza cuando el usuario no pertenece al canal:

```irc
:server.name 442 roxana #general :You're not on that channel
```

### `482 ERR_CHANOPRIVSNEEDED`

Se utiliza cuando:

- El modo `+t` está activo.
- El usuario intenta modificar el tema.
- El usuario no es operador.

Respuesta:

```irc
:server.name 482 roxana #general :You're not channel operator
```

### `451 ERR_NOTREGISTERED`

Se utiliza cuando un cliente no registrado intenta ejecutar el comando:

```irc
:server.name 451 * :You have not registered
```

---

## Flujo general

```text
TOPIC recibido
    ↓
¿Cliente registrado?
    ├── no → 451 ERR_NOTREGISTERED
    └── sí
         ↓
¿Se indicó un canal?
    ├── no → 461 ERR_NEEDMOREPARAMS
    └── sí
         ↓
¿Existe el canal?
    ├── no → 403 ERR_NOSUCHCHANNEL
    └── sí
         ↓
¿El usuario pertenece al canal?
    ├── no → 442 ERR_NOTONCHANNEL
    └── sí
         ↓
¿Se proporcionó un nuevo tema?
    ├── no → responder con 331 o 332
    └── sí
         ↓
¿Está activo el modo +t?
    ├── no → actualizar el tema
    └── sí
         ↓
¿El usuario es operador?
    ├── no → 482 ERR_CHANOPRIVSNEEDED
    └── sí → actualizar el tema
         ↓
Notificar a todos los miembros
```

---

## Resultado esperado

Al terminar esta fase, el servidor debe ser capaz de:

- Consultar el tema de un canal.
- Informar cuando un canal no tiene tema.
- Establecer y modificar el tema.
- Eliminar el tema mediante un parámetro final vacío.
- Aplicar correctamente la restricción del modo `+t`.
- Rechazar cambios realizados por usuarios sin permisos.
- Notificar los cambios a todos los miembros del canal.
- Devolver respuestas numéricas coherentes ante cualquier error.