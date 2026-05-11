#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_fd, 5);

    printf("Server waiting for connection...\n");
while (1) {
    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("%s connected\n", client_ip);

    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int bytes = recv(client_fd, buffer, sizeof(buffer), 0);

        if (bytes <= 0 || strcmp(buffer, "bye\n") == 0) {
            printf("Client disconnected\n");
            break;
        }
        else{
        printf("Received: %s\n", buffer);
        send(client_fd, buffer, bytes, 0);
        }
    }

    close(client_fd);
}

    close(server_fd);
    return 0;
}