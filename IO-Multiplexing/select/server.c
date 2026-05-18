#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

int main() {

    int server_fd;
    int client_fd;
    int max_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t addr_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE + 1];

    fd_set master_set;
    fd_set read_set;


    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if(server_fd == -1) {
        perror("socket");
        return 1;
    }


    int opt = 1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));


    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

if(bind(server_fd,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr)) == -1) {

        perror("bind");

        close(server_fd);

        return 1;
    }


    if(listen(server_fd, MAX_CLIENTS) == -1) {

        perror("listen");

        close(server_fd);

        return 1;
    }


    FD_ZERO(&master_set);

    FD_SET(server_fd, &master_set);

    max_fd = server_fd;

    printf("Server running on port %d...\n", PORT);

    while(1) {

      
        read_set = master_set;

        int activity = select(max_fd + 1,
                              &read_set,
                              NULL,
                              NULL,
                              NULL);

        if(activity == -1) {
            perror("select");
            break;
        }


        for(int fd = 0; fd <= max_fd; fd++) {

      

            if(!FD_ISSET(fd, &read_set))
                continue;

       
            if(fd == server_fd) {

                client_fd = accept(server_fd,
                                   (struct sockaddr*)&client_addr,
                                   &addr_len);

                if(client_fd == -1) {
                    perror("accept");
                    continue;
                }

                FD_SET(client_fd, &master_set);

                if(client_fd > max_fd)
                    max_fd = client_fd;

                printf("New client connected (fd=%d)\n",
                       client_fd);
            }

         
            else {

                int bytes = recv(fd,
                                 buffer,
                                 BUFFER_SIZE,
                                 0);

            
                if(bytes == 0) {

                    printf("Client disconnected (fd=%d)\n",
                           fd);

                    close(fd);

                    FD_CLR(fd, &master_set);
                }

                else if(bytes < 0) {

                    perror("recv");

                    close(fd);

                    FD_CLR(fd, &master_set);
                }

              
                else {

                    buffer[bytes] = '\0';

                    buffer[strcspn(buffer, "\n")] = '\0';

                    printf("Client %d: %s\n",
                           fd,
                           buffer);

                    

                    for(int j = 0; j <= max_fd; j++) {

                        if(!FD_ISSET(j, &master_set))
                            continue;

                        if(j == server_fd)
                            continue;

                        if(j == fd)
                            continue;

                        send(j,
                             buffer,
                             strlen(buffer),
                             0);

                        send(j,
                             "\n",
                             1,
                             0);
                    }
                }
            }
        }
    }

    close(server_fd);

    return 0;
}