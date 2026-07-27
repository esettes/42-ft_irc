# Fase 4 — Modelo básico de cliente

## Objetivo

Crear una clase `Client` que represente el estado de cada usuario conectado al servidor.

Cada vez que el servidor acepte una nueva conexión TCP mediante `accept()`, deberá crear un objeto `Client` asociado al **file descriptor del socket** de esa conexión.

En esta fase todavía no es necesario implementar por completo los comandos IRC. El objetivo es preparar el modelo de datos que permitirá gestionar posteriormente:

* La recepción y el envío de información.
* La autenticación mediante contraseña.
* El registro IRC mediante `NICK` y `USER`.
* Los canales a los que pertenece el cliente.
* La desconexión y eliminación segura del cliente.

---

## 1. Crear la clase `Client`

La clase debe almacenar como mínimo los siguientes datos:

```text
Client
├── socket file descriptor
├── input buffer
├── output buffer
├── nickname
├── username
├── real name
├── password accepted
├── nickname received
├── username received
├── registered
└── channels joined
```

Una posible declaración inicial sería:

```cpp
class Client
{
    private:
        int _socketFileDescriptor;

        std::string _inputBuffer;
        std::string _outputBuffer;

        std::string _nickname;
        std::string _username;
        std::string _realName;

        bool _passwordAccepted;
        bool _nicknameReceived;
        bool _usernameReceived;
        bool _registered;

        std::set<std::string> _joinedChannels;

    public:
        explicit Client(int socketFileDescriptor);
        ~Client();
};
```

El contenedor utilizado para los canales puede cambiar más adelante cuando se implemente la clase `Channel`. Por ahora, un `std::set<std::string>` permite almacenar los nombres sin duplicados.

---

## 2. Almacenar el file descriptor

Cada cliente debe conocer el file descriptor del socket utilizado para comunicarse con él:

```cpp
int _socketFileDescriptor;
```

Este descriptor permite identificar la conexión en:

* El contenedor de clientes del servidor.
* La estructura utilizada por `poll()`.
* Las llamadas a `recv()`.
* Las llamadas a `send()`.
* La gestión de desconexiones.

Debe proporcionarse un método para consultarlo:

```cpp
int getSocketFileDescriptor() const;
```

---

## 3. Implementar el buffer de entrada

El buffer de entrada almacena los bytes recibidos mediante `recv()`:

```cpp
std::string _inputBuffer;
```

TCP no garantiza que un comando IRC llegue completo en una sola llamada. También puede entregar varios comandos juntos.

Por ejemplo, un cliente puede enviar:

```text
NICK roxana\r\nUSER roxana 0 * :Roxana\r\n
```

El servidor podría recibir esos datos:

* En una sola llamada a `recv()`.
* Divididos en varias llamadas.
* Junto con otros comandos posteriores.

Por eso, los datos deben añadirse al buffer:

```cpp
void appendToInputBuffer(const std::string &receivedData);
```

También será necesario extraer únicamente las líneas completas terminadas en `\r\n`:

```cpp
bool extractNextLine(std::string &line);
```

Si todavía no existe una línea completa, los datos deben permanecer guardados en el buffer hasta la siguiente lectura.

---

## 4. Implementar el buffer de salida

El buffer de salida almacena las respuestas pendientes de enviar:

```cpp
std::string _outputBuffer;
```

No se debe asumir que `send()` enviará todos los bytes solicitados, especialmente porque los sockets son no bloqueantes.

Métodos recomendados:

```cpp
void appendToOutputBuffer(const std::string &message);
const std::string &getOutputBuffer() const;
void removeSentOutput(std::size_t sentByteCount);
bool hasPendingOutput() const;
```

Cuando `send()` consiga enviar una parte del buffer, solamente deben eliminarse los bytes realmente enviados.

Si todavía quedan datos pendientes, el cliente deberá seguir vigilándose con el evento `POLLOUT`.

---

## 5. Almacenar la identidad IRC

El cliente debe guardar los datos recibidos durante el registro:

```cpp
std::string _nickname;
std::string _username;
std::string _realName;
```

### Nickname

Se recibe mediante:

```text
NICK <nickname>
```

El nickname debe ser válido y no puede estar siendo utilizado por otro cliente.

Métodos recomendados:

```cpp
const std::string &getNickname() const;
void setNickname(const std::string &nickname);
```

### Username y real name

Se reciben mediante:

```text
USER <username> 0 * :<real name>
```

Métodos recomendados:

```cpp
const std::string &getUsername() const;
void setUsername(const std::string &username);

const std::string &getRealName() const;
void setRealName(const std::string &realName);
```

---

## 6. Representar por separado los estados del registro

El registro IRC depende de varias condiciones. Por eso no conviene representarlo mediante un único estado como:

```cpp
bool _authenticated;
```

En su lugar, deben almacenarse por separado:

```cpp
bool _passwordAccepted;
bool _nicknameReceived;
bool _usernameReceived;
bool _registered;
```

Cada estado tiene una responsabilidad concreta:

| Estado              | Significado                                                      |
| ------------------- | ---------------------------------------------------------------- |
| `_passwordAccepted` | El cliente ha enviado la contraseña correcta mediante `PASS`.    |
| `_nicknameReceived` | El servidor ha aceptado un nickname válido y disponible.         |
| `_usernameReceived` | El servidor ha recibido correctamente el comando `USER`.         |
| `_registered`       | El cliente ha completado todas las condiciones del registro IRC. |

Métodos recomendados:

```cpp
bool isPasswordAccepted() const;
void setPasswordAccepted(bool accepted);

bool hasReceivedNickname() const;
void setNicknameReceived(bool received);

bool hasReceivedUsername() const;
void setUsernameReceived(bool received);

bool isRegistered() const;
```

