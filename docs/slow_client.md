#### Qué representa un cliente lento

Un cliente lento es aquel cuyo socket no consume datos tan rápido como el servidor los genera:

```text
Mensajes encolados → outputBuffer crece → send() no consigue vaciarlo
```

Sin límite, un único cliente podría hacer crecer indefinidamente la memoria del servidor. Al alcanzar el límite se guarda:

`Output buffer limit exceeded`

y el bucle de eventos lo elimina mediante `disconnectClient()`.