#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

int sock;

void* send_thread(void* arg) {

    char buffer[1024];

    while(1) {

        fgets(buffer, sizeof(buffer), stdin);

        send(sock,
             buffer,
             strlen(buffer),
             0);
    }

    return NULL;
}

void* recv_thread(void* arg) {

    char buffer[1024];

    while(1) {

        int bytes = recv(sock,
                         buffer,
                         sizeof(buffer) - 1,
                         0);

        if(bytes > 0) {

            buffer[bytes] = '\0';

            printf("Message: %s", buffer);
        }

        else if(bytes == 0) {

            printf("Server disconnected\n");

            close(sock);

            return NULL;
        }

        else {

            perror("recv");

            close(sock);

            return NULL;
        }
    }

    return NULL;
}

int main() {

    struct sockaddr_in server_addr;

    pthread_t sender;
    pthread_t receiver;

    sock = socket(AF_INET,
                  SOCK_STREAM,
                  0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    inet_pton(AF_INET,
              "127.0.0.1",
              &server_addr.sin_addr);

    connect(sock,
            (struct sockaddr*)&server_addr,
            sizeof(server_addr));

    printf("Connected to server\n");

    pthread_create(&sender,
                   NULL,
                   send_thread,
                   NULL);

    pthread_create(&receiver,
                   NULL,
                   recv_thread,
                   NULL);

    pthread_join(sender, NULL);
    pthread_join(receiver, NULL);

    close(sock);

    return 0;
}