#include "mylib_error_functions.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#define MAX_SERVICE 100
#define BUF_SPACE 1024
typedef struct
{
    int fd;
    char buf[BUF_SPACE];
    size_t len;  /* bytes currently in buf          */
    size_t sent; /* how many of those were written  */
} client;
client;

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

int change_state(int epfd, client *c, uint32_t event)
{
    struct epoll_event revent;
    revent.events = event;
    revent.data.ptr = c;
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &revent) == -1)
    {
        return -1;
    }
    return 0;
}
int remove_from_epoll_instance(int epfd, client *c)
{
    int res = epoll_ctl(epfd, EPOLL_CTL_DEL, c->fd, NULL);

    close(c->fd);
    free(c);
    return res;
}
int add_to_epoll_instance(int epfd, int fd, uint32_t event)
{
    struct epoll_event revent;

    revent.events = event;
    client *tmp = malloc(sizeof(client));
    if (tmp == NULL)
    {
        return -1;
    }
    tmp->fd = fd;
    tmp->len = 0;
    tmp->sent = 0;
    revent.data.ptr = tmp;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &revent) == -1)
    {
        free(tmp);
        return -1;
    }
    return 0;
}

int create_listner(int port)
{
    int fd, opt = 1;

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1)
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
    if (listen(fd, MAX_SERVICE) == -1)
    {
        close(fd);
        return -1;
    }
    return fd;
}
int process_client_for_reading(client *c)
{
    if (c->len == BUF_SPACE)
    {
        return 1; /* Alter the Client state from READ to WRITE*/
    }
    ssize_t nr;
    nr = read(c->fd, (c->buf) + (c->len), BUF_SPACE - (c->len));
    if (nr == 0)
    {
        return -1;
    }
    if (nr < 0)
    {
        if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
        {
            return 0; /* Continue the loop serving others*/
        }
        else
        {
            ml_print_err_no("READ: ");
            return -1; /* Client has disconnected remove this.*/
        }
    }
    c->len += nr;
    return 1;
}
int process_client_for_writing(client *c)
{
    if (c->len == 0)
    {
        return 0;
    }
    if (c->len < c->sent)
    {
        return -1;
    }
    ssize_t nw;
    nw = write(c->fd, c->buf + c->sent, c->len - c->sent);
    if (nw >= 0)
    {
        c->sent += nw;
        if (c->sent == c->len)
        {
            c->sent = 0;
            c->len = 0;
            return 2; /* Change State from WRITE to READ*/
        }
        else
        {

            return 1; /*Chnage State from READ to WRITE*/
        }
    }
    else
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
        {
            return 0;
        }
        else
        {
            return -1;
        }
    }
}
void process_listner(int epfd, int fd)
{
    do
    {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            if (errno == EINTR || errno == ECONNABORTED)
            {
                continue;
            }
            ml_print_err_no("Process_Client: ");
            return;
        }

        if (set_nonblocking(client_fd) == -1)
        {
            ml_warn("Failed to set Non-Blocking. Skipping...!");
            close(client_fd);
            break;
        }
        if (add_to_epoll_instance(epfd, client_fd, EPOLLIN) == -1)
        {
            ml_warn("Failed to set add to epoll_instance. Skipping...!");
            close(client_fd);
            break;
        }

    } while (true);

    return;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        ml_fatal("Usage: ./server <port>");
    }

    int port = ml_str_to_num(argv[1]);

    if (!IN_RANGE(port, 0, 65535))
    {
        ml_fatal("Invalid Port.");
    }

    int listner_fd;

    if ((listner_fd = create_listner(port)) == -1)
    {
        ml_err_exit("Listner Socket: ");
    }

    int epfd = epoll_create1(0);

    if (epfd == -1)
    {
        ml_err_exit("Epoll: ");
    }

    if (add_to_epoll_instance(epfd, listner_fd, EPOLLIN) == -1)
    {
        ml_err_exit("Failed to add listner fd in epoll instance.");
    }
    int m;
    struct epoll_event events[MAX_SERVICE];
    do
    {
        m = epoll_wait(epfd, events, MAX_SERVICE, -1);
        if (m == -1)
        {
            ml_err_exit("EPOLL_WAIT: ");
        }
        else
        {
            int i;
            for (i = 0; i < m; i++)
            {

                if (((client *)(events[i].data.ptr))->fd == listner_fd)
                {
                    process_listner(epfd, listner_fd);
                    continue;
                }

                if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
                {
                    if (remove_from_epoll_instance(epfd, events[i].data.ptr) == -1)
                    {
                        ml_warn("Failed to close a client.");
                    }
                    continue;
                }

                if (events[i].events & EPOLLIN)
                {
                    int n = process_client_for_reading((client *)events[i].data.ptr);
                    if (n < 0)
                    {
                        if (remove_from_epoll_instance(epfd, events[i].data.ptr) == -1)
                        {
                            ml_warn("Failed to close a client.");
                        }
                        continue;
                    }
                    else if (n == 0)
                    {
                        continue;
                    }
                    else
                    {
                        if (change_state(epfd, events[i].data.ptr, EPOLLOUT) == -1)
                        {
                            ml_warn("Failed to change state of a client.");
                        }
                    }
                }
                if (events[i].events & EPOLLOUT)
                {
                    int res = process_client_for_writing(events[i].data.ptr);
                    if (res < 0)
                    {
                        if (remove_from_epoll_instance(epfd, events[i].data.ptr) == -1)
                        {
                            ml_warn("Failed to close a client.");
                        }
                        continue;
                    }
                    else if (res == 2)
                    {
                        if (change_state(epfd, events[i].data.ptr, EPOLLIN) == -1)
                        {
                            ml_warn("Failed to change State. ");
                        }
                    }
                    continue;
                }
            }
        }

    } while (1);

    return 0;
}