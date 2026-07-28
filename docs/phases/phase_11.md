# Fase 11 — Modelo de canal

## Objetivo

Implementar la clase `Channel`, responsable de representar el estado de un canal IRC.

Esta fase prepara la estructura necesaria para implementar posteriormente:

- `JOIN`
- `PART`
- `PRIVMSG`
- `TOPIC`
- `INVITE`
- `KICK`
- `MODE`
- `QUIT`

En esta fase debe definirse principalmente el modelo de datos y las operaciones básicas sobre los miembros y modos del canal.

---

## Estructura del canal

Cada canal debe almacenar:

```text
Channel
├── name
├── topic
├── members
├── operators
├── invited clients
├── invite-only mode
├── topic-restricted mode
├── key
└── user limit
```

Una posible declaración inicial sería:

```cpp
class Client;

class Channel
{
private:
    std::string name;
    std::string topic;

    std::set<Client *> members;
    std::set<Client *> operators;
    std::set<Client *> invitedClients;

    bool inviteOnly;
    bool topicRestricted;
    bool keyEnabled;
    bool limitEnabled;

    std::string channelKey;
    std::size_t userLimit;

public:
    explicit Channel(const std::string &channelName);
    ~Channel();

    // {...}
};

#endif
```

La declaración anticipada:

```cpp
class Client;
```

permite almacenar punteros a clientes sin incluir toda la definición de `Client` dentro de `Channel.hpp`.

---

## Constructor del canal

El constructor debe guardar el nombre e inicializar todos los modos desactivados:

```cpp
Channel::Channel(const std::string &channelName)
    : name(channelName),
      topic(""),
      inviteOnly(false),
      topicRestricted(false),
      keyEnabled(false),
      limitEnabled(false),
      channelKey(""),
      userLimit(0)
{
}
```

El canal comienza:

- Sin tema.
- Sin miembros.
- Sin operadores.
- Sin invitaciones.
- Sin contraseña.
- Sin límite de usuarios.
- Con todos los modos desactivados.

---

## Nombre del canal

El canal debe guardar su nombre completo:

```cpp
std::string name;
```

Ejemplos válidos habituales:

```text
#general
#programacion
#irc
```

El nombre debe utilizarse como identificador único dentro del servidor.

La validación completa del nombre puede realizarse en el comando `JOIN`, antes de crear el canal.

---

## Tema del canal

El tema se almacena en:

```cpp
std::string topic;
```

Puede estar vacío si todavía no se ha definido ninguno.

Operaciones necesarias:

```cpp
const std::string &Channel::getTopic() const;
void Channel::setTopic(const std::string &newTopic);
```

El modo `+t` determinará posteriormente si solamente los operadores pueden modificarlo.

---

## Miembros del canal

Los clientes conectados al canal pueden almacenarse mediante:

```cpp
std::set<Client *> members;
```

El uso de `std::set` evita que el mismo cliente aparezca más de una vez.

Operaciones básicas:

```cpp
void Channel::addMember(Client *client);
void Channel::removeMember(Client *client);
bool Channel::hasMember(const Client *client) const;
std::size_t Channel::getMemberCount() const;
bool Channel::isEmpty() const;
```

Ejemplo de implementación:

```cpp
void Channel::addMember(Client *client)
{
    if (client != NULL)
        members.insert(client);
}

void Channel::removeMember(Client *client)
{
    members.erase(client);
}

bool Channel::hasMember(const Client *client) const
{
    return members.find(const_cast<Client *>(client)) != members.end();
}

std::size_t Channel::getMemberCount() const
{
    return members.size();
}

bool Channel::isEmpty() const
{
    return members.empty();
}
```

El canal no debe eliminar los objetos `Client`.

Los punteros solamente representan relaciones con clientes administrados por el servidor.

---

## Operadores del canal

Los operadores pueden almacenarse en otro conjunto:

```cpp
std::set<Client *> operators;
```

Un operador siempre debe ser también miembro del canal.

Antes de añadir un operador debe comprobarse:

```cpp
void Channel::addOperator(Client *client)
{
    if (client != NULL && hasMember(client))
        operators.insert(client);
}
```

También deben existir operaciones para consultar y retirar permisos:

```cpp
bool Channel::hasOperator(const Client *client) const;
void Channel::removeOperator(Client *client);
```

Cuando un cliente abandona el canal, también debe eliminarse del conjunto de operadores.

```cpp
void Channel::removeMember(Client *client)
{
    operators.erase(client);
    invitedClients.erase(client);
    members.erase(client);
}
```

---

## Primer operador del canal

El primer usuario que entra en un canal nuevo debe convertirse automáticamente en operador.

El flujo de `JOIN` será:

