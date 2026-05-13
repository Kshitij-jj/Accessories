#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENT 5
#define BUFFER_SIZE 1024

int main() {

    int server_fd;
    int client_fd[MAX_CLIENT];

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t addr_len = sizeof(client_addr);

    char buffer[BUFFER_SIZE + 1];

    

    for(int i = 0; i < MAX_CLIENT; i++) {
        client_fd[i] = -1;
    }

   

    server_fd = socket(AF_INET,
                       SOCK_STREAM,
                       0);

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

    

    if(listen(server_fd, MAX_CLIENT) == -1) {

        perror("listen");

        close(server_fd);

        return 1;
    }

    

    int flags = fcntl(server_fd,
                      F_GETFL,
                      0);

    fcntl(server_fd,
          F_SETFL,
          flags | O_NONBLOCK);

    printf("Server running on port %d...\n",
           PORT);

    while(1) {

        

        while(1) {

            int new_fd = accept(server_fd,
                                (struct sockaddr*)&client_addr,
                                &addr_len);

            if(new_fd == -1) {

                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK) {

                    break;
                }

                perror("accept");

                break;
            }

           

            flags = fcntl(new_fd,
                          F_GETFL,
                          0);

            fcntl(new_fd,
                  F_SETFL,
                  flags | O_NONBLOCK);

          

            int added = 0;

            for(int i = 0; i < MAX_CLIENT; i++) {

                if(client_fd[i] == -1) {

                    client_fd[i] = new_fd;

                    added = 1;

                    printf("Client %d connected\n",
                           i);

                    break;
                }
            }

            

            if(!added) {

                printf("Server full\n");

                close(new_fd);
            }
        }

        

        for(int i = 0; i < MAX_CLIENT; i++) {

            if(client_fd[i] == -1)
                continue;

            int bytes = recv(client_fd[i],
                             buffer,
                             BUFFER_SIZE,
                             0);

            

            if(bytes > 0) {

                buffer[bytes] = '\0';

                

                buffer[strcspn(buffer, "\n")] = '\0';

                

                if(strcmp(buffer, "exit") == 0) {

                    printf("Client %d disconnected\n",
                           i);

                    close(client_fd[i]);

                    client_fd[i] = -1;

                    continue;
                }

                printf("Client %d: %s\n",
                       i,
                       buffer);

              

                for(int j = 0; j < MAX_CLIENT; j++) {

                    if(client_fd[j] == -1)
                        continue;

             

                    if(j == i)
                        continue;

                    send(client_fd[j],
                         buffer,
                         strlen(buffer),
                         0);
                }
            }

           

            else if(bytes == 0) {

                printf("Client %d disconnected\n",
                       i);

                close(client_fd[i]);

                client_fd[i] = -1;
            }


            else {

                if(errno == EAGAIN ||
                   errno == EWOULDBLOCK) {

                    continue;
                }

                perror("recv");

                close(client_fd[i]);

                client_fd[i] = -1;
            }
        }

      

        usleep(10000);
    }

    close(server_fd);

    return 0;
}