# Fase 17 — Comando `MODE`

## Objetivo

Implementar la consulta y modificación de los modos de un canal.

Es recomendable dejar `MODE` para el final de los comandos de operador porque es el handler con más combinaciones, validaciones y consumo de parámetros.

El proyecto exige implementar los siguientes modos:

| Modo | Función |
|---|---|
| `+i` / `-i` | Activar o desactivar el canal solo para invitados |
| `+t` / `-t` | Restringir o permitir la modificación del topic |
| `+k` / `-k` | Establecer o eliminar la contraseña del canal |
| `+o` / `-o` | Conceder o retirar privilegios de operador |
| `+l` / `-l` | Establecer o eliminar el límite de usuarios |

---

## 1. Consultar los modos actuales

Formato:

```text
MODE #general
```

El servidor debe comprobar:

- Que el canal existe.
- Que el usuario está registrado.
- No es necesario ser operador para consultar los modos.

La respuesta puede utilizar:

```text
324 RPL_CHANNELMODEIS
```

Ejemplo:

```text
:server.name 324 roxana #general +itkl secret 10
```

La respuesta debe incluir:

- Los modos activos.
- La contraseña si está activo `+k`.
- El límite de usuarios si está activo `+l`.

El modo `+o` no se incluye normalmente en esta lista porque representa un privilegio asociado a usuarios concretos. Los operadores pueden identificarse mediante el prefijo `@` en la respuesta de `NAMES`.

Es conveniente utilizar siempre el mismo orden al construir la lista de modos, por ejemplo:

```text
itkl
```

---

## 2. Modos sin argumentos

Los modos `i` y `t` no consumen parámetros adicionales.

### Modo de invitación

```text
MODE #general +i
MODE #general -i
```

Comportamiento:

- `+i` activa el modo de acceso solo mediante invitación.
- `-i` desactiva esta restricción.
- Cuando está activo, `JOIN` debe rechazar a los usuarios que no estén invitados.
- Una invitación válida permite superar esta restricción.

### Restricción del topic

```text
MODE #general +t
MODE #general -t
```

Comportamiento:

- `+t` permite que solamente los operadores cambien el topic.
- `-t` permite que cualquier miembro del canal cambie el topic.
- Este estado debe ser consultado desde el handler de `TOPIC`.

---

## 3. Modos con argumentos

### Contraseña del canal

```text
MODE #general +k secret
MODE #general -k
```

Comportamiento:

- `+k` consume una contraseña como argumento.
- Guarda la contraseña del canal.
- Activa el modo de canal con clave.
- `JOIN` debe comprobar que el usuario proporciona la contraseña correcta.
- `-k` elimina la contraseña almacenada.
- `-k` desactiva el modo de canal con clave.
- En este diseño, `-k` no necesita consumir ningún argumento.

La contraseña proporcionada a `+k` no debe estar vacía.

---

### Límite de usuarios

```text
MODE #general +l 10
MODE #general -l
```

Comportamiento:

- `+l` consume un número como argumento.
- El número debe ser un entero estrictamente positivo.
- Guarda el límite máximo de usuarios.
- `JOIN` debe rechazar nuevas entradas cuando el canal haya alcanzado el límite.
- `-l` elimina el límite.
- `-l` no consume ningún argumento.

La conversión del límite debe validar:

- Que todos los caracteres sean numéricos.
- Que el valor sea mayor que cero.
- Que el valor no provoque desbordamiento.
- Que el valor pueda almacenarse en el tipo utilizado por el canal.

---

### Operadores del canal

```text
MODE #general +o roxana
MODE #general -o roxana
```

Comportamiento:

- `+o` concede privilegios de operador al usuario indicado.
- `-o` elimina sus privilegios de operador.
- Ambos modos consumen un nickname como argumento.

Antes de modificar los privilegios se debe comprobar:

- Que el usuario objetivo existe.
- Que el usuario objetivo pertenece al canal.
- Que el emisor pertenece al canal.
- Que el emisor es operador del canal.

La colección de operadores debe impedir duplicados.

No es necesario expulsar al usuario cuando se le retira el privilegio de operador. Simplemente deja de formar parte de la colección de operadores.

---

## 4. Validaciones generales

Para modificar los modos de un canal se debe comprobar, en este orden:

1. El usuario está registrado.
2. Se ha proporcionado el nombre del canal.
3. El canal existe.
4. Se ha proporcionado una cadena de modos.
5. El emisor pertenece al canal.
6. El emisor es operador.
7. Todos los modos son conocidos.
8. Cada modo dispone de los argumentos necesarios.
9. Los argumentos son válidos para el modo correspondiente.

Consultar los modos mediante:

```text
MODE #general
```

no debería requerir privilegios de operador.

Modificar los modos sí debe requerir que el emisor pertenezca al canal y sea operador.

---

## 5. Combinación de modos

El comando debe aceptar varios modos dentro de la misma cadena:

