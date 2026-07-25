# Fase 2 — Socket de escucha

## Objetivo

En esta fase se debe crear y configurar únicamente el **socket de escucha del servidor IRC**.

Al ejecutar:

```bash
./ircserv 6667 password
```

el servidor debe quedar escuchando conexiones TCP en el puerto `6667`.

Desde otra terminal:

```bash
nc 127.0.0.1 6667
```

la conexión TCP debe poder establecerse, aunque el servidor todavía no interprete comandos ni procese mensajes.

---

## Alcance de esta fase

Se debe implementar:

1. Creación del socket con `socket()`.
2. Configuración de `SO_REUSEADDR` con `setsockopt()`.
3. Configuración del socket como no bloqueante con `fcntl()`.
4. Preparación de la dirección del servidor.
5. Asociación del socket al puerto con `bind()`.
6. Activación del modo escucha con `listen()`.
7. Control de errores y cierre correcto del descriptor.

Todavía **no** se debe implementar:

* `accept()`.
* Gestión de clientes.
* `poll()`.
* Recepción de datos con `recv()`.
* Envío de datos con `send()`.
* Parser de mensajes IRC.
* Interpretación de comandos.
* Registro de usuarios.
* Canales.

---

## 1. Crear el socket del servidor

Se debe crear un socket TCP sobre IPv4:

```cpp
socket(AF_INET, SOCK_STREAM, 0);
```

Significado de los argumentos:

* `AF_INET`: utiliza direcciones IPv4.
* `SOCK_STREAM`: crea un socket orientado a conexión, utilizado por TCP.
* `0`: permite que el sistema seleccione automáticamente el protocolo correspondiente, en este caso TCP.

El valor devuelto es un **file descriptor** que identifica el socket.

Si `socket()` devuelve `-1`, significa que se ha producido un error y el servidor no puede continuar.

El descriptor debe guardarse como atributo de la clase `Server`, por ejemplo:

```cpp
int _serverSocketFileDescriptor;
```

---

## 2. Configurar `SO_REUSEADDR`

Después de crear el socket, se debe activar la opción `SO_REUSEADDR` mediante `setsockopt()`:

```cpp
int optionValue = 1;

setsockopt(
    _serverSocketFileDescriptor,
    SOL_SOCKET,
    SO_REUSEADDR,
    &optionValue,
    sizeof(optionValue)
);
```

Esta opción permite reutilizar la dirección y el puerto poco después de haber detenido el servidor.

Sin esta configuración, al reiniciar rápidamente el programa podría aparecer un error como:

```text
Address already in use
```

Si `setsockopt()` devuelve `-1`, se debe:

1. Guardar o consultar el error mediante `errno`.
2. Cerrar el socket creado.
3. Detener la inicialización del servidor.

---

## 3. Hacer el socket no bloqueante

El socket de escucha debe configurarse como no bloqueante mediante `fcntl()`.

Primero se obtienen sus flags actuales:

```cpp
int currentFlags = fcntl(
    _serverSocketFileDescriptor,
    F_GETFL,
    0
);
```

Después se añaden los flags de modo no bloqueante:

```cpp
fcntl(
    _serverSocketFileDescriptor,
    F_SETFL,
    currentFlags | O_NONBLOCK
);
```

Es importante conservar los flags anteriores utilizando:

```cpp
currentFlags | O_NONBLOCK
```

No se debe reemplazar directamente toda la configuración del descriptor.

Aunque en esta fase todavía no se utilice `accept()`, configurar el socket como no bloqueante prepara el servidor para la futura gestión de múltiples clientes mediante `poll()`.

Se deben comprobar por separado los valores devueltos por ambas llamadas a `fcntl()`. Un resultado de `-1` indica un error.

---

## 4. Preparar la dirección del servidor

Para indicar en qué dirección y puerto debe escuchar el socket, se utiliza una estructura `sockaddr_in`:

```cpp
struct sockaddr_in serverAddress;
```

Antes de rellenarla, se debe inicializar toda su memoria a cero:

```cpp
std::memset(
    &serverAddress,
    0,
    sizeof(serverAddress)
);
```

Después se configuran sus campos:

```cpp
serverAddress.sin_family = AF_INET;
serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
serverAddress.sin_port = htons(port);
```

Significado:

* `sin_family = AF_INET`: la dirección utiliza IPv4.
* `INADDR_ANY`: el servidor acepta conexiones dirigidas a cualquiera de las interfaces de red de la máquina, incluida `127.0.0.1`.
* `htons(port)`: convierte el puerto al orden de bytes utilizado por la red.
* `htonl(INADDR_ANY)`: convierte la dirección al orden de bytes de red.

El puerto ya debe haber sido validado durante la fase 1.

---

