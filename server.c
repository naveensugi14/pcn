#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main()
{
    int sock_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE] = {0};

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Binding failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("[UDP SERVER] Listening on port %d..\n", PORT);

    int dropped = 0;
    int count = 0;

    while (count < 4)
    {
        // FIX 1: Pass &addr_len (pointer)
        int bytes_read = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0,
                                  (struct sockaddr *)&client_addr, &addr_len);
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';

            if (strcmp(buffer, "Packet 2") == 0 && dropped == 0)
            {
                printf("Dropped %s on purpose \n", buffer);
                dropped = 1;
            }
            else
            {
                printf("[server] Accepted : %s\n", buffer);

                char *reply = "ACK";
                // FIX 2: Pass addr_len (value)
                sendto(sock_fd, reply, strlen(reply), 0,
                       (struct sockaddr *)&client_addr, addr_len);
                count++;
            }
        }
    }

    close(sock_fd);
    return 0;
}