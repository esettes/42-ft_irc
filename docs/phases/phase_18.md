# Fase 18 — Limpieza de estado y desconexiones

## Objetivo

Implementar una eliminación segura y completa de los clientes desconectados.

Esta fase es crítica porque un cliente puede estar referenciado desde varias estructuras del servidor:

- Lista utilizada por `poll()`.
- Mapa de clientes conectados.
- Índice global de nicknames.
- Canales a los que pertenece.
- Listas de operadores.
- Listas de invitados.
- Buffers de entrada y salida.

Una desconexión incompleta puede dejar referencias inválidas, provocar comportamientos incoherentes o producir errores de memoria.

---

## Función central de desconexión

Toda desconexión debe pasar por una única función:

```cpp
void Server::disconnectClient(
    int clientFileDescriptor,
    const std::string &reason
);
```

Esta función debe ser la responsable de eliminar completamente al cliente del servidor.

Debe utilizarse cuando:

- El cliente envía `QUIT`.
- `recv()` devuelve `0`.
- `recv()` devuelve un error irrecuperable.
- Se detecta una desconexión mediante `poll()`.
- El servidor necesita cerrar explícitamente la conexión.

No se debe duplicar esta lógica en diferentes handlers.

---

## Flujo recomendado

```text
Localizar al cliente
        ↓
Construir el mensaje QUIT
        ↓
Obtener los clientes que deben recibirlo
        ↓
Eliminar al cliente de todos los canales
        ↓
Eliminarlo de operadores e invitados
        ↓
Eliminar los canales vacíos
        ↓
Eliminar su nickname del índice global
        ↓
Eliminarlo de poll()
        ↓
Cerrar su descriptor
        ↓
Eliminarlo del mapa de clientes
```

---

## 1. Localizar al cliente

Antes de comenzar, debe comprobarse que el descriptor corresponde a un cliente existente.

```cpp
std::map<int, Client>::iterator clientIterator;

clientIterator = clients.find(clientFileDescriptor);
if (clientIterator == clients.end())
{
    return;
}
```

La función debe tolerar que se intente desconectar un descriptor que ya ha sido eliminado, evitando dobles cierres y accesos inválidos.

---

## 2. Construir el mensaje `QUIT`

Si el cliente estaba registrado, debe construirse el mensaje que recibirán los demás usuarios:

```text
:nickname!username@hostname QUIT :Connection closed
```

Si la desconexión procede del comando:

```text
QUIT :Leaving
```

El motivo proporcionado por el usuario debe conservarse:

```text
:nickname!username@hostname QUIT :Leaving
```

El mensaje debe construirse antes de destruir el objeto `Client`, porque después ya no estarán disponibles su nickname, username y hostname.

---

## 3. Determinar quién debe recibir el `QUIT`

El mensaje `QUIT` debe enviarse a los usuarios que compartan al menos un canal con el cliente desconectado.

Un mismo usuario puede compartir varios canales con él, pero debe recibir el mensaje una sola vez.

Para evitar duplicados, conviene recopilar los destinatarios en un `std::set`:

```cpp
std::set<int> recipientFileDescriptors;
```

Por cada canal al que pertenezca el cliente:

1. Recorrer sus miembros.
2. Excluir al cliente que se está desconectando.
3. Añadir el descriptor de cada miembro al conjunto.

Después se envía el mensaje `QUIT` a todos los destinatarios recopilados.

---

## 4. Eliminar al cliente de todos los canales

El cliente debe desaparecer de todas las colecciones internas de cada canal:

- Miembros.
- Operadores.
- Invitados.

Después de la eliminación deben cumplirse siempre estas condiciones:

- Ningún operador puede existir si no es miembro del canal.
- Ninguna invitación debe apuntar a un cliente inexistente.
- Ningún canal debe mantener referencias al cliente desconectado.

Puede resultar útil implementar una función auxiliar:

```cpp
void Channel::removeClient(int clientFileDescriptor);
```

Esta función puede encargarse de eliminar al cliente de:

- `members`.
- `operators`.
- `invitedClients`.

---

## 5. Eliminar canales vacíos

Después de retirar al cliente, cada canal debe comprobarse:

```cpp
if (channel.isEmpty())
{
    // Remove the channel from the server.
}
```

Los canales que se queden sin miembros deben eliminarse del mapa global del servidor.

No se deben borrar elementos de un contenedor mientras se recorre incorrectamente, porque eso puede invalidar el iterador.

Una forma segura es guardar el siguiente iterador antes de borrar:

```cpp
std::map<std::string, Channel>::iterator channelIterator;
std::map<std::string, Channel>::iterator nextChannelIterator;

channelIterator = channels.begin();
while (channelIterator != channels.end())
{
    nextChannelIterator = channelIterator;
    ++nextChannelIterator;

    channelIterator->second.removeClient(clientFileDescriptor);

    if (channelIterator->second.isEmpty())
    {
        channels.erase(channelIterator);
    }

    channelIterator = nextChannelIterator;
}
```

Otra opción es recopilar primero los nombres de los canales vacíos y eliminarlos después.

