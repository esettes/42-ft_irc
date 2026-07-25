# Fase 3 — Bucle principal con un único `poll()`

## Objetivo de la fase

Implementar el bucle principal de eventos del servidor utilizando una única llamada central a `poll()`.

Al finalizar esta fase, el servidor debe ser capaz de:

* Aceptar varios clientes simultáneamente.
* Mantener todos los sockets en modo no bloqueante.
* Detectar cuándo hay datos disponibles para leer.
* Enviar datos únicamente cuando el socket permita escribir.
* Detectar errores y desconexiones.
* Cerrar y eliminar correctamente los clientes desconectados.
* Continuar funcionando aunque uno de los clientes se desconecte o provoque un error.

En esta fase todavía no es necesario interpretar comandos IRC.

---

## 1. Mantener una colección de descriptores

El servidor debe almacenar todos los sockets que necesita vigilar en una colección:

```cpp
std::vector<pollfd> pollDescriptors;
```

Normalmente, el primer elemento será siempre el socket de escucha:

```text
pollDescriptors[0] → socket de escucha
pollDescriptors[1] → primer cliente
pollDescriptors[2] → segundo cliente
pollDescriptors[3] → tercer cliente
```

Cada estructura `pollfd` contiene:

* `fd`: descriptor del socket.
* `events`: operaciones que queremos vigilar.
* `revents`: eventos que realmente han ocurrido.

El socket de escucha debe vigilar inicialmente `POLLIN`:

```cpp
pollfd listeningDescriptor;

listeningDescriptor.fd = listeningSocketFileDescriptor;
listeningDescriptor.events = POLLIN;
listeningDescriptor.revents = 0;
```

En el socket de escucha, `POLLIN` significa que existe al menos una conexión pendiente que puede aceptarse mediante `accept()`.

---

## 2. Crear un único bucle principal

El servidor debe ejecutar un bucle mientras permanezca activo:

```text
while servidor activo
    llamar a poll()
    procesar socket de escucha
    procesar sockets de clientes
```

Debe existir una única llamada central a `poll()` encargada de vigilar:

* El socket de escucha.
* La lectura de los clientes.
* La escritura hacia los clientes.
* Los errores.
* Las desconexiones.

No se debe crear un `poll()` separado para cada operación.

---

## 3. Comprobar el resultado de `poll()`

La función `poll()` puede devolver:

* Un valor mayor que `0`: hay descriptores con eventos pendientes.
* `0`: terminó el tiempo de espera sin que ocurriera ningún evento.
* `-1`: ocurrió un error.

Si `poll()` devuelve `-1`:

* Si `errno == EINTR`, la llamada fue interrumpida por una señal y el bucle puede continuar.
* Para otros errores, debe notificarse el problema y finalizar el servidor de forma controlada.

Un timeout permite que el servidor recupere periódicamente el control, aunque no haya actividad:

```cpp
const int pollTimeoutMilliseconds = 1000;
```

También se puede utilizar `-1` para esperar indefinidamente, siempre que la finalización mediante señales esté correctamente diseñada.

---

## 4. Aceptar nuevos clientes

Cuando el socket de escucha tenga el evento `POLLIN`, se debe llamar a:

```cpp
accept();
```

Después de aceptar una conexión:

1. Obtener el descriptor del nuevo socket.
2. Configurarlo como no bloqueante mediante `fcntl()`.
3. Crear la representación interna del cliente.
4. Añadir un nuevo `pollfd` a `pollDescriptors`.
5. Configurarlo inicialmente para vigilar `POLLIN`.
6. Mostrar un mensaje indicando que el cliente se ha conectado.

El descriptor del cliente debería empezar con:

```cpp
clientDescriptor.events = POLLIN;
```

Si la configuración o el registro del cliente falla después de `accept()`, el descriptor recién creado debe cerrarse para evitar una fuga de recursos.

---

## 5. Recibir datos de los clientes

Cuando un cliente tenga el evento `POLLIN`, significa que `recv()` puede ejecutarse sin bloquear el servidor.

El resultado de `recv()` debe interpretarse de la siguiente manera:

### `recv()` devuelve un valor mayor que cero

Se han recibido datos.

Los bytes deben añadirse al búfer de entrada del cliente:

```text
client input buffer += received data
```

En esta fase todavía no es necesario interpretar esos datos como comandos IRC. El objetivo es comprobar que el servidor puede recibirlos sin bloquearse.

### `recv()` devuelve cero

El cliente ha cerrado la conexión de manera ordenada.

El servidor debe:

* Cerrar su descriptor.
* Eliminar su información interna.
* Eliminar su correspondiente `pollfd`.
* Informar de la desconexión.

### `recv()` devuelve `-1`

Se debe comprobar `errno`:

* `EAGAIN` o `EWOULDBLOCK`: en ese momento no quedan datos disponibles; no es una desconexión.
* `EINTR`: la operación fue interrumpida; puede intentarse de nuevo en una iteración posterior.
* Cualquier otro error: debe eliminarse el cliente.

Nunca debe ejecutarse `recv()` sobre un cliente sin haber detectado previamente `POLLIN` mediante el `poll()` central.

---

## 6. Enviar datos a los clientes

Cada cliente debería disponer de un búfer de salida con los datos pendientes de enviar.

Cuando ese búfer no esté vacío, se debe activar `POLLOUT`:

```cpp
clientDescriptor.events |= POLLOUT;
```

Cuando `poll()` indique `POLLOUT`, se puede llamar a `send()`.

`send()` puede enviar menos bytes de los solicitados. Por tanto:

1. Se eliminan del búfer solamente los bytes que realmente se enviaron.
2. El resto permanece pendiente para el siguiente `POLLOUT`.
3. Cuando el búfer queda vacío, se desactiva `POLLOUT`.