## 5. Asociar el socket con `bind()`

`bind()` asocia el socket con la dirección y el puerto configurados:

```cpp
bind(
    _serverSocketFileDescriptor,
    reinterpret_cast<struct sockaddr *>(&serverAddress),
    sizeof(serverAddress)
);
```

Después de esta llamada, el socket queda asociado al puerto indicado al ejecutar el servidor.

Por ejemplo:

```bash
./ircserv 6667 password
```

asociará el socket al puerto `6667`.

Si `bind()` devuelve `-1`, las causas más habituales son:

* El puerto ya está siendo utilizado.
* El puerto no es válido.
* El proceso no tiene permisos para utilizar ese puerto.
* La dirección está mal configurada.

En caso de error, se debe cerrar el socket antes de terminar la inicialización.

---

## 6. Activar el modo escucha con `listen()`

Después de ejecutar correctamente `bind()`, se debe poner el socket en modo escucha:

```cpp
listen(
    _serverSocketFileDescriptor,
    SOMAXCONN
);
```

`SOMAXCONN` indica que se utilizará el límite máximo permitido por el sistema para la cola de conexiones pendientes.

A partir de este momento, el sistema operativo puede recibir solicitudes de conexión TCP dirigidas al puerto.

Si `listen()` devuelve `-1`, se debe cerrar el socket y detener la inicialización.

---

## 7. Mantener el servidor en ejecución

Después de ejecutar `listen()`, el proceso debe permanecer activo.

Si `main()` termina inmediatamente, el destructor de `Server` cerrará el socket y `nc` no podrá conectarse.

En esta fase todavía no hace falta aceptar ni procesar clientes, pero el servidor debe permanecer ejecutándose hasta recibir la señal de finalización gestionada en la fase 1.

---

## 8. Gestionar correctamente los errores

Todas las llamadas al sistema deben comprobar su valor de retorno:

| Función        | Error |
| -------------- | ----: |
| `socket()`     |  `-1` |
| `setsockopt()` |  `-1` |
| `fcntl()`      |  `-1` |
| `bind()`       |  `-1` |
| `listen()`     |  `-1` |

Si se produce un error después de haber creado el socket, se debe cerrar su descriptor:

```cpp
close(_serverSocketFileDescriptor);
```

También conviene establecerlo a un valor inválido para evitar cerrarlo dos veces:

```cpp
_serverSocketFileDescriptor = -1;
```

El mensaje de error puede construirse utilizando:

```cpp
std::strerror(errno)
```

La inicialización no debe continuar después de que falle una de estas operaciones.

---

## Orden de implementación

El orden correcto de las operaciones es:

```text
socket()
    ↓
setsockopt(SO_REUSEADDR)
    ↓
fcntl(F_GETFL)
    ↓
fcntl(F_SETFL, O_NONBLOCK)
    ↓
preparar sockaddr_in
    ↓
bind()
    ↓
listen()
```

No se debe llamar a `bind()` antes de crear y configurar el socket, ni a `listen()` antes de que `bind()` haya terminado correctamente.

---

## Comprobación manual

### 1. Compilar el servidor

```bash
make
```

### 2. Ejecutarlo

```bash
./ircserv 6667 password
```

El programa debe permanecer activo y no debe devolver inmediatamente el prompt de la terminal.

### 3. Comprobar el puerto

En otra terminal:

```bash
ss -ltnp | grep ':6667'
```

Debe aparecer el puerto en estado:

```text
LISTEN
```

### 4. Probar la conexión TCP

```bash
nc 127.0.0.1 6667
```

`nc` debe poder establecer la conexión y quedarse esperando.

En esta fase, escribir texto en `nc` no tiene que producir ninguna respuesta, porque todavía no se reciben datos ni se interpretan comandos.

### 5. Comprobar un puerto ocupado

Con una instancia del servidor ya ejecutándose, se puede intentar iniciar otra:

```bash
./ircserv 6667 password
```

La segunda instancia debe detectar el error de `bind()` y terminar de forma controlada, mostrando un mensaje similar a:

```text
bind: Address already in use
```

---

## Criterios para considerar terminada la fase

La fase 2 estará completa cuando:

* El socket TCP se cree correctamente.
* `SO_REUSEADDR` esté activado.
* El socket esté configurado como no bloqueante.
* La estructura `sockaddr_in` esté correctamente inicializada.
* El socket se asocie al puerto recibido por argumentos.
* El socket entre en estado `LISTEN`.
* `nc 127.0.0.1 6667` pueda establecer una conexión TCP.
* Todos los errores de las llamadas al sistema sean comprobados.
* El socket se cierre correctamente al detener el servidor.
* No se haya implementado todavía la gestión de clientes ni de comandos IRC.
