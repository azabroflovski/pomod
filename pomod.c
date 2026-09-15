/* pomod: tiny pomodoro timer. one binary, two roles: the daemon keeps
 * the timer, everything else is a client talking to it over a unix socket.
 *
 * the protocol is one line of text each way, so you can poke it by hand:
 *   echo status | nc -U $TMPDIR/pomod.sock
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_MINUTES 25
#define MAX_MINUTES     (24 * 60)

static volatile sig_atomic_t quit;

static void on_signal(int sig) {
    (void)sig;
    quit = 1;
}

/* seconds on a clock that doesn't jump when someone changes the system
 * time, and keeps ticking while the laptop sleeps */
static time_t now(void) {
    struct timespec ts;
#ifdef CLOCK_BOOTTIME
    clock_gettime(CLOCK_BOOTTIME, &ts);  /* linux */
#else
    clock_gettime(CLOCK_MONOTONIC, &ts); /* macos, counts sleep already */
#endif
    return ts.tv_sec;
}

static int sock_addr(struct sockaddr_un *addr) {
    const char *dir = getenv("XDG_RUNTIME_DIR");
    if (!dir) dir = getenv("TMPDIR");
    if (!dir) dir = "/tmp";

    int dirlen = strlen(dir);
    while (dirlen > 1 && dir[dirlen - 1] == '/') dirlen--;

    memset(addr, 0, sizeof *addr);
    addr->sun_family = AF_UNIX;
    int n = snprintf(addr->sun_path, sizeof addr->sun_path, "%.*s/pomod.sock", dirlen, dir);
    if (n < 0 || (size_t)n >= sizeof addr->sun_path) {
        fprintf(stderr, "pomod: socket path too long: %.*s/pomod.sock\n", dirlen, dir);
        return -1;
    }
    return 0;
}

static int dial(const struct sockaddr_un *addr) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr *)addr, sizeof *addr) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* read one line (or whatever arrives before EOF) and chop the newline */
static void read_line(int fd, char *buf, size_t size) {
    size_t len = 0;
    ssize_t n;
    while (len < size - 1 && (n = read(fd, buf + len, size - 1 - len)) > 0) {
        len += n;
        if (memchr(buf, '\n', len)) break;
    }
    buf[len] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';
}

/* ---- daemon ---- */

static void run_hook(const char *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        /* ignored signals survive exec, don't pass that on to the hook */
        signal(SIGPIPE, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }
}

struct timer {
    time_t end;  /* when the running pomodoro ends, 0 if it isn't running */
    time_t left; /* seconds left on a paused one, 0 if it isn't paused */
};

static void say_left(int fd, const char *prefix, time_t left) {
    dprintf(fd, "%s%02d:%02d left\n", prefix, (int)(left / 60), (int)(left % 60));
}

/* t is the same "now" the main loop just used to check if the pomodoro is
 * over, so a running timer always has at least a second left in here */
static void handle_client(int srv, struct timer *timer, time_t t) {
    int c = accept(srv, NULL, NULL);
    if (c < 0) return;

    /* a client that connects and says nothing must not freeze the daemon */
    struct timeval tv = { .tv_sec = 1 };
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    char line[64];
    read_line(c, line, sizeof line);

    char *arg = line + strcspn(line, " ");
    if (*arg) *arg++ = '\0';

    time_t left = timer->end ? timer->end - t : timer->left;

    if (line[0] == '\0') {
        /* empty connect, another `pomod daemon` checking if we're alive */
    } else if (strcmp(line, "start") == 0) {
        char *e = arg;
        long min = *arg ? strtol(arg, &e, 10) : DEFAULT_MINUTES;
        if (*e || min < 1 || min > MAX_MINUTES) {
            dprintf(c, "error: minutes should be 1..%d, got '%s'\n", MAX_MINUTES, arg);
        } else if (timer->end) {
            say_left(c, "already running, ", left);
        } else if (timer->left) {
            say_left(c, "paused, ", left);
        } else {
            timer->end = t + min * 60;
            dprintf(c, "started, %ld min\n", min);
            printf("pomod: started, %ld min\n", min);
        }
    } else if (strcmp(line, "pause") == 0) {
        if (timer->end) {
            timer->left = left;
            timer->end = 0;
            say_left(c, "paused, ", left);
            printf("pomod: paused\n");
        } else if (timer->left) {
            say_left(c, "already paused, ", left);
        } else {
            dprintf(c, "not running\n");
        }
    } else if (strcmp(line, "resume") == 0) {
        if (timer->left) {
            timer->end = t + timer->left;
            timer->left = 0;
            say_left(c, "resumed, ", left);
            printf("pomod: resumed\n");
        } else if (timer->end) {
            say_left(c, "already running, ", left);
        } else {
            dprintf(c, "not running\n");
        }
    } else if (strcmp(line, "stop") == 0) {
        if (timer->end || timer->left) {
            *timer = (struct timer){ 0 };
            dprintf(c, "stopped\n");
            printf("pomod: stopped\n");
        } else {
            dprintf(c, "not running\n");
        }
    } else if (strcmp(line, "status") == 0) {
        if (timer->end)
            say_left(c, "", left);
        else if (timer->left)
            say_left(c, "paused, ", left);
        else
            dprintf(c, "not running\n");
    } else {
        dprintf(c, "error: unknown command '%s'\n", line);
    }

    close(c);
}

