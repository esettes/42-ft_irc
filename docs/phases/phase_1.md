# Fase 1 — Esqueleto y ciclo de vida del servidor

El objetivo de esta fase es preparar cómo se inicia, permanece ejecutándose y termina el servidor. Todavía no se crean conexiones ni se procesan comandos IRC.

## 1. Estructura mínima

```text
ft_irc/
├── include/
│   ├── Server.hpp
│   └── SignalHandler.hpp
├── src/
│   ├── main.cpp
│   ├── Server.cpp
│   └── SignalHandler.cpp
└── Makefile
```

Cada archivo debe tener una responsabilidad clara:

- `main.cpp`: valida los argumentos y ejecuta el servidor.
- `Server`: administra el ciclo de vida y los recursos.
- `SignalHandler`: detecta solicitudes de cierre.
- `Makefile`: compila el proyecto.

## 2. Validar la cantidad de argumentos

El programa se ejecuta así:

```bash
./ircserv <port> <password>
```

Por tanto, `main()` debe recibir exactamente tres elementos:

```cpp
if (argumentCount != 3)
{
    printUsage(argumentValues[0]);
    return 1;
}
```

`argumentValues[0]` es el nombre del programa.

## 3. Validar el puerto

El puerto debe:

- No estar vacío.
- Contener únicamente dígitos.
- Poder convertirse sin desbordamiento.
- Estar entre `1` y `65535`.

No conviene usar únicamente `atoi()`, porque acepta parcialmente textos inválidos:

```text
atoi("12abc") → 12
```

Es preferible comprobar los caracteres y utilizar `strtol()`.

## 4. Validar la contraseña

La contraseña no puede estar vacía:

```cpp
if (passwordArgument.empty())
{
    throw std::invalid_argument(
        "password cannot be empty"
    );
}
```

No necesitas imponer todavía una longitud mínima ni caracteres especiales.

## 5. Responsabilidad de `main()`

`main()` debe limitarse a:

```text
Validar argumentos
    → instalar señales
    → construir Server
    → ejecutar Server
    → capturar excepciones
```

No debe encargarse de:

- Procesar comandos IRC.
- Crear clientes.
- Ejecutar directamente la lógica de `poll()`.
- Cerrar manualmente los recursos internos de `Server`.

## 6. Estado inicial de `Server`

Como mínimo, `Server` debe guardar:

```cpp
int _port;
std::string _password;
int _listeningSocketFileDescriptor;
```

El constructor debe usar una lista de inicialización:

```cpp
Server::Server(
    int port,
    const std::string &password
)
    : _port(port),
      _password(password),
      _listeningSocketFileDescriptor(-1)
{
}
```

El servidor guarda su propia copia de la contraseña.

## 7. Descriptores inicializados a `-1`

Un descriptor válido puede ser `0`, por lo que no debes utilizarlo para representar “sin socket”.

```cpp
const int INVALID_FILE_DESCRIPTOR = -1;
```

La evolución habitual será:

```text
Antes de socket():   -1
Después de socket():  3
Después de close():  -1
```

Esto ayuda a impedir cierres duplicados.

## 8. Impedir la copia de `Server`

Copiar un `Server` podría provocar que dos objetos creyeran ser propietarios del mismo socket.

En C++98 se impide declarando como privados:

```cpp
Server(const Server &other);
Server &operator=(const Server &other);
```

No deben implementarse ni utilizarse.

## 9. Gestionar señales

Debes controlar:

- `SIGINT`: se recibe normalmente con `Ctrl+C`.
- `SIGTERM`: solicita la terminación del proceso.
- `SIGPIPE`: debe ignorarse para que un envío a un cliente desconectado no termine todo el servidor.

La instalación puede hacerse desde:

```cpp
SignalHandler::install();
```

## 10. Usar una bandera de finalización

El manejador de señales solamente debe modificar una bandera:

```cpp
static volatile sig_atomic_t _shutdownRequested;
```

El manejador debe ser mínimo:

```cpp
void SignalHandler::handleTerminationSignal(
    int signalNumber
)
{
    static_cast<void>(signalNumber);
    _shutdownRequested = 1;
}
```

No debe:

- Cerrar sockets.
- Lanzar excepciones.
- Utilizar `std::cout`.
- Ejecutar lógica compleja.

## 11. Ciclo temporal de ejecución

Como todavía no existe un socket de escucha, puedes utilizar temporalmente:

