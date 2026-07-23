## 1. Validación de argumentos y puerto

Los puertos inferiores a `1024` son válidos, aunque normalmente necesitan permisos especiales para hacer `bind()`.

## 2. Contraseña

La contraseña debe convertirse en parte del estado interno del servidor.

No es necesario un getter que exponga la contraseña.

```cpp
bool isPasswordCorrect(const std::string &candidatePassword) const;
```

## 3. Gestión de señales

El manejador de señales no debe llamar directamente a:

```cpp
server.stop();
close(...);
std::cout << ...;
delete ...;
```

El manejador solamente modifica una variable `volatile sig_atomic_t`. Después, el flujo normal del programa detecta el cambio, sale de `run()` y ejecuta los destructores.

Las señales gestionadas son:

- `SIGINT`: aparece al pulsar `Ctrl+C`.
- `SIGTERM`: finalización solicitada mediante `kill -TERM <process_id>`
- `SIGPIPE`: se ignora para evitar que, más adelante, un `send()` a una conexión cerrada mate todo el servidor.

No usamos `SA_RESTART` porque interesa que una llamada bloqueada en `poll()` pueda ser interrumpida por la señal.

## 4. class Server

Aspectos importantes:

- `password` es un `std::string`, por lo que se guarda una copia.
- `password` es privado.
- El descriptor empieza en `-1`, que significa “no existe un descriptor abierto”.
- `Server` no se puede copiar porque en el futuro será propietario de sockets. Dos copias podrían intentar cerrar el mismo descriptor.

