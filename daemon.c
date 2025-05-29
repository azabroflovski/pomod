#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>

#include "pomod.h"

int server_fd = -1;
int client_fd = -1;
time_t start_time = 0;
int running = 0;

void cleanup() {
    if (server_fd != -1) close(server_fd);
    if (client_fd != -1) close(client_fd);
    unlink(SOCKET_PATH);
}

void handle_command(Command *cmd, int client_sock) {
    if (cmd->type == CMD_START) {
        if (!running) {
            start_time = time(NULL);
            running = 1;
            printf("[Daemon] Pomodoro started.\n");
        }
    } else if (cmd->type == CMD_STOP) {
        if (running) {
            running = 0;
            start_time = 0;
            printf("[Daemon] Pomodoro stopped.\n");
        }
    } else if (cmd->type == CMD_STATUS) {
        StatusResponse resp;
        if (running) {
            time_t now = time(NULL);
            int elapsed = now - start_time;
            int remaining = POMODORO_DURATION - elapsed;
            if (remaining <= 0) {
                running = 0;
                start_time = 0;
                remaining = 0;
                printf("[Daemon] Pomodoro finished.\n");
            }
            resp.running = 1;
            resp.seconds_left = remaining;
        } else {
            resp.running = 0;
            resp.seconds_left = 0;
        }
        send(client_sock, &resp, sizeof(resp), 0);
    }
}

void signal_handler(int sig) {
    cleanup();
    exit(0);
}

int main() {
    struct sockaddr_un addr;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    unlink(SOCKET_PATH);

    if ((server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        cleanup();
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        cleanup();
        exit(EXIT_FAILURE);
    }

    printf("[Daemon] Listening on %s\n", SOCKET_PATH);

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        Command cmd;
        ssize_t bytes = recv(client_fd, &cmd, sizeof(cmd), 0);
        if (bytes == sizeof(cmd)) {
            handle_command(&cmd, client_fd);
        }
        close(client_fd);
    }

    cleanup();
    return 0;
}
