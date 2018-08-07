#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct connection_data;

typedef void (*call_back)(int, struct connection_data *);

struct connection_data
{
    int fd;
    void *arg;
    call_back read_cb;
    call_back write_cb;
    call_back error_cb;
};

