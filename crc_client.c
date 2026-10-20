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

void compute_crc(const char *dividend, const char *generator, char *checksum) {
    int data_len = strlen(dividend);
    int gen_len = strlen(generator);
    char current[64];

    for (int i = 0; i < gen_len; i++) {
        current[i] = dividend[i];
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
        current[gen_len - 1] = dividend[next_bit++];
    }

    current[gen_len - 1] = '\0';
    strcpy(checksum, current);
}

// Convert input string data into its binary equivalent
void text_to_binary(const char *text, char *binary) {
    binary[0] = '\0';
    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        for (int j = 7; j >= 0; j--) {
            if ((ch >> j) & 1)
                strcat(binary, "1");
            else
                strcat(binary, "0");
        }
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char text[128];
    char binary_data[1024] = {0};
    char augmented_data[1024] = {0};
    char checksum[64] = {0};
    char transmitted_frame[1024] = {0};
    char response[64] = {0};

    printf("=== SENDER (CRC) ===\n");
    printf("Enter string data: ");
    if (fgets(text, sizeof(text), stdin) != NULL) {
        text[strcspn(text, "\n")] = 0; // Remove newline
    }

    // Step 1: Convert data to binary
    text_to_binary(text, binary_data);
    printf("Binary Equivalent:  %s\n", binary_data);
    printf("Generator:          %s\n", GENERATOR);

    // Step 2: Append zeros (generator length - 1 = 4 zeros)
    strcpy(augmented_data, binary_data);
    int gen_len = strlen(GENERATOR);
    for (int i = 0; i < gen_len - 1; i++) {
        strcat(augmented_data, "0");
    }

    // Step 3: Compute CRC checksum
    compute_crc(augmented_data, GENERATOR, checksum);
    printf("Generated Checksum: %s\n", checksum);

    // Step 4: Construct transmitted frame
    strcpy(transmitted_frame, binary_data);
    strcat(transmitted_frame, checksum);
    printf("Original Frame:     %s\n", transmitted_frame);

    // Step 5: Option to manually inject error
    char choice;
    printf("\nIntroduce bit error before sending? (y/n): ");
    scanf(" %c", &choice);
    if (choice == 'y' || choice == 'Y') {
        int pos;
        printf("Enter bit index to flip (0 to %lu): ", strlen(transmitted_frame) - 1);
        scanf("%d", &pos);
        if (pos >= 0 && pos < strlen(transmitted_frame)) {
            transmitted_frame[pos] = (transmitted_frame[pos] == '0') ? '1' : '0';
            printf("Corrupted Frame:    %s\n", transmitted_frame);
        }
    }

    // Step 6: Connect to Server via TCP Socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection to server failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Transmit frame
    send(sock, transmitted_frame, strlen(transmitted_frame), 0);
    read(sock, response, sizeof(response));
    printf("\n[SENDER] Server Response: %s\n", response);

    close(sock);
    return 0;
}