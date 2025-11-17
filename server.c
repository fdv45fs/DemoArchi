/* Simple TCP counter server
 * Usage: ./server [port]
 * Default port: 12345
 * Protocol (line-based, commands terminated by '\n'):
 *   INCR   -> increments counter, replies with OK
 *   DECR   -> decrements counter, replies with OK
 *   GET    -> replies with current counter value
 *   RESET  -> sets counter to 0, replies with OK
 *   QUIT   -> server closes the client connection
 * Replies are line-terminated by '\n'.
 * The server handles multiple clients using select().
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
#include <fcntl.h>
#include <sys/select.h>
#include <strings.h> /* for strcasecmp */

#define DEFAULT_PORT 12345
#define MAX_CLIENTS FD_SETSIZE
#define BUFSIZE 4096

struct client {
	int fd;
	char buf[BUFSIZE];
	int len;
};

static long long counter = 0;

static int make_listener(int port) {
	int sockfd;
	struct sockaddr_in addr;
	int opt = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		perror("socket");
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		close(sockfd);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(sockfd);
		return -1;
	}

	if (listen(sockfd, 16) < 0) {
		perror("listen");
		close(sockfd);
		return -1;
	}

	return sockfd;
}

static void remove_client(struct client *clients, int i, int listener, int *maxfd) {
	if (clients[i].fd >= 0) close(clients[i].fd);
	clients[i].fd = -1;
	clients[i].len = 0;
	clients[i].buf[0] = '\0';

	// recompute maxfd: must be at least listener
	*maxfd = listener;
	for (int j = 0; j < MAX_CLIENTS; ++j) if (clients[j].fd > *maxfd) *maxfd = clients[j].fd;
}

static void handle_command(const char *cmd, int fd) {
	char out[128];

	if (strcasecmp(cmd, "INCR") == 0) {
		counter++;
		snprintf(out, sizeof(out), "OK\n");
		send(fd, out, strlen(out), 0);
	} else if (strcasecmp(cmd, "DECR") == 0) {
		counter--;
		snprintf(out, sizeof(out), "OK\n");
		send(fd, out, strlen(out), 0);
	} else if (strcasecmp(cmd, "GET") == 0) {
		snprintf(out, sizeof(out), "%lld\n", counter);
		send(fd, out, strlen(out), 0);
	} else if (strcasecmp(cmd, "RESET") == 0) {
		counter = 0;
		snprintf(out, sizeof(out), "OK\n");
		send(fd, out, strlen(out), 0);
	} else if (strcasecmp(cmd, "QUIT") == 0) {
		// client side will close after receiving no more data; server will close fd elsewhere
		snprintf(out, sizeof(out), "BYE\n");
		send(fd, out, strlen(out), 0);
		// caller should close
	} else {
		snprintf(out, sizeof(out), "ERR Unknown command\n");
		send(fd, out, strlen(out), 0);
	}
}

int main(int argc, char **argv) {
	int port = DEFAULT_PORT;
	int listener;
	struct client clients[MAX_CLIENTS];
	fd_set readfds;
	int maxfd;

	if (argc >= 2) port = atoi(argv[1]);

	listener = make_listener(port);
	if (listener < 0) exit(EXIT_FAILURE);

	for (int i = 0; i < MAX_CLIENTS; ++i) {
		clients[i].fd = -1;
		clients[i].len = 0;
		clients[i].buf[0] = '\0';
	}

	printf("Counter server listening on port %d\n", port);

	maxfd = listener;

	while (1) {
		FD_ZERO(&readfds);
		FD_SET(listener, &readfds);
		for (int i = 0; i < MAX_CLIENTS; ++i) {
			if (clients[i].fd >= 0) FD_SET(clients[i].fd, &readfds);
		}

		int nready = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		if (nready < 0) {
			if (errno == EINTR) continue;
			perror("select");
			break;
		}

		if (FD_ISSET(listener, &readfds)) {
			struct sockaddr_in cliaddr;
			socklen_t clilen = sizeof(cliaddr);
			int connfd = accept(listener, (struct sockaddr*)&cliaddr, &clilen);
			if (connfd < 0) {
				perror("accept");
			} else {
				// find slot
				int i;
				for (i = 0; i < MAX_CLIENTS; ++i) if (clients[i].fd < 0) break;
				if (i == MAX_CLIENTS) {
					fprintf(stderr, "Too many clients, rejecting\n");
					close(connfd);
				} else {
					clients[i].fd = connfd;
					clients[i].len = 0;
					clients[i].buf[0] = '\0';
					if (connfd > maxfd) maxfd = connfd;
					char ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
					printf("New connection from %s:%d (fd=%d)\n", ip, ntohs(cliaddr.sin_port), connfd);
				}
			}
			if (--nready <= 0) continue;
		}

		for (int i = 0; i < MAX_CLIENTS; ++i) {
			int fd = clients[i].fd;
			if (fd < 0) continue;
			if (FD_ISSET(fd, &readfds)) {
				char tmp[BUFSIZE];
				ssize_t r = recv(fd, tmp, sizeof(tmp) - 1, 0);
				if (r <= 0) {
					if (r == 0) {
						printf("Client fd=%d closed connection\n", fd);
					} else {
						perror("recv");
					}
					remove_client(clients, i, listener, &maxfd);
				} else {
					tmp[r] = '\0';
					// append to client's buffer
					if (clients[i].len + r >= BUFSIZE - 1) {
						// overflow, reset
						clients[i].len = 0;
						clients[i].buf[0] = '\0';
					}
					memcpy(clients[i].buf + clients[i].len, tmp, r);
					clients[i].len += r;
					clients[i].buf[clients[i].len] = '\0';

					// process lines
					char *line_start = clients[i].buf;
					char *newline;
					while ((newline = strchr(line_start, '\n')) != NULL) {
						*newline = '\0';
						// trim CR if present
						if (newline > line_start && *(newline - 1) == '\r') *(newline - 1) = '\0';
						// handle command
						if (strcasecmp(line_start, "QUIT") == 0) {
							handle_command(line_start, fd);
							remove_client(clients, i, listener, &maxfd);
							break; // client removed
						} else {
							handle_command(line_start, fd);
						}
						line_start = newline + 1;
					}

					// if some leftover data, move it to beginning
					if (clients[i].fd >= 0) {
						int leftover = clients[i].buf + clients[i].len - line_start;
						if (leftover > 0 && line_start != clients[i].buf) memmove(clients[i].buf, line_start, leftover);
						clients[i].len = leftover;
						clients[i].buf[clients[i].len] = '\0';
					}
				}
				if (--nready <= 0) break;
			}
		}
	}

	close(listener);
	return 0;
}
