# Phase 6 — IRC parser

## Goal

Implement a parser that transforms a complete IRC line into a data structure that the server can use later.

The parser receives only complete lines that have already been extracted from the TCP buffer during the previous phase.

For example:

```text
PRIVMSG #general :Hello everyone
```

Must be converted into:

```text
Command
├── name: "PRIVMSG"
└── parameters:
    ├── "#general"
    └── "Hello everyone"
```

The parser still does not execute the command. Its only responsibility is to interpret the text and separate its components.

---

## Command structure

A simple structure can store:

- The command name.
- The ordered list of parameters.

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

The final parameter, also called `trailing`, can be stored as the last element of the vector.

For this line:

```text
PRIVMSG #general :Hello everyone
```

The result would be:

```text
_name = "PRIVMSG"

_parameters[0] = "#general"
_parameters[1] = "Hello everyone"
```

It is not mandatory to create a separate variable for the `trailing`, as long as it is correctly kept as a single parameter.

---

## Fundamental parser rules

An IRC line generally has this structure:

```text
COMMAND param1 param2 :trailing parameter
```

The parser must recognize:

1. The command name.
2. The normal parameters separated by spaces.
3. An optional final parameter that starts with `:`.
4. The final parameter may contain spaces.
5. The `:` character must not form part of the stored value.

---

## Command name

The first element of the line is the command name.

Example:

```text
NICK roxana
```

Result:

```text
name = "NICK"
parameters[0] = "roxana"
```

It is recommended to convert the command name to uppercase so the dispatcher can treat entries such as these in the same way:

```text
nick roxana
Nick roxana
NICK roxana
```

All of them should produce:

```text
name = "NICK"
```

---

## Normal parameters

Normal parameters are separated by one or more spaces.

Example:

```text
MODE #general +o roxana
```

Result:

```text
name = "MODE"

parameters[0] = "#general"
parameters[1] = "+o"
parameters[2] = "roxana"
```

The spaces used as separators must not be stored.

---

## Final parameter or `trailing`

When a parameter starts with `:`, the entire remaining text belongs to the same parameter, even if it contains spaces.

Example:

```text
USER roxana 0 * :Roxana Example
```

Result:

```text
name = "USER"

parameters[0] = "roxana"
parameters[1] = "0"
parameters[2] = "*"
parameters[3] = "Roxana Example"
```

The parser must remove only the first `:` that marks the start of the final parameter.

Another example:

```text
PRIVMSG #general :Hello everyone
```

Result:

```text
name = "PRIVMSG"

parameters[0] = "#general"
parameters[1] = "Hello everyone"
```

It must not produce:

```text
parameters[1] = "Hello"
parameters[2] = "everyone"
```

---

## General operation

The parser can follow this process:

```text
Complete IRC line
        ↓
Ignore leading spaces
        ↓
Extract the command name
        ↓
Extract parameters separated by spaces
        ↓
If a parameter starting with ':' appears
        ↓
Store all remaining text as the last parameter
        ↓
Return the Command object
```

A possible interface would be:

```cpp
class MessageParser
{
public:
    static Command parse(const std::string &line);
};
```

Use:

```cpp
Command command = MessageParser::parse(
    "PRIVMSG #general :Hello everyone"
);
```

---

## Relationship with TCP reconstruction

The parser must not work directly with the data returned by `recv()`.

The correct flow is:

```text
recv()
   ↓
Data is appended to the client buffer
   ↓
Complete lines ending in "\r\n" are extracted
   ↓
Each complete line is delivered to the parser
   ↓
The parser returns a Command
   ↓
The dispatcher will execute the command in a later phase
```

For example, if TCP delivers:

```text
First recv():   "PRIV"
Second recv():  "MSG #general :Hello\r\n"
```

The parser must not receive the two parts separately.

It must receive:

```text
PRIVMSG #general :Hello
```

The `\r\n` terminator must already have been removed during line extraction.

---

## Special cases it must handle

### Empty line

```text

```

It contains no valid command.

The parser must indicate that the line cannot be interpreted, for example:

- Returning an error state.
- Throwing a controlled exception.
- Returning an empty `Command` object.

The strategy must be consistent throughout the server.

### Extra spaces

Input:

```text
   NICK    roxana
```

Result:

```text
name = "NICK"
parameters[0] = "roxana"
```

### Command without parameters

Input:

```text
QUIT
```

Result:

```text
name = "QUIT"
parameters = empty
```

### Empty `trailing`

Input:

```text
QUIT :
```

Result:

```text
name = "QUIT"
parameters[0] = ""
```

### Colons inside the `trailing`

Input:

```text
PRIVMSG #general :Current time: 14:30
```

Result:

```text
name = "PRIVMSG"
parameters[0] = "#general"
parameters[1] = "Current time: 14:30"
```

Once the start of the `trailing` has been detected, the other `:` characters are part of the text.

---

## Parser responsibilities

The parser must:

- Receive a complete IRC line.
- Extract the command name.
- Separate the normal parameters.
- Recognize the final parameter started by `:`.
- Keep the spaces of the final parameter.
- Keep the order of the parameters.
- Normalize the command name if working in uppercase is chosen.
- Detect empty or uninterpretable lines.

The parser must not:

- Check whether the client is registered.
- Validate the server password.
- Look up users or channels.
- Check whether a nickname is available.
- Execute commands such as `JOIN`, `PRIVMSG` or `QUIT`.
- Check operator permissions.
- Send replies to the client.
- Generate IRC numeric codes.
- Modify the server state.
- Read directly from the socket.
- Manage the client’s TCP buffer.

Those responsibilities belong to the dispatcher and the command handlers.

---

## Separation of responsibilities

```text
Client
└── Stores the input buffer

Server
└── Receives data and extracts complete lines

MessageParser
└── Converts each line into a Command

CommandDispatcher
└── Selects the corresponding handler

CommandHandler
└── Validates and executes the command
```

This separation avoids mixing socket reading, protocol analysis and server logic.

---

## Minimum tests

The parser should be tested at least with these inputs:

```text
PASS secret
NICK roxana
USER roxana 0 * :Roxana Example
PING :server
JOIN #general
PRIVMSG #general :Hello everyone
QUIT :Leaving the server
CAP LS 302
```

Expected results:

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
└── ["#general", "Hello everyone"]

QUIT
└── ["Leaving the server"]

CAP
└── ["LS", "302"]
```

Edge cases must also be tested:

```text
""
"   "
"QUIT"
"QUIT :"
"   NICK    roxana"
"PRIVMSG #general :Current time: 14:30"
```

---

## Expected result of the phase

At the end of this phase, the server must be able to:

1. Receive a complete IRC line from the TCP reconstruction system.
2. Deliver it to the parser.
3. Obtain a `Command` object.
4. Query the command name.
5. Query its parameters in the correct order.
6. Keep the final parameter as a single unit.
7. Leave the command ready for the dispatcher to execute it in a later phase.

In this phase it is still not necessary for the commands to produce real effects on the server.
