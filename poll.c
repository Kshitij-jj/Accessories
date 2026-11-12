#include "mylib_error_functions.h" // from the my custom library
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_CLIENTS 1020
#define BUF_SPACE 4096

struct client
{
    int fd;
    char buf[BUF_SPACE];
    size_t len;  /* bytes currently in buf          */
    size_t sent; /* how many of those were written  */
};

struct pollfd fds[MAX_CLIENTS];
struct client Clients[MAX_CLIENTS];
nfds_t nfds = 0;

int set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        return -1;
    }
    return 0;
}

int create_listner(const char *port_str)
{
    int fd, opt = 1;
    char *endstr;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1)
    {
        return -1;
    }

    long port = strtol(port_str, &endstr, 10);
    if (*endstr != '\0' || endstr == port_str)
    {
        return -1;
    }
    if (port < 1 || port > 65535)
    {
        return -1;
    }
    addr.sin_port = htons((uint16_t)port);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        return -1;
    }
    if (set_nonblocking(fd) == -1)
    {
        close(fd);
        return -1;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(fd);
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&addr, len) == -1)
    {
        close(fd);
        return -1;
    }
    if (listen(fd, MAX_CLIENTS) == -1)
    {
        close(fd);
        return -1;
    }
    return fd;
}

int add_client(int fd)
{
    if (nfds == MAX_CLIENTS)
    {
        return -1;
    }
    fds[nfds].fd = fd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;
    Clients[nfds].fd = fd;
    Clients[nfds].len = 0;
    Clients[nfds].sent = 0;
    ++nfds;
    return (int)nfds - 1;
}

void remove_client(nfds_t i)
{
    if (i == 0)
    {
        return;
    }
    close(fds[i].fd);
    if (i != nfds - 1)
    {
        fds[i] = fds[nfds - 1];
        Clients[i] = Clients[nfds - 1];
    }
    --nfds;
}

void on_listner_ready(int listner)
{
    for (;;)
    {
        int fd = accept(listner, NULL, NULL);
        if (fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            if (errno == EINTR || errno == ECONNABORTED)
            {
                continue;
            }

            perror("accept");
            return;
        }
        if (set_nonblocking(fd) == -1)
        {
            ml_warn("Couldn't set Non-Blocking.");
            close(fd);
            continue;
        }
        if (add_client(fd) == -1)
        {
            ml_warn("Max Client reached.");
            close(fd);
            continue;
        }
    }
}

int on_client_readable(nfds_t i)
{
    ssize_t nr = read(fds[i].fd, Clients[i].buf, BUF_SPACE);
    if (nr > 0)
    {
        printf("Read %zd bytes from fd %d\n", nr, Clients[i].fd);
        Clients[i].len = (size_t)nr;
        Clients[i].sent = 0;
        fds[i].events = POLLOUT;
        return 0;
    }
    if (nr == 0)
    {
        return -1;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    {
        return 0;
    }
    return -1;
}

int on_client_writable(nfds_t i)
{
    ssize_t nw = write(Clients[i].fd, Clients[i].buf + Clients[i].sent,
                       Clients[i].len - Clients[i].sent);
    if (nw >= 0)
    {
        printf("Wrote %zd bytes to fd %d\n", nw, Clients[i].fd);
        Clients[i].sent += (size_t)nw;
        if (Clients[i].sent == Clients[i].len)
        {
            Clients[i].len = 0;
            Clients[i].sent = 0;
            fds[i].events = POLLIN;
        }

        return 0;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
    {
        return 0;
    }
    return -1;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        ml_fatal("Usage: ./server <port>");
    }
    signal(SIGPIPE, SIG_IGN);

    int listner = create_listner(argv[1]);
    if (listner == -1)
    {
        ml_fatal("Listening Socket: Err");
    }

    fds[0].fd = listner;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    nfds = 1;

    for (;;)
    {
        int n = poll(fds, nfds, -1);
        if (n == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            ml_err_exit("POLL: ");
        }

        for (int i = (int)nfds - 1; i > 0; i--)
        {
            if (fds[i].revents == 0)
            {
                continue;
            }

            int dead;
            if (fds[i].revents & POLLNVAL)
            {
                dead = 1;
            }
            else if (fds[i].events & POLLOUT)
            {
                dead = (on_client_writable((nfds_t)i) == -1);
            }
            else
            {
                dead = (on_client_readable((nfds_t)i) == -1);
            }

            if (dead)
            {
                remove_client((nfds_t)i);
            }
        }

        if (fds[0].revents & POLLIN)
        {
            on_listner_ready(listner);
        }
    }
    return 0;
}