```text
MODE #general +it
MODE #general -it
MODE #general +kl secret 10
MODE #general +o-l roxana
MODE #general +kol secret roxana 10
```

Los símbolos `+` y `-` cambian la operación aplicada a las letras que aparecen después.

Ejemplo:

```text
MODE #general +o-l roxana
```

Debe interpretarse como:

1. `+o roxana`: conceder privilegios de operador a `roxana`.
2. `-l`: eliminar el límite de usuarios.

Ejemplo:

```text
MODE #general +kol secret roxana 10
```

Debe interpretarse como:

1. `+k secret`
2. `+o roxana`
3. `+l 10`

Los argumentos se consumen en el mismo orden en el que aparecen las letras de modo.

---

## 6. Consumo de parámetros

Cada modo tiene reglas diferentes:

| Modo | Con `+` | Con `-` |
|---|---:|---:|
| `i` | Sin argumento | Sin argumento |
| `t` | Sin argumento | Sin argumento |
| `k` | Requiere contraseña | Sin argumento |
| `l` | Requiere límite | Sin argumento |
| `o` | Requiere nickname | Requiere nickname |

Ejemplo:

```text
MODE #general +itkol secret roxana 10
```

Consumo de parámetros:

| Operación | Parámetro consumido |
|---|---|
| `+i` | Ninguno |
| `+t` | Ninguno |
| `+k` | `secret` |
| `+o` | `roxana` |
| `+l` | `10` |

---

## 7. Parser específico de modos

El parser general de IRC debería producir algo equivalente a:

```text
command: MODE
parameters:
  - "#general"
  - "+kol"
  - "secret"
  - "roxana"
  - "10"
```

Después, el handler de `MODE` debe utilizar un parser específico para interpretar la cadena `+kol`.

Flujo recomendado:

```text
cadena de modos
    ↓
recorrer cada carácter
    ↓
si aparece + o - → actualizar la operación actual
    ↓
identificar la letra de modo
    ↓
determinar si necesita argumento
    ↓
consumir el siguiente argumento cuando corresponda
    ↓
validar la operación
    ↓
modificar el estado del canal
```

El parser debe mantener:

- La operación actual: añadir o eliminar.
- El índice del parámetro que debe consumirse.
- La lista de operaciones válidas.
- Los modos aplicados correctamente.
- Los argumentos asociados a esos modos.

Conviene separar:

1. La interpretación de la cadena de modos.
2. La validación de cada operación.
3. La modificación del canal.
4. La construcción del mensaje que se notificará.

---

## 8. Representación interna recomendada

Una operación de modo puede representarse conceptualmente con:

```text
ModeOperation
├── action: add/remove
├── mode: i/t/k/o/l
├── requiresArgument
└── argument
```

Ejemplo para:

```text
MODE #general +o-l roxana
```

Resultado:

```text
ModeOperation
├── action: add
├── mode: o
└── argument: roxana

ModeOperation
├── action: remove
├── mode: l
└── argument: ninguno
```

Generar primero una colección de operaciones facilita:

- Detectar parámetros ausentes.
- Validar combinaciones.
- Evitar modificar parcialmente el canal por un error de parsing.
- Construir correctamente la notificación final.
- Probar el parser independientemente del servidor.

---

## 9. Actualización del modelo `Channel`

La clase `Channel` debe almacenar, como mínimo:

```cpp
bool inviteOnly;
bool topicRestricted;
bool keyEnabled;
bool limitEnabled;
std::string channelKey;
std::size_t userLimit;
```

También debe disponer de una colección de operadores:

```text
operators
```

Conviene ofrecer funciones claras para consultar y modificar cada estado:

```text
isInviteOnly()
setInviteOnly()

isTopicRestricted()
setTopicRestricted()

hasKey()
getKey()
setKey()
removeKey()

hasUserLimit()
getUserLimit()
setUserLimit()
removeUserLimit()

isOperator()
addOperator()
removeOperator()
```

La lógica relacionada con los estados del canal debería permanecer dentro de `Channel`, mientras que el handler de `MODE` se encarga de:

- Validar el comando.
- Interpretar los modos.
- Buscar usuarios y canales.
- Aplicar las operaciones.
- Enviar errores.
- Notificar los cambios.

---

## 10. Notificación de los cambios

Cuando una modificación se realiza correctamente, debe notificarse a todos los miembros del canal, incluido el emisor.

Ejemplo:

```text
:roxana!roxana@localhost MODE #general +it
```

Ejemplo con argumentos:

```text
:roxana!roxana@localhost MODE #general +kl secret 10
```

Ejemplo de cambio de operador:

```text
:roxana!roxana@localhost MODE #general +o otroUsuario
```

La notificación debe incluir:

- El prefijo completo del usuario que ejecutó el comando.
- El nombre del canal.
- Los modos aplicados.
- Los argumentos correspondientes.
- La terminación `\r\n`.

