Project: TCP Counter (server + client + React frontend)
=====================================================

Short description
-----------------

This repository contains a small example system that manages a single counter:

## Project status

- `server.c`: Builds with `gcc -o server server.c` and listens on the configured port (default 12345).
- `client.c`: implemented as a small HTTP bridge (client-backend). Builds with `gcc -o client client.c`. Listens by default on `127.0.0.1:8000` and proxies REST requests to the TCP server.
- `frontend/`: calls the client-backend at `http://localhost:8000` by default. 

Technology choices
------------------

- Server: plain C + BSD sockets + select() for simplicity. This is easy to run and reason about.
- Client: plain C TCP client (interactive). For a production desktop app you'd typically split networking/backend logic from UI.
- Frontend: React + Vite. The app expects to call a local HTTP API provided by a client-backend (C) process.

Recommended architecture
------------------------------------------------

- Server C (Linux) using libevent + OpenSSL for high-performance async TLS server.
- Client-backend C: manages TCP connection to Server C and exposes a local HTTP API (using civetweb or mongoose) and optionally a WebSocket endpoint for real-time updates.
- Frontend: React (Electron or browser WebView). Communicates with client-backend via `http://localhost:PORT` and WebSocket.

Libraries you can use for HTTPS / embedded HTTP server in C:

- OpenSSL (TLS): industry standard, low-level API.
- mbedTLS / wolfSSL: lighter TLS libraries with easier integration for embedded targets.
- civetweb: embeddable, easy-to-use HTTP(S) server (MIT-like license). Good for client-backend exposing REST + WebSocket.
- mongoose: embeddable HTTP server with WebSocket support (check license for your use-case).
- libevent + OpenSSL: for building an async TLS server from sockets upward.

Build & run (quick start)
-------------------------

1) Build server and client (Linux with gcc):

```sh
gcc -o server server.c
gcc -o client client.c
```

2) Run server (default port 12345):

```sh
./server
```

3) Run client (connects to 127.0.0.1:12345):

```sh
./client
```

4) Frontend (from `frontend/`):

```sh
cd frontend
npm install
npm run dev
```

The React app is a UI that expects a local client-backend to provide REST endpoints:

- GET  /counter          -> { "value": <number> }
- POST /counter/incr     -> { "value": <number> }
- POST /counter/decr     -> { "value": <number> }
- POST /counter/reset    -> { "value": <number> }
- (optional) WebSocket /ws -> messages like { type: 'counter', value: <number> }