```text
El cliente solicita JOIN
        ↓
El servidor busca el canal
        ↓
Si no existe, crea el canal
        ↓
Añade al cliente como miembro
        ↓
Si era el primer miembro, lo convierte en operador
```

Ejemplo:

```cpp
bool channelWasEmpty = channel.isEmpty();

channel.addMember(&client);

if (channelWasEmpty)
    channel.addOperator(&client);
```

No conviene comprobar si el canal está vacío después de añadir al cliente, porque en ese momento ya contendrá un miembro.

---

## Clientes invitados

Los clientes invitados se almacenan en:

```cpp
std::set<Client *> invitedClients;
```

Operaciones necesarias:

```cpp
void Channel::inviteClient(Client *client);
void Channel::removeInvitation(Client *client);
bool Channel::hasInvitation(const Client *client) const;
```

Esta colección será utilizada por:

```text
INVITE
JOIN
MODE +i
```

Cuando un cliente invitado entra correctamente en el canal, su invitación debe consumirse:

```cpp
channel.removeInvitation(&client);
```

Una invitación no convierte automáticamente al cliente en miembro. Solamente le permite superar la restricción del modo `+i`.

---

## Modos obligatorios

El canal debe almacenar el estado de los modos exigidos por el proyecto:

| Modo | Estado interno | Función |
|---|---|---|
| `+i` | `inviteOnly` | Solamente pueden entrar clientes invitados |
| `+t` | `topicRestricted` | Solamente los operadores pueden cambiar el tema |
| `+k` | `keyEnabled` y `channelKey` | Exige una contraseña para entrar |
| `+l` | `limitEnabled` y `userLimit` | Limita el número de miembros |
| `+o` | `operators` | Concede o retira privilegios de operador |

---

## Modo de invitación: `+i`

Estado:

```cpp
bool inviteOnly;
```

Métodos:

```cpp
bool Channel::isInviteOnly() const
{
    return inviteOnly;
}

void Channel::setInviteOnly(bool enabled)
{
    inviteOnly = enabled;
}
```

Cuando está activado, `JOIN` debe permitir la entrada únicamente si el cliente aparece en `invitedClients`.

---

## Restricción del tema: `+t`

Estado:

```cpp
bool topicRestricted;
```

Métodos:

```cpp
bool Channel::isTopicRestricted() const
{
    return topicRestricted;
}

void Channel::setTopicRestricted(bool enabled)
{
    topicRestricted = enabled;
}
```

Cuando está activado, solamente un operador puede modificar el tema del canal.

Cuando está desactivado, cualquier miembro puede modificarlo.

---

## Contraseña del canal: `+k`

Estados:

```cpp
bool keyEnabled;
std::string channelKey;
```

Métodos:

```cpp
bool Channel::isKeyEnabled() const
{
    return keyEnabled;
}

const std::string &Channel::getKey() const
{
    return channelKey;
}

void Channel::setKey(const std::string &newKey)
{
    channelKey = newKey;
    keyEnabled = true;
}

void Channel::removeKey()
{
    channelKey.clear();
    keyEnabled = false;
}
```

Al activar `+k`, debe proporcionarse una contraseña.

Al retirar `-k`, la contraseña almacenada debe eliminarse para mantener un estado coherente.

---

## Límite de usuarios: `+l`

Estados:

```cpp
bool limitEnabled;
std::size_t userLimit;
```

Métodos:

```cpp
bool Channel::isLimitEnabled() const
{
    return limitEnabled;
}

std::size_t Channel::getUserLimit() const
{
    return userLimit;
}

void Channel::setUserLimit(std::size_t newLimit)
{
    userLimit = newLimit;
    limitEnabled = true;
}

void Channel::removeUserLimit()
{
    userLimit = 0;
    limitEnabled = false;
}
```

Cuando está activo, `JOIN` debe impedir la entrada si:

```cpp
channel.getMemberCount() >= channel.getUserLimit()
```

El límite debe ser un número válido mayor que cero.

---

## Propiedad de los canales

El servidor debe ser propietario de todos los canales.

Relación recomendada:

```text
Server
└── channels
    ├── "#general" → Channel
    ├── "#programacion" → Channel
    └── "#irc" → Channel
```

Una posible estructura sería:

```cpp
std::map<std::string, Channel *> channels;
```

El servidor será responsable de:

- Crear los canales.
- Buscar los canales por nombre.
- Eliminar los canales vacíos.
- Liberar su memoria al cerrar el servidor.

Ejemplo de búsqueda:

```cpp
std::map<std::string, Channel *>::iterator channelIterator;

channelIterator = channels.find(channelName);

if (channelIterator != channels.end())
{
    Channel *channel = channelIterator->second;
}
```