static int serve(const char *hook) {
    struct sockaddr_un addr;
    if (sock_addr(&addr) < 0) return 1;

    int fd = dial(&addr);
    if (fd >= 0) {
        close(fd);
        fprintf(stderr, "pomod: daemon already running on %s\n", addr.sun_path);
        return 1;
    }
    unlink(addr.sun_path); /* nobody answered, it's a leftover from a crash */

    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return 1;
    }
    fcntl(srv, F_SETFD, FD_CLOEXEC); /* don't leak it into hooks */

    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(srv, 8) < 0) {
        perror(addr.sun_path);
        close(srv);
        return 1;
    }

    /* no SA_RESTART, so poll/read return EINTR and we get to clean up */
    struct sigaction sa = { .sa_handler = on_signal };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN); /* client left before the reply: EPIPE, not death */
    signal(SIGCHLD, SIG_IGN); /* finished hooks get reaped by the kernel */

    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("pomod: listening on %s\n", addr.sun_path);

    struct timer timer = { 0 };

    while (!quit) {
        /* idle or paused: sleep until someone connects. running: also wake up
         * every second to look at the clock. an exact timeout would fire late
         * after suspend, linux doesn't count sleep time in poll timeouts */
        struct pollfd p = { .fd = srv, .events = POLLIN };
        int r = poll(&p, 1, timer.end ? 1000 : -1);
        if (r < 0 && errno != EINTR) {
            perror("poll");
            break;
        }

        time_t t = now();
        if (timer.end && t >= timer.end) {
            timer.end = 0;
            printf("pomod: done\a\n");
            if (hook) run_hook(hook);
        }

        if (r > 0) handle_client(srv, &timer, t);
    }

    close(srv);
    unlink(addr.sun_path);
    return 0;
}

/* ---- client ---- */

static int request(const char *cmd, const char *arg) {
    struct sockaddr_un addr;
    if (sock_addr(&addr) < 0) return 1;

    int fd = dial(&addr);
    if (fd < 0) {
        fprintf(stderr, "pomod: can't reach %s, is `pomod daemon` running?\n", addr.sun_path);
        return 1;
    }

    dprintf(fd, "%s %s\n", cmd, arg);

    char reply[128];
    read_line(fd, reply, sizeof reply);
    close(fd);

    if (reply[0] == '\0') {
        fprintf(stderr, "pomod: no reply from daemon\n");
        return 1;
    }
    if (strncmp(reply, "error: ", 7) == 0) {
        fprintf(stderr, "pomod: %s\n", reply + 7);
        return 1;
    }
    puts(reply);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3 || argv[1][0] == '-') {
        fprintf(stderr,
            "usage: pomod daemon [cmd]   run the timer, cmd goes to sh when a pomodoro ends\n"
            "       pomod start [min]    start a pomodoro, %d min by default\n"
            "       pomod pause\n"
            "       pomod resume\n"
            "       pomod stop\n"
            "       pomod status\n", DEFAULT_MINUTES);
        return 1;
    }

    if (strcmp(argv[1], "daemon") == 0)
        return serve(argc == 3 ? argv[2] : NULL);

    return request(argv[1], argc == 3 ? argv[2] : "");
}
