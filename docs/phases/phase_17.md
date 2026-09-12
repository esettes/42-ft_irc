# Phase 17 — `MODE` command

## Goal

Implement querying and changing channel modes.

It is recommended to leave `MODE` for the end of the operator commands because it is the handler with the most combinations, validations and parameter consumption.

The project requires implementing the following modes:

| Mode | Function |
|---|---|
| `+i` / `-i` | Enable or disable the invite-only channel |
| `+t` / `-t` | Restrict or allow topic changes |
| `+k` / `-k` | Set or remove the channel password |
| `+o` / `-o` | Grant or remove operator privileges |
| `+l` / `-l` | Set or remove the user limit |

---

## 1. Query the current modes

Format:

```text
MODE #general
```

The server must check:

- That the channel exists.
- That the user is registered.
- Being an operator is not required to query the modes.

The reply can use:

```text
324 RPL_CHANNELMODEIS
```

Example:

```text
:server.name 324 roxana #general +itkl secret 10
```

The reply must include:

- The active modes.
- The password if `+k` is active.
- The user limit if `+l` is active.

Mode `+o` is not normally included in this list because it represents a privilege associated with specific users. Operators can be identified through the `@` prefix in the `NAMES` reply.

It is useful to always use the same order when building the mode list, for example:

```text
itkl
```

---

## 2. Modes without arguments

Modes `i` and `t` do not consume extra parameters.

### Invite mode

```text
MODE #general +i
MODE #general -i
```

Behaviour:

- `+i` enables invite-only access.
- `-i` disables this restriction.
- When it is active, `JOIN` must reject users who are not invited.
- A valid invitation lets them bypass this restriction.

### Topic restriction

```text
MODE #general +t
MODE #general -t
```

Behaviour:

- `+t` lets only operators change the topic.
- `-t` lets any channel member change the topic.
- This state must be queried from the `TOPIC` handler.

---

## 3. Modes with arguments

### Channel password

```text
MODE #general +k secret
MODE #general -k
```

Behaviour:

- `+k` consumes a password as an argument.
- It stores the channel password.
- It enables the keyed-channel mode.
- `JOIN` must check that the user provides the correct password.
- `-k` removes the stored password.
- `-k` disables the keyed-channel mode.
- In this design, `-k` does not need to consume any argument.

The password provided to `+k` must not be empty.

---

### User limit

```text
MODE #general +l 10
MODE #general -l
```

Behaviour:

- `+l` consumes a number as an argument.
- The number must be a strictly positive integer.
- It stores the maximum user limit.
- `JOIN` must reject new entries when the channel has reached the limit.
- `-l` removes the limit.
- `-l` does not consume any argument.

Conversion of the limit must validate:

- That every character is numeric.
- That the value is greater than zero.
- That the value does not cause overflow.
- That the value can be stored in the type used by the channel.

---

### Channel operators

```text
MODE #general +o roxana
MODE #general -o roxana
```

Behaviour:

- `+o` grants operator privileges to the indicated user.
- `-o` removes their operator privileges.
- Both modes consume a nickname as an argument.

Before changing the privileges it must be checked:

- That the target user exists.
- That the target user belongs to the channel.
- That the sender belongs to the channel.
- That the sender is a channel operator.

The operator collection must prevent duplicates.

It is not necessary to kick the user when their operator privilege is removed. They simply stop being part of the operator collection.

---

## 4. General validations

To change a channel’s modes it must be checked, in this order:

1. The user is registered.
2. The channel name has been provided.
3. The channel exists.
4. A mode string has been provided.
5. The sender belongs to the channel.
6. The sender is an operator.
7. Every mode is known.
8. Each mode has the required arguments.
9. The arguments are valid for the corresponding mode.

Querying the modes through:

```text
MODE #general
```

should not require operator privileges.

Changing the modes must require the sender to belong to the channel and be an operator.

---

## 5. Combining modes

The command must accept several modes inside the same string:

```text
MODE #general +it
MODE #general -it
MODE #general +kl secret 10
MODE #general +o-l roxana
MODE #general +kol secret roxana 10
```

