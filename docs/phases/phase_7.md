# Fase 7 — Buffer de salida y escritura no bloqueante

## Objetivo

Implementar el envío de respuestas IRC de forma no bloqueante.

Una llamada a `send()` no garantiza que se envíen todos los datos solicitados. El sistema operativo puede aceptar solamente una parte, por lo que cada cliente debe mantener un buffer con los bytes que todavía están pendientes de envío.

El flujo general será:

```text
Se genera una respuesta IRC
        ↓
Se añade al buffer de salida del cliente
        ↓
Se activa POLLOUT
        ↓
poll() indica que el socket permite escribir
        ↓
send() intenta enviar los datos
        ↓
Se eliminan únicamente los bytes enviados
        ↓
Si quedan datos, POLLOUT continúa activo
        ↓
Si el buffer queda vacío, POLLOUT se desactiva
```

---

## 1. Añadir un buffer de salida a cada cliente

Cada objeto `Client` debe almacenar su propio buffer de salida:

```cpp
class Client
{
private:
    int _socketFileDescriptor;
    std::string _outputBuffer;
};
```

El buffer debe pertenecer al cliente porque cada conexión puede enviar datos a una velocidad diferente.

Un cliente puede recibir inmediatamente todas las respuestas, mientras que otro puede tardar más y acumular temporalmente datos pendientes.

---

## 2. Añadir las respuestas al buffer

Cuando el servidor quiera enviar una respuesta, no debe asumir que puede llamar directamente a `send()` y enviar todo el mensaje.

En su lugar, debe añadir la respuesta al buffer:

```cpp
void Client::appendToOutputBuffer(const std::string &message)
{
    _outputBuffer += message;
}
```

Ejemplo:

```cpp
client.appendToOutputBuffer(
    ":server 001 roxana :Welcome to the IRC server\r\n"
);
```

Las respuestas IRC deben terminar con `\r\n`.

Si el cliente ya tiene información pendiente, la nueva respuesta se añade al final del buffer para conservar el orden de los mensajes.

---

## 3. Consultar el buffer de salida

La clase `Client` debe permitir consultar los datos pendientes:

```cpp
const std::string &Client::getOutputBuffer() const
{
    return _outputBuffer;
}
```

También resulta útil disponer de una función que indique si existen datos pendientes:

```cpp
bool Client::hasPendingOutput() const
{
    return !_outputBuffer.empty();
}
```

---

## 4. Activar `POLLOUT` cuando existan datos pendientes

`POLLOUT` indica que el socket está preparado para aceptar datos mediante `send()`.

Solo debe activarse cuando el cliente tenga información pendiente:

```cpp
pollFileDescriptor.events = POLLIN;

if (client.hasPendingOutput())
    pollFileDescriptor.events |= POLLOUT;
```

Los eventos de un cliente tendrán estas funciones:

- `POLLIN`: indica que existen datos disponibles para leer.
- `POLLOUT`: indica que se puede intentar enviar información pendiente.

Activar `POLLOUT` no garantiza que todo el buffer pueda enviarse. Únicamente indica que tiene sentido intentar llamar a `send()`.

---

## 5. Comprobar `POLLOUT` después de `poll()`

Después de ejecutar `poll()`, debe comprobarse el campo `revents`:

```cpp
if (pollFileDescriptor.revents & POLLOUT)
    handleClientWrite(pollFileDescriptor.fd);
```

La función encargada de la escritura debe intentar enviar el contenido actual del buffer:

```cpp
ssize_t sentByteCount = send(
    client.getSocketFileDescriptor(),
    client.getOutputBuffer().data(),
    client.getOutputBuffer().size(),
    MSG_NOSIGNAL
);
```

El valor devuelto por `send()` indica cuántos bytes se han enviado realmente.

---

## 6. Gestionar envíos parciales

`send()` puede enviar menos bytes de los solicitados.

Ejemplo:

```text
Tamaño inicial del outputBuffer: 200 bytes
Bytes enviados por send():       80 bytes
Bytes que quedan pendientes:     120 bytes
```

En este caso deben eliminarse únicamente los primeros 80 bytes:

```cpp
void Client::removeSentOutput(std::size_t sentByteCount)
{
    if (sentByteCount >= _outputBuffer.size())
    {
        _outputBuffer.clear();
        return;
    }

    _outputBuffer.erase(0, sentByteCount);
}
```

Uso:

```cpp
if (sentByteCount > 0)
{
    client.removeSentOutput(
        static_cast<std::size_t>(sentByteCount)
    );
}
```

Nunca debe vaciarse todo el buffer sin comprobar cuántos bytes ha enviado realmente `send()`.

---

## 7. Gestionar los resultados de `send()`

### Envío correcto

Si `send()` devuelve un valor mayor que cero, esa cantidad de bytes se ha enviado correctamente:

```cpp
if (sentByteCount > 0)
{
    client.removeSentOutput(
        static_cast<std::size_t>(sentByteCount)
    );

    return;
}
```

### Socket temporalmente no disponible