---

## 6. Eliminar el nickname

Si existe un índice global como:

```cpp
std::map<std::string, int> nicknameIndex;
```

Debe eliminarse la entrada correspondiente al nickname del cliente:

```cpp
nicknameIndex.erase(client.getNickname());
```

La eliminación debe realizarse antes de destruir el objeto `Client`.

Si el cliente todavía no había enviado `NICK`, no habrá ninguna entrada que eliminar.

---

## 7. Eliminar el descriptor de `poll()`

El descriptor debe eliminarse del contenedor de estructuras `pollfd`.

Si se utiliza un `std::vector<pollfd>`, hay que buscar la estructura cuyo campo `fd` coincida con el descriptor del cliente.

```cpp
for (std::vector<pollfd>::iterator iterator = pollFileDescriptors.begin();
     iterator != pollFileDescriptors.end();
     ++iterator)
{
    if (iterator->fd == clientFileDescriptor)
    {
        pollFileDescriptors.erase(iterator);
        break;
    }
}
```

Después de eliminarlo, el servidor no debe volver a procesar eventos asociados a ese descriptor durante la misma iteración del bucle.

---

## 8. Cerrar el descriptor

Una vez eliminado de las estructuras del servidor, debe cerrarse el socket:

```cpp
::close(clientFileDescriptor);
```

El descriptor debe cerrarse una sola vez.

Después de llamar a `close()`, no debe volver a utilizarse para:

- Leer datos.
- Enviar mensajes.
- Buscar al cliente.
- Modificar eventos de `poll()`.
- Acceder a buffers.

---

## 9. Eliminar el cliente del mapa

Finalmente, el cliente puede eliminarse del contenedor principal:

```cpp
clients.erase(clientFileDescriptor);
```

Esta operación debe realizarse después de haber utilizado toda la información necesaria del objeto, como:

- Nickname.
- Username.
- Hostname.
- Canales.
- Prefijo IRC.

Después de `erase()`, cualquier referencia, puntero o iterador al cliente deja de ser válido.

---

## Tratamiento de `recv()`

Cuando `recv()` devuelve `0`, significa que el cliente ha cerrado la conexión:

```cpp
ssize_t receivedBytes;

receivedBytes = recv(
    clientFileDescriptor,
    buffer,
    sizeof(buffer),
    0
);

if (receivedBytes == 0)
{
    disconnectClient(
        clientFileDescriptor,
        "Connection closed"
    );
    return;
}
```

Si `recv()` devuelve `-1`, debe comprobarse `errno`.

Los errores temporales no deben desconectar al cliente:

- `EAGAIN`.
- `EWOULDBLOCK`.
- `EINTR`.

Otros errores pueden tratarse como una desconexión:

```cpp
if (receivedBytes == -1)
{
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return;
    }

    if (errno == EINTR)
    {
        return;
    }

    disconnectClient(
        clientFileDescriptor,
        "Connection error"
    );
    return;
}
```

---

## Tratamiento de eventos de `poll()`

Los eventos que pueden indicar una conexión cerrada o inválida incluyen:

- `POLLHUP`.
- `POLLERR`.
- `POLLNVAL`.

Ejemplo:

```cpp
if (pollFileDescriptor.revents & (POLLHUP | POLLERR | POLLNVAL))
{
    disconnectClient(
        pollFileDescriptor.fd,
        "Connection closed"
    );
    continue;
}
```

Debe evitarse seguir procesando el mismo cliente después de llamar a `disconnectClient()`.

---

## Integración con `QUIT`

El handler de `QUIT` no debe implementar manualmente toda la limpieza.

Su responsabilidad debe limitarse a:

1. Obtener el motivo opcional.
2. Llamar a `disconnectClient()`.
3. Finalizar inmediatamente el procesamiento del cliente.

```cpp
void Server::handleQuit(
    Client &client,
    const IrcMessage &message
)
{
    std::string reason = "Client Quit";

    if (!message.parameters.empty())
    {
        reason = message.parameters[0];
    }

    disconnectClient(
        client.getFileDescriptor(),
        reason
    );
}
```

Después de llamar a `disconnectClient()`, el handler no debe volver a acceder a `client`, porque la referencia puede haber quedado invalidada.

---

## Diferencia entre `QUIT` y `KICK`

`QUIT` desconecta completamente al cliente del servidor.

`KICK` solamente expulsa a un usuario de un canal:

```text
KICK #general roxana :Reason
```

Por tanto, `KICK` no debe llamar a `disconnectClient()`.

Puede reutilizar una función auxiliar de eliminación de canales:

```cpp
void Server::removeClientFromChannel(
    Client &client,
    Channel &channel
);
```

Esta función debe:

- Eliminar al cliente de los miembros del canal.
- Eliminar sus privilegios de operador en ese canal.
- Eliminarlo de la lista de invitados si corresponde.
- Eliminar el canal si queda vacío.

El cliente debe continuar conectado al servidor y puede seguir utilizando otros canales.

---

## Evitar iteradores invalidados

La desconexión puede modificar contenedores que están siendo recorridos, especialmente:

- El vector de `pollfd`.
- El mapa de clientes.
- El mapa de canales.
- Las colecciones de miembros.
- Las colecciones de operadores.
- Las colecciones de invitados.

Nunca debe incrementarse o utilizarse un iterador después de que su elemento haya sido eliminado.

Cuando se borra durante un recorrido, debe usarse una estrategia segura:

- Guardar el siguiente iterador antes de borrar.
- Utilizar el iterador devuelto por `erase()` si el estándar disponible lo permite.
- Guardar primero los elementos que deben eliminarse y borrarlos después.

En C++98, guardar el siguiente iterador antes de llamar a `erase()` suele ser la opción más sencilla y portable.

---

## Evitar punteros y referencias colgantes

Si los canales almacenan punteros o referencias a objetos `Client`, deben eliminarse antes de borrar el cliente del mapa principal.

Orden obligatorio:

```text
Eliminar referencias desde canales
        ↓
Eliminar el objeto Client
```

Nunca debe hacerse:

```text
Eliminar el objeto Client
        ↓
Intentar retirarlo de los canales
```

La segunda secuencia obligaría a acceder a un objeto que ya no existe.

Una alternativa más robusta es que los canales almacenen identificadores estables, como descriptores, en lugar de punteros directos.

---

## Desconexiones durante el bucle de eventos

Si el servidor recorre el vector de `pollfd` mediante índices y elimina un elemento, las posiciones posteriores se desplazan.

Por ejemplo:

```text
Antes:  [server][client A][client B][client C]
Borrar:                  [client B]
Después:[server][client A][client C]
```

Si el índice se incrementa inmediatamente, `client C` podría no procesarse.

Las soluciones posibles son:

- No incrementar el índice cuando se elimina el elemento actual.
- Recorrer el vector en orden inverso.
- Marcar los clientes que deben desconectarse y eliminarlos después.
- Hacer que `disconnectClient()` indique si el vector ha cambiado.

---

## Invariantes que deben mantenerse

Después de cada desconexión deben cumplirse estas reglas:

- Todo descriptor presente en `poll()` corresponde a una conexión válida.
- Todo cliente conectado aparece una sola vez en el mapa de clientes.
- Todo nickname registrado pertenece a un cliente existente.
- Todo miembro de un canal pertenece a un cliente conectado.
- Todo operador también es miembro del canal.
- Toda invitación pertenece a un cliente conectado.
- Ningún canal vacío permanece almacenado.
- Ningún descriptor se cierra más de una vez.
- Ningún destinatario recibe dos veces el mismo mensaje `QUIT`.

---

## Casos de prueba recomendados

### Desconexión normal

```text
QUIT :Leaving
```

Comprobar que:

- Los demás usuarios reciben `QUIT`.
- El socket se cierra.
- El cliente desaparece de `poll()`.
- El nickname queda disponible.
- El cliente desaparece de todos sus canales.

### Cierre inesperado

Cerrar `netcat` o el cliente IRC sin enviar `QUIT`.

Comprobar que:

- `recv()` devuelve `0`.
- Se ejecuta la misma limpieza.
- Los demás miembros reciben la notificación.
- No quedan referencias al cliente.

### Usuario presente en varios canales

Añadir un usuario a varios canales y desconectarlo.

Comprobar que:

- Desaparece de todos los canales.
- Desaparece de todas las listas de operadores.
- Los usuarios que compartían varios canales reciben un solo `QUIT`.

### Último miembro de un canal

Desconectar al único miembro.

Comprobar que:

- El canal queda vacío.
- El canal se elimina del servidor.

### Operador desconectado

Desconectar a un operador.

Comprobar que:

- Se elimina de `operators`.
- No queda registrado como operador después de abandonar el canal.
- El canal sigue funcionando si todavía tiene miembros.

### Usuario invitado

Invitar a un usuario y desconectarlo antes de que haga `JOIN`.

Comprobar que:

- Se elimina de `invitedClients`.
- No queda ninguna referencia inválida.

### Desconexión doble

Intentar desconectar dos veces el mismo descriptor.

Comprobar que:

- No se produce un doble `close()`.
- No se accede a un cliente inexistente.
- El servidor continúa funcionando.

### Varios clientes desconectados durante el mismo `poll()`

Cerrar varias conexiones casi simultáneamente.

Comprobar que:

- Ningún evento se salta por el desplazamiento del vector.
- Todos los clientes se eliminan correctamente.
- No se invalidan índices o iteradores.

---

## Resultado esperado

Al terminar esta fase, cualquier tipo de desconexión debe dejar el servidor en un estado completamente coherente:

```text
Cliente desconectado
        ↓
Sin descriptor en poll()
Sin entrada en clients
Sin nickname reservado
Sin pertenencia a canales
Sin privilegios de operador
Sin invitaciones pendientes
Sin canales vacíos
Sin referencias colgantes
```

La regla principal de esta fase es:

> Toda desconexión completa debe ejecutarse mediante `Server::disconnectClient()`, y ninguna función debe continuar utilizando al cliente después de llamarla.