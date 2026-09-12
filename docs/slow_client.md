#### What a slow client represents

A slow client is one whose socket does not consume data as fast as the server produces it:

```text
Queued messages → outputBuffer grows → send() cannot empty it
```

Without a limit, a single client could grow the server’s memory indefinitely. When the limit is reached the server stores:

`Output buffer limit exceeded`

and the event loop removes it through `disconnectClient()`.