```cpp
void Server::run()
{
    while (!SignalHandler::isShutdownRequested())
    {
        const int pollResult = ::poll(
            NULL,
            0,
            1000
        );

        if (pollResult == -1 && errno != EINTR)
        {
            throw createSystemError(
                "poll",
                errno
            );
        }
    }
}
```

`poll(NULL, 0, 1000)` espera un segundo sin consumir continuamente la CPU.

## 12. Tratar correctamente `EINTR`

Cuando una señal interrumpe `poll()`, puede devolver:

```text
-1
```

con:

```cpp
errno == EINTR
```

Esto no es un fallo real. Después de la interrupción, el bucle vuelve a comprobar la bandera y termina.

## 13. Limpieza mediante el destructor

El destructor de `Server` debe iniciar la limpieza:

```cpp
Server::~Server()
{
    closeAllFileDescriptors();
}
```

Se ejecutará automáticamente:

- Cuando `run()` termine normalmente.
- Cuando se lance una excepción después de construir `Server`.
- Cuando se abandone el ámbito donde se creó el objeto.

No debes llamar manualmente a:

```cpp
server.~Server();
```

## 14. Cerrar cada descriptor una sola vez

Antes de cerrar un descriptor, comprueba que no sea `-1`:

```cpp
void Server::closeFileDescriptor(int &fileDescriptor)
{
    if (fileDescriptor == -1)
        return;

    const int descriptorToClose = fileDescriptor;

    fileDescriptor = -1;

    if (::close(descriptorToClose) == -1)
    {
        // Report warning without throwing
    }
}
```

El parámetro es una referencia porque también se modifica el atributo original.

La secuencia es:

```text
Guardar el descriptor
    → marcar el atributo como inválido
    → llamar a close()
    → no volver a cerrarlo
```

## 15. No lanzar excepciones desde el destructor

Si `close()` falla durante la destrucción, muestra una advertencia, pero no lances otra excepción.

Una excepción lanzada mientras ya se está procesando otra podría terminar el programa mediante:

```cpp
std::terminate();
```

## 16. Propiedad de los descriptores

Define desde ahora quién es responsable de cerrarlos:

| Recurso | Propietario | Quién lo cierra |
|---|---|---|
| Socket de escucha | `Server` | `Server` |
| Socket de cliente | `Server` | `Server` |
| Objeto `Client` | `Server` | `Server` |
| Entrada `pollfd` | No es propietaria | Nadie |
| Descriptor guardado en `Client` | Solo identifica la conexión | `Server` |

Eliminar un `pollfd` de un vector no cierra el socket.

La futura secuencia de desconexión será:

```text
Detectar desconexión
    → quitar descriptor de poll
    → cerrar descriptor
    → eliminar Client
```

## 17. Compilación

El proyecto debe compilar con:

```makefile
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
```

Archivos mínimos:

```makefile
SOURCES = src/main.cpp \
          src/Server.cpp \
          src/SignalHandler.cpp
```

## 18. Pruebas necesarias

### Argumentos incorrectos

```bash
./ircserv
./ircserv 6667
./ircserv 6667 password extra
```

### Puertos inválidos

```bash
./ircserv abc password
./ircserv 12abc password
./ircserv -1 password
./ircserv 0 password
./ircserv 65536 password
```

### Contraseña vacía

```bash
./ircserv 6667 ""
```

### Finalización limpia

```bash
./ircserv 6667 password
```

Después pulsa:

```text
Ctrl+C
```

### Probar `SIGTERM`

En una terminal:

```bash
./ircserv 6667 password
```

En otra:

```bash
pgrep ircserv
kill -TERM <process_id>
```

### Memoria y descriptores

```bash
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-fds=yes \
    ./ircserv 6667 password
```

## Fase 1 terminada

Puedes considerar completada esta fase cuando:

- Se reciben exactamente el puerto y la contraseña.
- El puerto está correctamente validado.
- La contraseña no está vacía.
- `Server` mantiene un estado válido.
- Los descriptores comienzan en `-1`.
- `Server` no puede copiarse.
- `SIGINT` y `SIGTERM` solicitan el cierre.
- `SIGPIPE` está ignorada.
- El manejador solamente modifica una bandera.
- `run()` no consume innecesariamente la CPU.
- El destructor limpia los recursos.
- Ningún descriptor se cierra dos veces.
- No existen fugas propias del programa.
- Compila correctamente en C++98.

La fase siguiente será crear el socket de escucha:

```text
socket()
    → setsockopt()
    → modo no bloqueante
    → bind()
    → listen()
    → añadirlo a poll()
```