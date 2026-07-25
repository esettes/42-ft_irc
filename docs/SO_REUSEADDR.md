`SO_REUSEADDR` es importante activarlo porque permite reiniciar tu servidor y volver a asociarlo al mismo puerto inmediatamente, sin tener que esperar a que el sistema operativo libere por completo las conexiones anteriores.

El problema que evita

Cuando cierras un servidor TCP, algunas conexiones pueden permanecer temporalmente en estado `TIME_WAIT`. Esto es parte del funcionamiento normal de TCP: evita que paquetes atrasados de una conexión antigua interfieran con una conexión nueva.

Sin `SO_REUSEADDR`, al reiniciar rápidamente:

`./ircserv 6667 password`

`bind()` podría fallar con:

`bind: Address already in use`

aunque el servidor anterior ya no esté ejecutándose.

Con esta opción activada:

```cpp
int reuseAddressOption = 1;

::setsockopt(
    listenSocket,
    SOL_SOCKET,
    SO_REUSEADDR,
    &reuseAddressOption,
    sizeof(reuseAddressOption)
);
```

el sistema permite que el nuevo socket vuelva a utilizar esa dirección y ese puerto.

### Lo que no hace

`SO_REUSEADDR` no permite normalmente iniciar dos servidores escuchando simultáneamente en la misma dirección y el mismo puerto.

Si ya tienes una instancia activa:

`./ircserv 6667 password`

y ejecutas otra:

`./ircserv 6667 password`

la segunda debería seguir fallando en `bind()` porque el primer servidor todavía posee el puerto.

Es decir:

| Situación                               |      Sin `SO_REUSEADDR` |              Con `SO_REUSEADDR` |
| --------------------------------------- | ----------------------: | ------------------------------: |
| Reiniciar rápidamente el servidor       |            Puede fallar |            Normalmente funciona |
| Puerto ocupado por otro servidor activo |                   Falla |                  Sigue fallando |
| Conexiones antiguas en `TIME_WAIT`      | Pueden impedir `bind()` | Se permite reutilizar el puerto |


No debe confundirse con `SO_REUSEPORT`, que sí está relacionado con permitir que varios sockets utilicen el mismo puerto bajo determinadas condiciones.

### Por qué se configura antes de `bind()`

La opción afecta a la asociación del socket con una dirección. Por eso el orden debe ser:

```text
socket()
    ↓
setsockopt(SO_REUSEADDR)
    ↓
bind()
    ↓
listen()
```

Activarla después de `bind()` sería demasiado tarde: `bind()` ya podría haber fallado.

Durante el desarrollo de `ft_irc`, hay que detener y reiniciar el servidor continuamente. Sin `SO_REUSEADDR`, habría que esperar antes de volver a utilizar el puerto `6667`, lo que haría las pruebas muy incómodas.