No se debe notificar como aplicado un modo que haya sido rechazado.

---

## 11. Respuestas y errores relevantes

| Código | Nombre | Situación |
|---:|---|---|
| `324` | `RPL_CHANNELMODEIS` | Consulta correcta de los modos actuales |
| `401` | `ERR_NOSUCHNICK` | El usuario indicado para `+o` o `-o` no existe |
| `403` | `ERR_NOSUCHCHANNEL` | El canal no existe |
| `441` | `ERR_USERNOTINCHANNEL` | El objetivo de `+o` o `-o` no pertenece al canal |
| `442` | `ERR_NOTONCHANNEL` | El emisor no pertenece al canal |
| `461` | `ERR_NEEDMOREPARAMS` | Faltan parámetros obligatorios |
| `472` | `ERR_UNKNOWNMODE` | Se ha recibido una letra de modo desconocida |
| `482` | `ERR_CHANOPRIVSNEEDED` | El emisor no es operador del canal |

Ejemplo de modo desconocido:

```text
MODE #general +x
```

Respuesta posible:

```text
:server.name 472 roxana x :is unknown mode char to me
```

---

## 12. Integración con otros comandos

La implementación de `MODE` debe modificar el comportamiento de otros handlers.

### `JOIN`

Debe consultar:

- `+i`: comprobar si el usuario está invitado.
- `+k`: comprobar la contraseña.
- `+l`: comprobar el límite de usuarios.

### `TOPIC`

Debe consultar:

- `+t`: solamente un operador puede modificar el topic.
- `-t`: cualquier miembro puede modificarlo.

### `INVITE`

Debe consultar si el emisor tiene los privilegios necesarios cuando corresponda.

### `KICK`

Debe comprobar que el emisor es operador.

### `MODE`

El modo `+o` debe modificar la misma colección de operadores utilizada por `KICK`, `INVITE`, `TOPIC` y el propio `MODE`.

---

## 13. Orden de implementación recomendado

1. Implementar la consulta:

   ```text
   MODE #general
   ```

2. Implementar modos simples:

   ```text
   MODE #general +i
   MODE #general -i
   MODE #general +t
   MODE #general -t
   ```

3. Implementar la contraseña:

   ```text
   MODE #general +k secret
   MODE #general -k
   ```

4. Implementar el límite:

   ```text
   MODE #general +l 10
   MODE #general -l
   ```

5. Implementar los operadores:

   ```text
   MODE #general +o roxana
   MODE #general -o roxana
   ```

6. Implementar combinaciones con un único signo:

   ```text
   MODE #general +it
   MODE #general +kol secret roxana 10
   ```

7. Implementar cambios de signo dentro de la misma cadena:

   ```text
   MODE #general +o-l roxana
   MODE #general +it-k secret
   ```

8. Integrar los estados con `JOIN`, `TOPIC`, `INVITE` y `KICK`.

9. Añadir pruebas de errores y combinaciones.

---

## 14. Casos mínimos de prueba

### Consulta

```text
MODE #general
```

Debe devolver los modos actuales.

### Activación y desactivación

```text
MODE #general +i
MODE #general -i
MODE #general +t
MODE #general -t
```

### Contraseña

```text
MODE #general +k secret
JOIN #general incorrecta
JOIN #general secret
MODE #general -k
```

### Límite

```text
MODE #general +l 2
MODE #general -l
```

También deben probarse límites inválidos:

```text
MODE #general +l 0
MODE #general +l -5
MODE #general +l texto
```

### Operadores

```text
MODE #general +o roxana
MODE #general -o roxana
```

### Combinaciones

```text
MODE #general +it
MODE #general -it
MODE #general +kl secret 10
MODE #general +o-l roxana
MODE #general +kol secret roxana 10
```

### Errores

Probar:

- Canal inexistente.
- Usuario objetivo inexistente.
- Usuario objetivo fuera del canal.
- Emisor fuera del canal.
- Emisor sin privilegios de operador.
- Modo desconocido.
- Contraseña ausente.
- Límite ausente.
- Límite inválido.
- Nickname ausente para `+o`.
- Nickname ausente para `-o`.
- Combinaciones con menos argumentos de los necesarios.

---

## Resultado esperado de la fase

Al finalizar esta fase, el servidor debe ser capaz de:

- Consultar los modos activos de un canal.
- Activar y desactivar `i`, `t`, `k`, `o` y `l`.
- Consumir correctamente los parámetros de cada modo.
- Interpretar varios modos dentro del mismo comando.
- Cambiar entre `+` y `-` dentro de la misma cadena.
- Validar permisos de miembro y operador.
- Notificar los cambios a todos los miembros del canal.
- Aplicar los modos al comportamiento de `JOIN`, `TOPIC`, `INVITE` y `KICK`.
- Responder coherentemente ante modos, parámetros o usuarios inválidos.