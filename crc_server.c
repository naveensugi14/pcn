#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define GENERATOR "10011" // G(x) = x^4 + x + 1

void xor_operation(char *remainder, const char *generator, int gen_len) {
    for (int i = 1; i < gen_len; i++) {
        remainder[i - 1] = (remainder[i] == generator[i]) ? '0' : '1';
    }
}

void compute_crc(const char *frame, const char *generator, char *remainder) {
    int data_len = strlen(frame);
    int gen_len = strlen(generator);
    char current[64];

    for (int i = 0; i < gen_len; i++) {
        current[i] = frame[i];
    }

    int next_bit = gen_len;
    while (next_bit <= data_len) {
        if (current[0] == '1') {
            xor_operation(current, generator, gen_len);
        } else {
            for (int i = 1; i < gen_len; i++) {
                current[i - 1] = current[i];
            }
        }
        current[gen_len - 1] = frame[next_bit++];
    }

    current[gen_len - 1] = '\0';
    strcpy(remainder, current);
}

int main() {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[2048] = {0};
    char remainder[64] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Waiting for client connection on port %d...\n", PORT);
    client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (client_sock < 0) {
        perror("Accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Receive transmitted frame
    read(client_sock, buffer, sizeof(buffer));
    printf("[SERVER] Received Frame: %s\n", buffer);
    printf("[SERVER] Generator Polynomial: %s\n", GENERATOR);

    // Recalculate CRC remainder
    compute_crc(buffer, GENERATOR, remainder);
    printf("[SERVER] Computed Remainder: %s\n", remainder);

    int error = 0;
    for (int i = 0; i < strlen(remainder); i++) {
        if (remainder[i] != '0') {
            error = 1;
            break;
        }
    }

    if (error) {
        printf("[SERVER] RESULT: ERROR DETECTED! Frame is corrupted.\n");
        send(client_sock, "ERROR", 5, 0);
    } else {
        printf("[SERVER] RESULT: SUCCESS! Frame is error-free.\n");
        send(client_sock, "ACK", 3, 0);
    }

    close(client_sock);
    close(server_fd);
    return 0;
}