The `+` and `-` symbols change the operation applied to the letters that appear afterwards.

Example:

```text
MODE #general +o-l roxana
```

Must be interpreted as:

1. `+o roxana`: grant operator privileges to `roxana`.
2. `-l`: remove the user limit.

Example:

```text
MODE #general +kol secret roxana 10
```

Must be interpreted as:

1. `+k secret`
2. `+o roxana`
3. `+l 10`

Arguments are consumed in the same order in which the mode letters appear.

---

## 6. Parameter consumption

Each mode has different rules:

| Mode | With `+` | With `-` |
|---|---:|---:|
| `i` | No argument | No argument |
| `t` | No argument | No argument |
| `k` | Requires password | No argument |
| `l` | Requires limit | No argument |
| `o` | Requires nickname | Requires nickname |

Example:

```text
MODE #general +itkol secret roxana 10
```

Parameter consumption:

| Operation | Consumed parameter |
|---|---|
| `+i` | None |
| `+t` | None |
| `+k` | `secret` |
| `+o` | `roxana` |
| `+l` | `10` |

---

## 7. Dedicated mode parser

The general IRC parser should produce something equivalent to:

```text
command: MODE
parameters:
  - "#general"
  - "+kol"
  - "secret"
  - "roxana"
  - "10"
```

Then the `MODE` handler must use a dedicated parser to interpret the `+kol` string.

Recommended flow:

```text
mode string
    ↓
walk each character
    ↓
if + or - appears → update the current operation
    ↓
identify the mode letter
    ↓
determine whether it needs an argument
    ↓
consume the next argument when appropriate
    ↓
validate the operation
    ↓
modify the channel state
```

The parser must keep:

- The current operation: add or remove.
- The index of the parameter that must be consumed.
- The list of valid operations.
- The modes applied correctly.
- The arguments associated with those modes.

It is useful to separate:

1. Interpretation of the mode string.
2. Validation of each operation.
3. Modification of the channel.
4. Construction of the message that will be notified.

---

## 8. Recommended internal representation

A mode operation can be represented conceptually as:

```text
ModeOperation
├── action: add/remove
├── mode: i/t/k/o/l
├── requiresArgument
└── argument
```

Example for:

```text
MODE #general +o-l roxana
```

Result:

```text
ModeOperation
├── action: add
├── mode: o
└── argument: roxana

ModeOperation
├── action: remove
├── mode: l
└── argument: none
```

First generating a collection of operations makes it easier to:

- Detect missing parameters.
- Validate combinations.
- Avoid partially modifying the channel because of a parsing error.
- Build the final notification correctly.
- Test the parser independently of the server.

---

## 9. Updating the `Channel` model

The `Channel` class must store, at a minimum:

```cpp
bool inviteOnly;
bool topicRestricted;
bool keyEnabled;
bool limitEnabled;
std::string channelKey;
std::size_t userLimit;
```

It must also have an operator collection:

```text
operators
```

It is useful to offer clear functions to query and change each state:

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

Logic related to channel states should stay inside `Channel`, while the `MODE` handler takes care of:

- Validating the command.
- Interpreting the modes.
- Looking up users and channels.
- Applying the operations.
- Sending errors.
- Notifying the changes.

---

## 10. Notifying the changes

When a change is performed correctly, every channel member must be notified, including the sender.

Example:

```text
:roxana!roxana@localhost MODE #general +it
```

Example with arguments:

```text
:roxana!roxana@localhost MODE #general +kl secret 10
```

Example of an operator change:

```text
:roxana!roxana@localhost MODE #general +o otherUser
```

The notification must include:

- The full prefix of the user who ran the command.
- The channel name.
- The applied modes.
- The corresponding arguments.
- The `\r\n` terminator.

A mode that has been rejected must not be notified as applied.

---

## 11. Relevant replies and errors