Si `send()` devuelve `-1` y `errno` contiene `EAGAIN` o `EWOULDBLOCK`, el socket no puede aceptar más datos en ese momento:

```cpp
if (
    sentByteCount == -1
    && (errno == EAGAIN || errno == EWOULDBLOCK)
)
{
    return;
}
```

No debe cerrarse la conexión ni modificarse el buffer.

Los datos permanecerán almacenados y el servidor volverá a intentarlo cuando `poll()` indique de nuevo `POLLOUT`.

### Llamada interrumpida por una señal

Si `errno` contiene `EINTR`, la llamada fue interrumpida por una señal:

```cpp
if (sentByteCount == -1 && errno == EINTR)
    return;
```

Tampoco deben eliminarse datos del buffer.

### Error definitivo

Cualquier otro error normalmente indica un problema real con la conexión:

```cpp
if (sentByteCount == -1)
{
    disconnectClient(
        client.getSocketFileDescriptor()
    );
}
```

La eliminación del cliente debe realizarse de forma segura, evitando invalidar iteradores que todavía estén en uso.

---

## 8. Desactivar `POLLOUT` cuando el buffer quede vacío

Cuando se hayan enviado todos los datos, debe dejar de vigilarse `POLLOUT`:

```cpp
pollFileDescriptor.events &= ~POLLOUT;
```

Otra opción es reconstruir los eventos del cliente en cada iteración:

```cpp
pollFileDescriptor.events = POLLIN;

if (client.hasPendingOutput())
    pollFileDescriptor.events |= POLLOUT;
```

Esto evita mantener `POLLOUT` activo cuando no hay nada que enviar.

Un socket suele estar disponible para escribir durante la mayor parte del tiempo. Si `POLLOUT` permanece siempre activo, `poll()` puede despertarse constantemente aunque no exista trabajo pendiente.

Esto puede provocar:

- Consumo innecesario de CPU.
- Iteraciones inútiles del bucle principal.
- Un bucle de espera activa.

---

## 9. Evitar `SIGPIPE`

Si se llama a `send()` sobre un socket cuya conexión ha sido cerrada, el proceso puede recibir la señal `SIGPIPE`.

En Linux puede evitarse utilizando `MSG_NOSIGNAL`:

```cpp
ssize_t sentByteCount = send(
    client.getSocketFileDescriptor(),
    client.getOutputBuffer().data(),
    client.getOutputBuffer().size(),
    MSG_NOSIGNAL
);
```

De esta forma, `send()` devolverá un error que podrá gestionarse sin que el servidor termine inesperadamente.

---

## 10. Limitar el tamaño del buffer de salida

Es recomendable establecer un tamaño máximo para impedir que un cliente lento acumule mensajes indefinidamente:

```cpp
const std::size_t MAXIMUM_OUTPUT_BUFFER_SIZE = 65536;
```

Antes de añadir una respuesta:

```cpp
if (
    client.getOutputBuffer().size() + message.size()
    > MAXIMUM_OUTPUT_BUFFER_SIZE
)
{
    disconnectClient(
        client.getSocketFileDescriptor()
    );

    return;
}

client.appendToOutputBuffer(message);
```

Esto protege al servidor frente a clientes que no leen las respuestas y provocan un crecimiento continuo del consumo de memoria.

---

## Estructura mínima recomendada para `Client`

```cpp
class Client
{
private:
    int _socketFileDescriptor;
    std::string _outputBuffer;

public:
    void appendToOutputBuffer(
        const std::string &message
    );

    const std::string &getOutputBuffer() const;

    bool hasPendingOutput() const;

    void removeSentOutput(
        std::size_t sentByteCount
    );
};
```

---

## Responsabilidades del servidor

El servidor debe encargarse de:

1. Generar la respuesta IRC.
2. Añadirla al buffer de salida del cliente.
3. Activar `POLLOUT`.
4. Esperar a que `poll()` indique que el socket permite escribir.
5. Llamar a `send()`.
6. Eliminar únicamente los bytes realmente enviados.
7. Mantener `POLLOUT` activo si todavía quedan datos.
8. Desactivar `POLLOUT` cuando el buffer quede vacío.
9. Gestionar `EAGAIN`, `EWOULDBLOCK` y `EINTR`.
10. Desconectar al cliente ante errores definitivos.
11. Limitar el tamaño máximo del buffer.

---

## Resultado esperado de la fase

Al terminar esta fase, el servidor debe ser capaz de:

- Mantener un buffer de salida independiente para cada cliente.
- Añadir respuestas al buffer sin bloquear el servidor.
- Enviar información únicamente cuando `poll()` indique `POLLOUT`.
- Gestionar correctamente envíos parciales.
- Conservar los bytes que todavía no se hayan enviado.
- Reintentar los envíos pendientes.
- Desactivar `POLLOUT` cuando el buffer quede vacío.
- Evitar consumo innecesario de CPU.
- Gestionar correctamente los errores de `send()`.
- Evitar que `SIGPIPE` cierre inesperadamente el servidor.
- Limitar la memoria consumida por clientes lentos.
- Mantener el orden correcto de las respuestas IRC.