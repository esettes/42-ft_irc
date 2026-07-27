# Fase 5 — Reconstrucción del flujo TCP

## Objetivo

Implementar la reconstrucción de mensajes IRC completos a partir de los bytes recibidos mediante `recv()`.

TCP transmite un **flujo continuo de bytes** y no conserva la separación entre los mensajes enviados. Por eso, una llamada a `recv()` puede devolver:

- Una parte de un comando.
- Un comando completo.
- Varios comandos juntos.
- Uno o varios comandos completos junto con parte del siguiente.

Los datos recibidos no deben enviarse directamente al parser IRC.

---

## 1. Añadir un buffer persistente a cada cliente

Cada objeto `Client` debe almacenar los datos recibidos que todavía no formen un comando completo:

```cpp
std::string _inputBuffer;
```

El buffer debe pertenecer al cliente porque cada conexión TCP tiene su propio flujo de datos.

No debe existir un único buffer compartido entre todos los clientes.

---

## 2. Añadir los datos recibidos al buffer

Cuando `recv()` devuelva una cantidad positiva de bytes, estos deben añadirse al final del buffer del cliente:

```text
recv()
   ↓
bytes recibidos
   ↓
inputBuffer
```

Conceptualmente:

```cpp
client.appendInput(receivedData, receivedByteCount);
```

Los datos que ya estaban almacenados no deben eliminarse hasta que se haya reconstruido una línea IRC completa.

---

## 3. Interpretar el resultado de `recv()`

El valor devuelto por `recv()` debe gestionarse de la siguiente manera:

- `receivedByteCount > 0`: se han recibido datos y deben añadirse al buffer.
- `receivedByteCount == 0`: el cliente ha cerrado la conexión.
- `receivedByteCount == -1`: se ha producido un error o no hay más datos disponibles.

En sockets no bloqueantes:

- `EAGAIN` y `EWOULDBLOCK` indican que actualmente no quedan más datos disponibles. No debe desconectarse al cliente.
- `EINTR` indica que la llamada fue interrumpida por una señal. La operación puede volver a intentarse.
- Otros errores pueden requerir cerrar la conexión del cliente.

---

## 4. Extraer únicamente líneas completas

Los mensajes IRC terminan normalmente con:

```text
\r\n
```

Después de añadir los bytes recibidos, debe buscarse este terminador dentro de `_inputBuffer`.

Mientras exista al menos una línea completa:

1. Localizar la posición de `\r\n`.
2. Extraer el contenido anterior al terminador.
3. Eliminar del buffer la línea extraída y su `\r\n`.
4. Entregar la línea completa al siguiente nivel del servidor.
5. Repetir el proceso por si existen más comandos completos.

Los datos situados después del último terminador deben permanecer en el buffer para la siguiente llamada a `recv()`.

---

## 5. Gestionar comandos fragmentados

TCP puede dividir un comando entre varias recepciones.

Ejemplo:

```text
Primer recv():   "PRIV"
Segundo recv():  "MSG #general :Hola"
Tercer recv():   "\r\n"
```

Evolución del buffer:

```text
"PRIV"
"PRIVMSG #general :Hola"
"PRIVMSG #general :Hola\r\n"
```

Solo después de recibir `\r\n` debe extraerse:

```text
PRIVMSG #general :Hola
```

El parser nunca debe recibir por separado:

```text
PRIV
MSG #general :Hola
```

El test del subject divide intencionadamente una palabra entre varios envíos para comprobar que el servidor reconstruye correctamente el flujo TCP.

---

## 6. Gestionar varios comandos en una recepción

También pueden recibirse varios comandos en una única llamada a `recv()`:

```text
PASS secret\r\nNICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```

El sistema debe extraer tres líneas independientes:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

No basta con buscar un único terminador. La extracción debe repetirse mientras el buffer contenga líneas completas.

---

## 7. Conservar los fragmentos incompletos

Una recepción puede contener comandos completos y parte del siguiente:

```text
PASS secret\r\nNICK roxana\r\nUS
```

Deben extraerse:

```text
PASS secret
NICK roxana
```

El buffer debe conservar:

```text
US
```

Si posteriormente llega:

```text
ER roxana 0 * :Roxana\r\n
```

El buffer pasará a contener:

```text
USER roxana 0 * :Roxana\r\n
```

Entonces podrá extraerse:

```text
USER roxana 0 * :Roxana
```

---

## 8. Separar framing y parsing

En esta fase todavía no es necesario interpretar la estructura interna de los comandos IRC.

Debe mantenerse el siguiente flujo:

```text
recv()
   ↓
añadir bytes al buffer
   ↓
buscar terminadores \r\n
   ↓
extraer líneas completas
   ↓
enviar cada línea completa al parser
```

Responsabilidades:

- `recv()` obtiene bytes del socket.
- `Client` conserva los bytes pendientes.
- El sistema de **framing** reconstruye líneas completas.
- El parser interpreta posteriormente cada línea IRC.

Regla arquitectónica principal:

> `recv()` no debe llamar directamente al parser con los bytes que acaba de recibir.

El parser solamente debe recibir líneas IRC completas.

---

## 9. Métodos recomendados para `Client`

La clase `Client` puede proporcionar los siguientes métodos:

```cpp
void appendInput(
    const char *receivedData,
    std::size_t receivedByteCount
);

bool extractNextLine(std::string &line);
```

### Método `appendInput()`

Debe añadir al buffer exactamente la cantidad de bytes indicada por `receivedByteCount`.

No debe asumir que los datos recibidos terminan en `'\0'`.

Ejemplo conceptual:

```cpp
void Client::appendInput(
    const char *receivedData,
    std::size_t receivedByteCount
)
{
    _inputBuffer.append(receivedData, receivedByteCount);
}
```

### Método `extractNextLine()`

Debe:

- Buscar el siguiente `\r\n`.
- Devolver `false` si todavía no existe una línea completa.
- Guardar la línea extraída en el parámetro `line`.
- Eliminar del buffer la línea y su terminador.
- Devolver `true` cuando se haya extraído correctamente una línea.

Ejemplo conceptual:

```cpp
bool Client::extractNextLine(std::string &line)
{
    const std::size_t lineEndingPosition =
        _inputBuffer.find("\r\n");

    if (lineEndingPosition == std::string::npos)
        return false;

    line = _inputBuffer.substr(0, lineEndingPosition);

    _inputBuffer.erase(
        0,
        lineEndingPosition + 2
    );

    return true;
}
```

Uso conceptual:

```cpp
std::string line;

while (client.extractNextLine(line))
{
    processCompleteLine(client, line);
}
```

---

## 10. Controlar el tamaño de las líneas

Un cliente podría enviar datos indefinidamente sin incluir ningún terminador:

```text
AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA...
```

Si no se establece un límite, `_inputBuffer` podría crecer indefinidamente y consumir toda la memoria del servidor.

Un mensaje IRC tradicional puede ocupar como máximo:

```text
512 bytes incluyendo \r\n
```

Por tanto, el contenido anterior al terminador puede ocupar como máximo:

```text
510 bytes
```

Debe definirse una política para entradas excesivas, por ejemplo:

- Rechazar la línea.
- Limpiar el buffer.
- Desconectar al cliente.

Para una primera implementación, desconectar al cliente que envíe una entrada excesiva es una solución sencilla y segura.

También puede establecerse un límite general para el buffer pendiente como protección adicional.

---

## 11. Pruebas necesarias

### Prueba 1 — Comando completo

Enviar:

```text
NICK roxana\r\n
```

Resultado esperado:

```text
NICK roxana
```

---

### Prueba 2 — Comando fragmentado

Enviar por separado:

```text
"NI"
"CK ro"
"xana\r"
"\n"
```

Resultado esperado:

```text
NICK roxana
```

Debe extraerse una única línea y solamente después de recibir el `\n` final.

---

### Prueba 3 — Varios comandos juntos

Enviar:

```text
PASS secret\r\nNICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```

Resultado esperado:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

---

### Prueba 4 — Comandos completos y fragmento pendiente

Enviar:

```text
PASS secret\r\nNICK roxana\r\nUS
```

Deben extraerse:

```text
PASS secret
NICK roxana
```

El buffer debe conservar:

```text
US
```

Después, enviar:

```text
ER roxana 0 * :Roxana\r\n
```

Debe extraerse:

```text
USER roxana 0 * :Roxana
```

---

### Prueba 5 — Cliente desconectado

Si `recv()` devuelve `0`, el servidor debe:

1. Cerrar el file descriptor del cliente.
2. Eliminarlo de la colección de clientes.
3. Eliminar su entrada correspondiente de `poll()`.
4. Liberar cualquier recurso asociado.

---

### Prueba 6 — Entrada sin terminador

Enviar una gran cantidad de datos sin `\r\n`.

El servidor debe impedir que el buffer crezca indefinidamente y aplicar la política definida para entradas excesivas.

---

## Criterio de finalización

La fase estará completada cuando:

- Cada cliente tenga su propio buffer de entrada persistente.
- Los bytes recibidos se añadan al buffer correspondiente.
- Los comandos fragmentados se reconstruyan correctamente.
- Se puedan extraer varios comandos recibidos juntos.
- Los fragmentos incompletos permanezcan almacenados.
- La extracción se repita mientras existan líneas completas.
- El parser solamente reciba líneas IRC completas.
- Se gestionen correctamente la desconexión y los errores de `recv()`.
- El buffer no pueda crecer indefinidamente.