| Code | Name | Situation |
|---:|---|---|
| `324` | `RPL_CHANNELMODEIS` | Correct query of the current modes |
| `401` | `ERR_NOSUCHNICK` | The user indicated for `+o` or `-o` does not exist |
| `403` | `ERR_NOSUCHCHANNEL` | The channel does not exist |
| `441` | `ERR_USERNOTINCHANNEL` | The target of `+o` or `-o` does not belong to the channel |
| `442` | `ERR_NOTONCHANNEL` | The sender does not belong to the channel |
| `461` | `ERR_NEEDMOREPARAMS` | Mandatory parameters are missing |
| `472` | `ERR_UNKNOWNMODE` | An unknown mode letter has been received |
| `482` | `ERR_CHANOPRIVSNEEDED` | The sender is not a channel operator |

Unknown mode example:

```text
MODE #general +x
```

Possible reply:

```text
:server.name 472 roxana x :is unknown mode char to me
```

---

## 12. Integration with other commands

The `MODE` implementation must change the behaviour of other handlers.

### `JOIN`

Must query:

- `+i`: check whether the user is invited.
- `+k`: check the password.
- `+l`: check the user limit.

### `TOPIC`

Must query:

- `+t`: only an operator can change the topic.
- `-t`: any member can change it.

### `INVITE`

Must check whether the sender has the required privileges when appropriate.

### `KICK`

Must check that the sender is an operator.

### `MODE`

Mode `+o` must modify the same operator collection used by `KICK`, `INVITE`, `TOPIC` and `MODE` itself.

---

## 13. Recommended implementation order

1. Implement the query:

   ```text
   MODE #general
   ```

2. Implement simple modes:

   ```text
   MODE #general +i
   MODE #general -i
   MODE #general +t
   MODE #general -t
   ```

3. Implement the password:

   ```text
   MODE #general +k secret
   MODE #general -k
   ```

4. Implement the limit:

   ```text
   MODE #general +l 10
   MODE #general -l
   ```

5. Implement operators:

   ```text
   MODE #general +o roxana
   MODE #general -o roxana
   ```

6. Implement combinations with a single sign:

   ```text
   MODE #general +it
   MODE #general +kol secret roxana 10
   ```

7. Implement sign changes inside the same string:

   ```text
   MODE #general +o-l roxana
   MODE #general +it-k secret
   ```

8. Integrate the states with `JOIN`, `TOPIC`, `INVITE` and `KICK`.

9. Add error and combination tests.

---

## 14. Minimum test cases

### Query

```text
MODE #general
```

Must return the current modes.

### Enable and disable

```text
MODE #general +i
MODE #general -i
MODE #general +t
MODE #general -t
```

### Password

```text
MODE #general +k secret
JOIN #general incorrect
JOIN #general secret
MODE #general -k
```

### Limit

```text
MODE #general +l 2
MODE #general -l
```

Invalid limits must also be tested:

```text
MODE #general +l 0
MODE #general +l -5
MODE #general +l text
```

### Operators

```text
MODE #general +o roxana
MODE #general -o roxana
```

### Combinations

```text
MODE #general +it
MODE #general -it
MODE #general +kl secret 10
MODE #general +o-l roxana
MODE #general +kol secret roxana 10
```

### Errors

Try:

- Nonexistent channel.
- Nonexistent target user.
- Target user outside the channel.
- Sender outside the channel.
- Sender without operator privileges.
- Unknown mode.
- Missing password.
- Missing limit.
- Invalid limit.
- Missing nickname for `+o`.
- Missing nickname for `-o`.
- Combinations with fewer arguments than needed.

---

## Expected result of the phase

At the end of this phase, the server must be able to:

- Query a channel’s active modes.
- Enable and disable `i`, `t`, `k`, `o` and `l`.
- Consume each mode’s parameters correctly.
- Interpret several modes inside the same command.
- Switch between `+` and `-` inside the same string.
- Validate member and operator permissions.
- Notify changes to every channel member.
- Apply the modes to the behaviour of `JOIN`, `TOPIC`, `INVITE` and `KICK`.
- Reply consistently to invalid modes, parameters or users.
