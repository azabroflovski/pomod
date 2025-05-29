#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/pomod.sock"

int main() {
  printf("salom bolla  \n");
  return EXIT_SUCCESS;
}