Cada cliente puede almacenar referencias o punteros a los canales a los que pertenece, pero no debe poseer copias independientes de esos canales.

---

## Evitar copias independientes

No debe existir una copia distinta del mismo canal dentro de cada cliente.

Diseño incorrecto:

```text
Client A → copia de #general
Client B → copia diferente de #general
Server   → otra copia de #general
```

Esto provocaría estados inconsistentes:

- Un usuario aparecería en una copia, pero no en otra.
- El tema podría ser diferente.
- Los operadores podrían no coincidir.
- Los modos podrían tener valores distintos.

Diseño correcto:

```text
Server
└── único objeto Channel "#general"
    ├── Client A
    ├── Client B
    └── Client C
```

Todos los clientes deben referirse al mismo objeto `Channel`.

---

## Estabilidad de los punteros

Si `Channel` almacena punteros a clientes:

```cpp
std::set<Client *> members;
```

los objetos `Client` deben permanecer en direcciones de memoria estables.

Este diseño es compatible con una estructura como:

```cpp
std::map<int, Client *> clients;
```

El servidor crea los clientes dinámicamente y sus direcciones no cambian mientras permanezcan conectados.

Antes de destruir un cliente, el servidor debe eliminar su puntero de:

- Los miembros de todos sus canales.
- Los operadores de todos sus canales.
- Las listas de invitados.
- Cualquier otro índice global.

Nunca debe quedar un puntero a un cliente destruido dentro de un canal.

---

## Eliminación de canales vacíos

Cuando el último miembro abandona un canal mediante `PART`, `KICK` o `QUIT`, el servidor debe eliminar el canal.

Flujo:

```text
Se elimina el cliente del canal
        ↓
Se comprueba si el canal está vacío
        ↓
Si está vacío, el servidor elimina el canal
```

Ejemplo:

```cpp
channel->removeMember(&client);

if (channel->isEmpty())
{
    delete channel;
    channels.erase(channelIterator);
}
```

La eliminación corresponde al servidor porque es el propietario del objeto `Channel`.

---

## Responsabilidades de `Channel`

La clase `Channel` debe encargarse de:

- Almacenar el nombre y el tema.
- Mantener la lista de miembros.
- Mantener la lista de operadores.
- Mantener la lista de invitados.
- Consultar si un cliente es miembro.
- Consultar si un cliente es operador.
- Añadir y eliminar miembros.
- Añadir y eliminar operadores.
- Añadir y consumir invitaciones.
- Almacenar el estado de los modos.
- Mantener coherencia entre sus colecciones.

La clase `Channel` no debería encargarse de:

- Leer datos del socket.
- Enviar directamente mensajes con `send()`.
- Registrar clientes.
- Buscar canales globalmente.
- Crear o destruir objetos `Client`.
- Interpretar comandos IRC completos.
- Construir respuestas numéricas.

Estas responsabilidades pertenecen al servidor, al sistema de respuestas o a los manejadores de comandos.

---

## Invariantes importantes

El modelo debe mantener siempre estas reglas:

1. Un operador también debe ser miembro del canal.
2. Un cliente no debe aparecer dos veces como miembro.
3. Un cliente desconectado no debe permanecer en ninguna colección.
4. Si `keyEnabled` es `false`, `channelKey` debe estar vacío.
5. Si `limitEnabled` es `false`, `userLimit` debe valer `0`.
6. Un canal vacío debe ser eliminado por el servidor.
7. La clase `Channel` no debe destruir los objetos `Client`.
8. Todos los clientes deben compartir el mismo objeto para un mismo canal.

---

## Pruebas mínimas

Antes de continuar con los comandos de canal, conviene comprobar:

- Crear un canal con todos los modos desactivados.
- Añadir un miembro.
- Intentar añadir dos veces el mismo miembro.
- Convertir al primer miembro en operador.
- Comprobar si un cliente es miembro.
- Comprobar si un cliente es operador.
- Añadir y retirar una invitación.
- Activar y desactivar `+i`.
- Activar y desactivar `+t`.
- Establecer y eliminar una contraseña.
- Establecer y eliminar un límite.
- Eliminar un miembro y retirarlo también de operadores e invitados.
- Detectar cuándo el canal queda vacío.
- Eliminar el canal vacío desde el servidor.

---

## Resultado esperado

Al terminar esta fase debe existir una clase `Channel` capaz de representar correctamente:

```text
Nombre
Tema
Miembros
Operadores
Invitados
Modo +i
Modo +t
Modo +k
Modo +l
```

El servidor debe mantener una única instancia de cada canal y ser responsable de su creación y destrucción.

La lógica completa de `JOIN`, `PART`, `KICK`, `INVITE`, `TOPIC` y `MODE` se implementará sobre este modelo en las siguientes fases.