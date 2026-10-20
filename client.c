#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main()
{
    int sock_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE] = {0};

    // 1. Create UDP socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Set 2-second timeout (THE TIMER)
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // 3. Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 4. Array of 4 packets to send
    char *messages[4] = {"Packet 0", "Packet 1", "Packet 2", "Packet 3"};

    // 5. Loop through each packet
    for (int i = 0; i < 4; i++)
    {
        int ack_received = 0;

        // Keep sending until an ACK is received for this packet
        while (!ack_received)
        {
            printf("[CLIENT] Sending: %s\n", messages[i]);
            sendto(sock_fd, messages[i], strlen(messages[i]), 0,
                   (struct sockaddr *)&server_addr, addr_len);

            // Wait up to 2 seconds for server reply
            int bytes_read = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *)&server_addr, &addr_len);

            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                printf("[CLIENT] Got reply: %s for %s\n\n", buffer, messages[i]);
                ack_received = 1; // Break the while loop, move to next packet
            }
            else
            {
                // bytes_read <= 0 means the 2-second timer expired!
                printf("[CLIENT] Timeout! No reply for %s. Retransmitting...\n", messages[i]);
            }
        }
    }

    // 6. Close socket
    close(sock_fd);
    return 0;
}