
Project: TCP Counter (server + client + React frontend)
=====================================================

Short description
-----------------
This repository contains a small example system that manages a single counter:

- `server.c`: a simple TCP counter server written in C. It accepts a line-based protocol (INCR, DECR, GET, RESET, QUIT) and handles multiple clients using select().
- `client.c`: a minimal interactive C client that connects to the server and sends commands typed by the user (or piped input).
- `frontend/`: a minimal React (Vite) frontend that talks to a local client-backend HTTP API (not included by default). The frontend expects a REST API at `http://localhost:8000` exposing `/counter` and endpoints to update the counter.

Project structure
-----------------

Top-level files and folders:

- `server.c`            : C TCP server (plain TCP). Build with `gcc -o server server.c`.
- `client.c`            : C interactive client. Build with `gcc -o client client.c`.
- `frontend/`           : React app (Vite). See `frontend/README.md` for running.

Technology choices
------------------

- Server: plain C + BSD sockets + select() for simplicity. This is easy to run and reason about.
- Client: plain C TCP client (interactive). For a production desktop app you'd typically split networking/backend logic from UI.
- Frontend: React + Vite. The app expects to call a local HTTP API provided by a client-backend (C) process.

Recommended architecture (your earlier proposal)
------------------------------------------------

Phương án 1 (suggested):

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
./server 12345
```

3) Run client (connects to 127.0.0.1:12345):

```sh
./client 127.0.0.1 12345
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

Troubleshooting notes (WSL / npm / esbuild)
------------------------------------------

- If you run `npm install` inside WSL and see `node: not found` or npm pointing at `/mnt/c/...`, that means your shell is invoking the Windows `npm` or Node is not installed in your WSL environment. Install Node inside WSL (recommended via `nvm`) and run `npm install` again.
- If `npm install` fails with `ETXTBSY` while installing `esbuild`, common causes are:
	- Project is on a Windows-mounted filesystem (/mnt/c) where executable binaries may be locked or incompatible. Move the project into the WSL filesystem (e.g. under `~/`) and reinstall.
	- Antivirus / Windows Defender locking the file temporarily — try closing programs that may access the path or rebooting.
	- A stale process is holding the esbuild binary — check `lsof`/`ps` and remove the file before reinstall.

If you hit those errors, recommended quick fixes:

```sh
# Prefer working inside WSL filesystem, not /mnt/c
# Example: copy project into WSL home
cp -r /mnt/c/Path/To/CK ~/
cd ~/CK/frontend
nvm install --lts   # if you use nvm; or apt install nodejs
npm install
```

Next steps / how I can help
---------------------------

- Implement the client-backend in C (civetweb) that bridges the TCP server to HTTP + WebSocket and optionally supports TLS.
- Convert the server to libevent + OpenSSL for TLS.
- Package the frontend into an Electron app that bundles the React UI and communicates with the local C backend.

If you want, tell me which next step to implement and I will add source files, build scripts and run a smoke-test.
