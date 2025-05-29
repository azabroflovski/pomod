#ifndef POMOD_H
#define POMOD_H

#define SOCKET_PATH "/tmp/pomod.sock"
#define POMODORO_DURATION 25 * 60

typedef enum {
    CMD_START,
    CMD_STOP,
    CMD_STATUS
} CommandType;

typedef struct {
    CommandType type;
} Command;

typedef struct {
    int running;
    int seconds_left;
} StatusResponse;

#endif
