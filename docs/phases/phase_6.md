# Fase 6 — Parser IRC

## Objetivo

Implementar un parser que transforme una línea IRC completa en una estructura de datos que pueda utilizar posteriormente el servidor.

El parser recibe únicamente líneas completas que ya han sido extraídas del buffer TCP durante la fase anterior.

Por ejemplo:

```text
PRIVMSG #general :Hola a todo el mundo
```

Debe convertirse en:

```text
Command
├── name: "PRIVMSG"
└── parameters:
    ├── "#general"
    └── "Hola a todo el mundo"
```

El parser todavía no ejecuta el comando. Su única responsabilidad es interpretar el texto y separar sus componentes.

---

## Estructura del comando

Una estructura sencilla puede almacenar:

- El nombre del comando.
- La lista ordenada de parámetros.

```cpp
class Command
{
private:
    std::string _name;
    std::vector<std::string> _parameters;

public:
    Command();

    const std::string &getName() const;
    const std::vector<std::string> &getParameters() const;

    void setName(const std::string &name);
    void addParameter(const std::string &parameter);
};
```

El parámetro final, también llamado `trailing`, puede guardarse como el último elemento del vector.

Para esta línea:

```text
PRIVMSG #general :Hola a todo el mundo
```

El resultado sería:

```text
_name = "PRIVMSG"

_parameters[0] = "#general"
_parameters[1] = "Hola a todo el mundo"
```

No es obligatorio crear una variable separada para el `trailing`, siempre que se conserve correctamente como un único parámetro.

---

## Reglas fundamentales del parser

Una línea IRC tiene generalmente esta estructura:

```text
COMMAND param1 param2 :trailing parameter
```

El parser debe reconocer:

1. El nombre del comando.
2. Los parámetros normales separados por espacios.
3. Un parámetro final opcional que comienza con `:`.
4. El parámetro final puede contener espacios.
5. El carácter `:` no debe formar parte del valor almacenado.

---

## Nombre del comando

El primer elemento de la línea es el nombre del comando.

Ejemplo:

```text
NICK roxana
```

Resultado:

```text
name = "NICK"
parameters[0] = "roxana"
```

Es recomendable convertir el nombre del comando a mayúsculas para que el dispatcher pueda tratar de la misma manera entradas como:

```text
nick roxana
Nick roxana
NICK roxana
```

Todas ellas deberían producir:

```text
name = "NICK"
```

---

## Parámetros normales

Los parámetros normales están separados por uno o varios espacios.

Ejemplo:

```text
MODE #general +o roxana
```

Resultado:

```text
name = "MODE"

parameters[0] = "#general"
parameters[1] = "+o"
parameters[2] = "roxana"
```

Los espacios utilizados como separadores no deben guardarse.

---

## Parámetro final o `trailing`

Cuando un parámetro comienza con `:`, todo el texto restante pertenece al mismo parámetro, aunque contenga espacios.

Ejemplo:

```text
USER roxana 0 * :Roxana Example
```

Resultado:

```text
name = "USER"

parameters[0] = "roxana"
parameters[1] = "0"
parameters[2] = "*"
parameters[3] = "Roxana Example"
```

El parser debe eliminar únicamente el primer `:` que indica el comienzo del parámetro final.

Otro ejemplo:

```text
PRIVMSG #general :Hola a todo el mundo
```

Resultado:

```text
name = "PRIVMSG"

parameters[0] = "#general"
parameters[1] = "Hola a todo el mundo"
```

No debe producir:

```text
parameters[1] = "Hola"
parameters[2] = "a"
parameters[3] = "todo"
parameters[4] = "el"
parameters[5] = "mundo"
```

---

## Funcionamiento general

El parser puede seguir este proceso:

```text
Línea IRC completa
        ↓
Ignorar espacios iniciales
        ↓
Extraer el nombre del comando
        ↓
Extraer parámetros separados por espacios
        ↓
Si aparece un parámetro que empieza por ':'
        ↓
Guardar todo el texto restante como último parámetro
        ↓
Devolver el objeto Command
```

Una interfaz posible sería:

```cpp
class MessageParser
{
public:
    static Command parse(const std::string &line);
};
```

Uso:

```cpp
Command command = MessageParser::parse(
    "PRIVMSG #general :Hola a todo el mundo"
);
```

---

## Relación con la reconstrucción TCP

El parser no debe trabajar directamente con los datos devueltos por `recv()`.

El flujo correcto es:

```text
recv()
   ↓
Se añaden los datos al buffer del cliente
   ↓
Se extraen líneas completas terminadas en "\r\n"
   ↓
Cada línea completa se entrega al parser
   ↓
El parser devuelve un Command
   ↓
El dispatcher ejecutará el comando en una fase posterior
```

Por ejemplo, si TCP entrega:

```text
Primer recv():  "PRIV"
Segundo recv(): "MSG #general :Hola\r\n"
```

El parser no debe recibir las dos partes por separado.

Debe recibir:

```text
PRIVMSG #general :Hola
```

El terminador `\r\n` ya debe haber sido eliminado durante la extracción de la línea.

---

## Casos especiales que debe manejar

### Línea vacía

```text

```

No contiene ningún comando válido.

El parser debe indicar que la línea no se puede interpretar, por ejemplo:

- Devolviendo un estado de error.
- Lanzando una excepción controlada.
- Devolviendo un objeto `Command` vacío.

La estrategia debe ser consistente en todo el servidor.

### Espacios adicionales

Entrada:

```text
   NICK    roxana
```

Resultado:

```text
name = "NICK"
parameters[0] = "roxana"
```

### Comando sin parámetros

Entrada:

```text
QUIT
```

Resultado:

```text
name = "QUIT"
parameters = empty
```

### `trailing` vacío

Entrada:

```text
QUIT :
```

Resultado:

```text
name = "QUIT"
parameters[0] = ""
```

### Dos puntos dentro del `trailing`

Entrada:

```text
PRIVMSG #general :Hora actual: 14:30
```

Resultado:

```text
name = "PRIVMSG"
parameters[0] = "#general"
parameters[1] = "Hora actual: 14:30"
```

Una vez detectado el comienzo del `trailing`, los demás caracteres `:` forman parte del texto.

---

## Responsabilidades del parser

El parser sí debe:

- Recibir una línea IRC completa.
- Extraer el nombre del comando.
- Separar los parámetros normales.
- Reconocer el parámetro final iniciado por `:`.
- Conservar los espacios del parámetro final.
- Mantener el orden de los parámetros.
- Normalizar el nombre del comando si se decide trabajar en mayúsculas.
- Detectar líneas vacías o imposibles de interpretar.

El parser no debe:

- Comprobar si el cliente está registrado.
- Validar la contraseña del servidor.
- Buscar usuarios o canales.
- Comprobar si un nickname está disponible.
- Ejecutar comandos como `JOIN`, `PRIVMSG` o `QUIT`.
- Comprobar permisos de operador.
- Enviar respuestas al cliente.
- Generar códigos numéricos IRC.
- Modificar el estado del servidor.
- Leer directamente desde el socket.
- Gestionar el buffer TCP del cliente.

Estas responsabilidades pertenecen al dispatcher y a los manejadores de comandos.

---

## Separación de responsabilidades

```text
Client
└── Almacena el buffer de entrada

Server
└── Recibe datos y extrae líneas completas

MessageParser
└── Convierte cada línea en un Command

CommandDispatcher
└── Selecciona el manejador correspondiente

CommandHandler
└── Valida y ejecuta el comando
```

Esta separación evita mezclar la lectura de sockets, el análisis del protocolo y la lógica del servidor.

---

## Pruebas mínimas

El parser debería probarse al menos con estas entradas:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana Example
PING :server
JOIN #general
PRIVMSG #general :Hola a todo el mundo
QUIT :Leaving the server
CAP LS 302
```

Resultados esperados:

```text
PASS
└── ["secret"]

NICK
└── ["roxana"]

USER
└── ["roxana", "0", "*", "Roxana Example"]

PING
└── ["server"]

JOIN
└── ["#general"]

PRIVMSG
└── ["#general", "Hola a todo el mundo"]

QUIT
└── ["Leaving the server"]

CAP
└── ["LS", "302"]
```

También deben probarse casos límite:

```text
""
"   "
"QUIT"
"QUIT :"
"   NICK    roxana"
"PRIVMSG #general :Hora actual: 14:30"
```

---

## Resultado esperado de la fase

Al terminar esta fase, el servidor debe ser capaz de:

1. Recibir una línea IRC completa desde el sistema de reconstrucción TCP.
2. Entregarla al parser.
3. Obtener un objeto `Command`.
4. Consultar el nombre del comando.
5. Consultar sus parámetros en el orden correcto.
6. Conservar el parámetro final como una única unidad.
7. Dejar el comando preparado para que el dispatcher lo ejecute en una fase posterior.

En esta fase todavía no es necesario que los comandos produzcan efectos reales en el servidor.