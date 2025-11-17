
/* Client-backend HTTP bridge for the TCP counter server
 * - Connects to the TCP counter server (host/port)
 * - Runs a small HTTP server on localhost (default port 8000) exposing REST endpoints:
 *     GET  /counter
 *     POST /counter/incr
 *     POST /counter/decr
 *     POST /counter/reset
 *
 * The HTTP server is intentionally tiny and single-threaded; it's suitable for local
 * desktop use and for the example React frontend in `frontend/`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 12345
#define DEFAULT_LISTEN_PORT 8000
#define BUFSIZE 8192

static int connect_to_server(const char *host, int port) {
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", host);
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// Send a single line command (terminated with \n) to the counter server and read back one line
static int send_command_and_read(int *psock, const char *cmd, char *out, size_t outlen, const char *host, int port) {
    int sock = *psock;
    ssize_t n;
    const char *term = "\n";

    if (sock < 0) {
        sock = connect_to_server(host, port);
        if (sock < 0) return -1;
        *psock = sock;
    }

    // send command
    size_t cmdlen = strlen(cmd);
    char tmp[256];
    if (cmdlen + 1 >= sizeof(tmp)) return -1;
    snprintf(tmp, sizeof(tmp), "%s\n", cmd);
    if (send(sock, tmp, strlen(tmp), 0) < 0) {
        perror("send to server");
        close(sock);
        *psock = -1;
        return -1;
    }

    // read one line reply
    size_t pos = 0;
    while (pos + 1 < outlen) {
        n = recv(sock, out + pos, 1, 0);
        if (n <= 0) {
            if (n == 0) fprintf(stderr, "server closed connection\n");
            else perror("recv");
            close(sock);
            *psock = -1;
            return -1;
        }
        if (out[pos] == '\r') continue;
        if (out[pos] == '\n') {
            out[pos] = '\0';
            return 0;
        }
        pos += n;
    }
    out[outlen-1] = '\0';
    return 0;
}

// Simple HTTP helpers
static void write_http_response(int clientfd, int status, const char *status_text, const char *content_type, const char *body) {
    char header[1024];
    int body_len = body ? strlen(body) : 0;
    int n = snprintf(header, sizeof(header), "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n", status, status_text, content_type, body_len);
    send(clientfd, header, n, 0);
    if (body_len > 0) send(clientfd, body, body_len, 0);
}

static void handle_client(int clientfd, int *psock, const char *host, int port) {
    char buf[BUFSIZE];
    ssize_t n = recv(clientfd, buf, sizeof(buf)-1, 0);
    if (n <= 0) { close(clientfd); return; }
    buf[n] = '\0';

    // parse request line
    char method[16], path[256];
    if (sscanf(buf, "%15s %255s", method, path) != 2) {
        write_http_response(clientfd, 400, "Bad Request", "text/plain", "Bad Request\n");
        close(clientfd);
        return;
    }

    // route
    if (strcmp(method, "GET") == 0 && strcmp(path, "/counter") == 0) {
        char reply[256];
        if (send_command_and_read(psock, "GET", reply, sizeof(reply), host, port) == 0) {
            char body[256];
            snprintf(body, sizeof(body), "{\"value\": %s}\n", reply);
            write_http_response(clientfd, 200, "OK", "application/json", body);
        } else {
            write_http_response(clientfd, 500, "Internal Server Error", "application/json", "{\"error\": \"server unreachable\"}\n");
        }
    } else if (strcmp(method, "POST") == 0 && (strcmp(path, "/counter/incr") == 0 || strcmp(path, "/counter/decr") == 0 || strcmp(path, "/counter/reset") == 0)) {
        const char *cmd = NULL;
        if (strcmp(path, "/counter/incr") == 0) cmd = "INCR";
        else if (strcmp(path, "/counter/decr") == 0) cmd = "DECR";
        else if (strcmp(path, "/counter/reset") == 0) cmd = "RESET";

        if (!cmd) {
            write_http_response(clientfd, 404, "Not Found", "application/json", "{\"error\": \"not found\"}\n");
        } else {
            char tmp[256];
            if (send_command_and_read(psock, cmd, tmp, sizeof(tmp), host, port) == 0) {
                // after OK, fetch value
                char reply[256];
                if (send_command_and_read(psock, "GET", reply, sizeof(reply), host, port) == 0) {
                    char body[256];
                    snprintf(body, sizeof(body), "{\"value\": %s}\n", reply);
                    write_http_response(clientfd, 200, "OK", "application/json", body);
                } else {
                    write_http_response(clientfd, 500, "Internal Server Error", "application/json", "{\"error\": \"read failed\"}\n");
                }
            } else {
                write_http_response(clientfd, 500, "Internal Server Error", "application/json", "{\"error\": \"command failed\"}\n");
            }
        }
    } else if (strcmp(method, "OPTIONS") == 0) {
        // respond for CORS preflight
        const char *hdr = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(clientfd, hdr, strlen(hdr), 0);
    } else {
        write_http_response(clientfd, 404, "Not Found", "application/json", "{\"error\": \"not found\"}\n");
    }

    close(clientfd);
}

int main(int argc, char **argv) {
    const char *host = DEFAULT_HOST;
    int port = DEFAULT_PORT;
    int listen_port = DEFAULT_LISTEN_PORT;
    int upstream_sock = -1;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = atoi(argv[2]);
    if (argc >= 4) listen_port = atoi(argv[3]);

    printf("Client-backend: will connect to server %s:%d and listen on 127.0.0.1:%d\n", host, port, listen_port);

    // Create HTTP listener on 127.0.0.1:listen_port
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) { perror("socket"); return 1; }
    int opt = 1; setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(listen_port);

    if (bind(listenfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); close(listenfd); return 1; }
    if (listen(listenfd, 16) < 0) { perror("listen"); close(listenfd); return 1; }

    for (;;) {
        struct sockaddr_in cliaddr;
        socklen_t clilen = sizeof(cliaddr);
        int clientfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen);
        if (clientfd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        handle_client(clientfd, &upstream_sock, host, port);
    }

    if (upstream_sock >= 0) close(upstream_sock);
    close(listenfd);
    return 0;
}

