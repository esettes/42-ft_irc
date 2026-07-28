# Fase 16 — KICK

## Objetivo

Implementar el comando `KICK`, que permite a un operador expulsar a un usuario de un canal.

## Formato del comando

```text
KICK #general roxana :Motivo de la expulsión
```

El comando recibe:

1. El nombre del canal.
2. El nickname del usuario objetivo.
3. Un motivo opcional.

Si no se proporciona un motivo, puede utilizarse el nickname del operador como motivo predeterminado.

## Flujo de ejecución

```text
KICK #general roxana :Motivo
        ↓
¿Están presentes el canal y el usuario objetivo?
        ↓
¿Existe el canal?
        ↓
¿Existe el usuario objetivo?
        ↓
¿El emisor pertenece al canal?
        ↓
¿El emisor es operador del canal?
        ↓
¿El objetivo pertenece al canal?
        ↓
Notificar el KICK a todos los miembros
        ↓
Eliminar al objetivo del canal
        ↓
Eliminar sus privilegios de operador
        ↓
¿El canal ha quedado vacío?
    ├── sí → eliminar el canal
    └── no → conservar el canal
```

## Comprobaciones necesarias

El servidor debe comprobar:

1. Que se han recibido los parámetros obligatorios.
2. Que el canal existe.
3. Que el usuario objetivo existe.
4. Que el usuario que ejecuta `KICK` pertenece al canal.
5. Que el emisor es operador del canal.
6. Que el usuario objetivo pertenece al canal.

Las comprobaciones deben realizarse antes de modificar el estado del canal.

## Notificación del KICK

Si todas las comprobaciones son correctas, el servidor debe construir un mensaje con el prefijo completo del operador:

```text
:operador!username@hostname KICK #general roxana :Motivo
```

Este mensaje debe enviarse a todos los miembros actuales del canal, incluido el usuario expulsado.

La notificación debe realizarse antes de eliminar al objetivo para garantizar que también reciba el mensaje.

## Actualización del canal

Después de enviar la notificación:

1. Eliminar al usuario objetivo de la colección de miembros.
2. Eliminarlo de la colección de operadores, si tenía ese privilegio.
3. Eliminar cualquier otra información del usuario asociada al canal.
4. Comprobar si el canal ha quedado vacío.
5. Eliminar el canal del servidor si ya no tiene miembros.

Expulsar a un usuario de un canal no debe cerrar su conexión con el servidor ni eliminarlo de otros canales.

## Errores relevantes

```text
401 ERR_NOSUCHNICK
```

El usuario objetivo no existe en el servidor.

```text
403 ERR_NOSUCHCHANNEL
```

El canal indicado no existe.

```text
441 ERR_USERNOTINCHANNEL
```

El usuario objetivo no pertenece al canal.

```text
442 ERR_NOTONCHANNEL
```

El emisor no pertenece al canal.

```text
461 ERR_NEEDMOREPARAMS
```

Falta el canal o el nickname del usuario objetivo.

```text
482 ERR_CHANOPRIVSNEEDED
```

El emisor pertenece al canal, pero no es operador.

## Ejemplo completo

Comando recibido:

```text
KICK #general roxana :Comportamiento inapropiado
```

Mensaje enviado a los miembros del canal:

```text
:admin!admin@localhost KICK #general roxana :Comportamiento inapropiado
```

Después de enviar el mensaje:

```text
Miembros antes:     admin, roxana, usuario2
Operadores antes:   admin, roxana

Miembros después:   admin, usuario2
Operadores después: admin
```

## Resultado esperado

Al finalizar esta fase, el servidor debe ser capaz de:

- Interpretar correctamente el comando `KICK`.
- Validar los permisos del usuario que solicita la expulsión.
- Comprobar que el canal y el usuario objetivo son válidos.
- Notificar la expulsión a todos los miembros actuales.
- Eliminar al usuario de los miembros y operadores del canal.
- Mantener abierta la conexión del usuario expulsado.
- Eliminar el canal cuando quede completamente vacío.
- Responder con códigos numéricos coherentes cuando el comando no pueda ejecutarse.