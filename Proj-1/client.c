#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[1024];

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));

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
    close(sock);
    return 0;
}
