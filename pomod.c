#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "pomod.h"

int send_command(CommandType type) {
    int sock;
    struct sockaddr_un addr;
    Command cmd = { .type = type };

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock);
        return 1;
    }

    if (send(sock, &cmd, sizeof(cmd), 0) == -1) {
        perror("send");
        close(sock);
        return 1;
    }

    if (type == CMD_STATUS) {
        StatusResponse resp;
        ssize_t bytes = recv(sock, &resp, sizeof(resp), 0);
        if (bytes != sizeof(resp)) {
            printf("Error receiving status.\n");
        } else if (resp.running) {
            printf("Pomodoro running. Time left: %02d:%02d\n", resp.seconds_left / 60, resp.seconds_left % 60);
        } else {
            printf("No Pomodoro session is running.\n");
        }
    }

    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s {start|stop|status}\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        return send_command(CMD_START);
    } else if (strcmp(argv[1], "stop") == 0) {
        return send_command(CMD_STOP);
    } else if (strcmp(argv[1], "status") == 0) {
        return send_command(CMD_STATUS);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return 1;
    }
}