```cpp
clientDescriptor.events &= ~POLLOUT;
```

No debe mantenerse `POLLOUT` activado permanentemente. Un socket suele estar disponible para escritura casi todo el tiempo y esto haría que `poll()` regresara continuamente, provocando consumo innecesario de CPU.

Nunca se debe llamar a `send()` si `poll()` no ha indicado previamente `POLLOUT`.

---

## 7. Detectar errores y desconexiones

Para cada cliente deben comprobarse también los siguientes eventos:

* `POLLERR`: ocurrió un error en el socket.
* `POLLHUP`: el otro extremo cerró la conexión.
* `POLLNVAL`: el descriptor no es válido.

Ante cualquiera de estos eventos, el cliente debe ser eliminado de forma segura.

Si `POLLIN` y `POLLHUP` aparecen simultáneamente, puede haber últimos datos pendientes. Una implementación robusta procesa primero la lectura y después elimina el cliente si continúa conectado.

Un error en un cliente no debe detener todo el servidor.

---

## 8. Eliminar correctamente un cliente

Eliminar un cliente implica realizar todas estas operaciones:

1. Obtener su descriptor.
2. Cerrar el socket con `close()`.
3. Eliminar el objeto `Client` o su información asociada.
4. Eliminar su entrada de `pollDescriptors`.
5. Informar de la desconexión.

Hay que tener cuidado al eliminar elementos de un `std::vector` mientras se está recorriendo.

Al ejecutar `erase()`, los elementos posteriores cambian de posición. Para evitar saltarse un cliente:

* Se puede recorrer el vector de atrás hacia delante.
* O controlar manualmente el índice y no incrementarlo después de eliminar un elemento.
* O marcar los clientes y eliminarlos después de procesar los eventos.

El socket de escucha no debe tratarse como un cliente ni eliminarse mediante la lógica normal de desconexión.

---

## 9. Mantener sincronizados descriptores y clientes

El servidor necesita poder encontrar el objeto `Client` asociado a cada descriptor.

Una posible organización es mantener:

```cpp
std::vector<pollfd> pollDescriptors;
std::map<int, Client> clients;
```

El descriptor del socket puede utilizarse como identificador:

```text
descriptor → objeto Client
```

Cada vez que se acepta un cliente, debe añadirse a ambas colecciones.

Cada vez que se desconecta, debe eliminarse de ambas.

No debe quedar:

* Un descriptor dentro de `pollDescriptors` sin cliente asociado.
* Un cliente registrado cuyo descriptor ya se haya cerrado.
* Un descriptor cerrado que continúe siendo vigilado por `poll()`.

---

## 10. Flujo general del bucle

El funcionamiento conceptual debe ser:

```text
poll()
│
├── listener contiene POLLIN
│   └── accept()
│       ├── configurar socket no bloqueante
│       ├── registrar cliente
│       └── añadir pollfd
│
├── cliente contiene POLLIN
│   └── recv()
│       ├── datos recibidos → guardar en input buffer
│       ├── resultado 0 → desconectar
│       └── error fatal → desconectar
│
├── cliente contiene POLLOUT
│   └── send()
│       ├── eliminar bytes enviados
│       └── desactivar POLLOUT si no quedan datos
│
└── POLLERR, POLLHUP o POLLNVAL
    └── cerrar y eliminar cliente
```

---

## 11. Mensajes de estado recomendados

Durante esta fase resulta útil mostrar información como:

```text
[SERVER] Event loop started
[SERVER] Waiting for events
[CLIENT] Connection accepted: fd=4
[CLIENT] Received 18 bytes: fd=4
[CLIENT] Connection closed: fd=4
[CLIENT] Socket error: fd=5
[SERVER] Shutting down
```

Estos mensajes facilitan comprobar que:

* Se aceptan varios clientes.
* Cada descriptor se procesa correctamente.
* Las desconexiones se detectan.
* El servidor continúa funcionando después de eliminar un cliente.

---

## 12. Pruebas de la fase

### Iniciar el servidor

```bash
./ircserv 6667 password
```

### Abrir varios clientes

Desde distintas terminales:

```bash
nc 127.0.0.1 6667
```

Debe ser posible abrir varias conexiones simultáneamente.

### Enviar datos

Escribir texto desde cada cliente y comprobar que el servidor registra la recepción sin quedarse bloqueado.

### Desconectar clientes

Cerrar una de las conexiones y comprobar que:

* El servidor detecta la desconexión.
* Cierra el descriptor.
* Elimina el cliente.
* Los demás clientes siguen conectados.
* Se pueden conectar nuevos clientes posteriormente.

---

## Criterios para considerar terminada la fase

La fase 3 está completada cuando:

* Existe una única llamada central a `poll()`.
* El socket de escucha forma parte de `pollDescriptors`.
* Los nuevos clientes se aceptan mediante `POLLIN`.
* Los sockets aceptados se configuran como no bloqueantes.
* Se pueden mantener varios clientes conectados.
* `recv()` solamente se ejecuta después de recibir `POLLIN`.
* `send()` solamente se ejecuta después de recibir `POLLOUT`.
* `POLLOUT` solo está activado cuando existen datos pendientes.
* Las lecturas no bloquean el servidor.
* Se detecta correctamente `recv() == 0`.
* Se procesan `POLLERR`, `POLLHUP` y `POLLNVAL`.
* Los clientes desconectados se cierran y eliminan.
* No quedan descriptores cerrados dentro de `pollDescriptors`.
* La eliminación de un cliente no provoca que se omitan otros eventos.
* La desconexión de un cliente no detiene el servidor.
* Todavía no se interpretan comandos IRC.