---

## 7. Comprobar cuándo se completa el registro

Un cliente estará registrado cuando se cumplan simultáneamente estas condiciones:

```text
PASS correcto
    +
NICK válido y disponible
    +
USER recibido
```

La condición puede representarse así:

```cpp
_passwordAccepted
    && _nicknameReceived
    && _usernameReceived
```

La comprobación debe realizarse después de procesar cada comando relacionado con el registro:

* `PASS`
* `NICK`
* `USER`

Esto es necesario porque `NICK` y `USER` pueden recibirse en distinto orden.

Por ejemplo, ambos órdenes son posibles:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana
```

```text
PASS secret
USER roxana 0 * :Roxana
NICK roxana
```

Puede añadirse un método como:

```cpp
bool Client::canBeRegistered() const
{
    return _passwordAccepted
        && _nicknameReceived
        && _usernameReceived;
}
```

El servidor podrá utilizarlo para completar el registro:

```cpp
if (!client.isRegistered() && client.canBeRegistered())
{
    client.markAsRegistered();
}
```

Es importante comprobar primero `isRegistered()` para no completar el registro ni enviar el mensaje de bienvenida varias veces.

Método recomendado:

```cpp
void markAsRegistered();
```

---

## 8. Inicializar correctamente el cliente

El constructor debe recibir el file descriptor de la conexión e inicializar todos los estados:

```cpp
Client::Client(int socketFileDescriptor)
    : _socketFileDescriptor(socketFileDescriptor),
      _inputBuffer(),
      _outputBuffer(),
      _nickname(),
      _username(),
      _realName(),
      _passwordAccepted(false),
      _nicknameReceived(false),
      _usernameReceived(false),
      _registered(false),
      _joinedChannels()
{
}
```

Cuando se crea un cliente:

* Tiene una conexión TCP activa.
* Todavía no ha aceptado la contraseña.
* Todavía no ha enviado un nickname válido.
* Todavía no ha enviado `USER`.
* Todavía no está registrado.
* Todavía no pertenece a ningún canal.

---

## 9. Almacenar los canales del cliente

El cliente debe saber a qué canales pertenece:

```cpp
std::set<std::string> _joinedChannels;
```

Métodos recomendados:

```cpp
void joinChannel(const std::string &channelName);
void leaveChannel(const std::string &channelName);
bool isInChannel(const std::string &channelName) const;
const std::set<std::string> &getJoinedChannels() const;
```

En esta fase basta con preparar la estructura. La lógica completa de `JOIN`, `PART` y la clase `Channel` se implementará más adelante.

---

## 10. Crear un cliente al aceptar una conexión

Después de que `accept()` devuelva un nuevo file descriptor, el servidor debe:

1. Configurar el nuevo socket como no bloqueante.
2. Crear el objeto `Client`.
3. Guardarlo en el contenedor de clientes.
4. Añadir su file descriptor a los elementos vigilados por `poll()`.

Una estructura habitual en `Server` es:

```cpp
std::map<int, Client *> _clients;
```

El file descriptor funciona como clave:

```cpp
Client *newClient = new Client(clientSocketFileDescriptor);
_clients[clientSocketFileDescriptor] = newClient;
```

Si el proyecto permite que `Client` sea copiable y su destructor sea accesible, también puede estudiarse almacenar objetos directamente:

```cpp
std::map<int, Client> _clients;
```

La decisión debe mantener una propiedad clara: el servidor es responsable de todos los clientes que contiene y debe liberarlos cuando se desconecten.

---

## 11. Eliminar correctamente un cliente

Cuando un cliente se desconecte o se produzca un error fatal, el servidor deberá:

1. Retirarlo de los elementos vigilados por `poll()`.
2. Eliminarlo de todos los canales.
3. Cerrar su socket.
4. Eliminar su objeto `Client`.
5. Borrarlo del contenedor de clientes.

Si se utilizan punteros:

```cpp
std::map<int, Client *>::iterator clientIterator =
    _clients.find(socketFileDescriptor);

if (clientIterator != _clients.end())
{
    delete clientIterator->second;
    _clients.erase(clientIterator);
}
```

Debe decidirse claramente quién cierra el socket:

* El destructor de `Client`.
* O el método de desconexión de `Server`.

No deben hacerlo ambos, porque se produciría un cierre duplicado del mismo file descriptor.

---

## 12. Responsabilidades de cada clase

### `Client`

Debe encargarse de almacenar y modificar:

* El file descriptor.
* Los buffers de entrada y salida.
* La identidad IRC.
* El progreso del registro.
* Los canales a los que pertenece.

### `Server`

Debe encargarse de:

* Aceptar conexiones.
* Crear y almacenar clientes.
* Comprobar que los nicknames no estén ocupados.
* Procesar los comandos recibidos.
* Completar el registro.
* Añadir o retirar clientes de los canales.
* Desconectar y eliminar clientes.

La clase `Client` representa el estado de una conexión, pero no debe conocer ni controlar todo el servidor.

---

## Resultado esperado

Al finalizar esta fase:

* Cada conexión aceptada tiene su propio objeto `Client`.
* Cada cliente está asociado a un file descriptor.
* Los datos recibidos pueden acumularse en un buffer de entrada.
* Las respuestas pendientes pueden conservarse en un buffer de salida.
* El cliente almacena nickname, username y real name.
* El progreso del registro se representa mediante estados separados.
* El registro puede completarse independientemente del orden de `NICK` y `USER`.
* Existe una estructura para recordar los canales del cliente.
* El servidor puede eliminar correctamente un cliente desconectado.

En esta fase todavía no es necesario implementar completamente `PASS`, `NICK`, `USER`, `JOIN` o `PART`. Debe quedar preparado el modelo sobre el que funcionarán esos comandos.
