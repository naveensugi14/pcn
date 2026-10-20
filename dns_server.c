/*
 * File: dns_server.c
 * Assignment 5: Simulation of Domain Name Resolution using UDP Sockets
 * Description: UDP DNS Server maintaining a domain-to-IP database table,
 *              resolving queries, and handling NXDOMAIN, invalid format, and timeout errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 5300
#define BUFFER_SIZE 512

typedef struct {
    char domain[128];
    char ip[64];
} dns_record_t;

// Simulated DNS Database Table (Hostname -> IP)
dns_record_t dns_table[] = {
    {"google.com", "142.250.193.206"},
    {"www.google.com", "142.250.193.206"},
    {"ssn.edu.in", "182.74.148.138"},
    {"www.ssn.edu.in", "182.74.148.138"},
    {"github.com", "140.82.121.4"},
    {"wikipedia.org", "198.35.26.96"},
    {"openai.com", "104.18.32.115"},
    {"microsoft.com", "20.112.52.29"},
    {"yahoo.com", "98.137.11.163"},
    {"annauniv.edu", "210.212.245.14"}
};
int dns_table_size = sizeof(dns_table) / sizeof(dns_record_t);

// Function to validate domain name format
int is_valid_domain(const char *domain) {
    if (strlen(domain) < 3 || strlen(domain) > 120) return 0;
    if (strchr(domain, '.') == NULL) return 0;
    for (int i = 0; domain[i] != '\0'; i++) {
        if (!isalnum(domain[i]) && domain[i] != '.' && domain[i] != '-') return 0;
    }
    return 1;
}

// Function to search DNS database
const char *resolve_dns(const char *query_domain) {
    for (int i = 0; i < dns_table_size; i++) {
        if (strcasecmp(dns_table[i].domain, query_domain) == 0) {
            return dns_table[i].ip;
        }
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE], response[BUFFER_SIZE];

    // 1. Create UDP Socket
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        perror("[-] Error: socket() failed");
        exit(EXIT_FAILURE);
    }

    // 2. Configure Server Address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    // 3. Bind Socket to Port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error: bind() failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("====================================================\n");
    printf("   SIMULATED UDP DNS SERVER (PORT %d)              \n", PORT);
    printf("====================================================\n");
    printf("[*] Loaded %d authoritative DNS records into memory table.\n", dns_table_size);
    printf("[*] DNS Server is listening for incoming UDP queries...\n\n");

    while (1) {
        client_len = sizeof(client_addr);
        bzero(buffer, BUFFER_SIZE);
        bzero(response, BUFFER_SIZE);

        ssize_t bytes_read = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *)&client_addr, &client_len);
        if (bytes_read < 0) continue;

        buffer[bytes_read] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0;

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        printf("[<-] Query from %s:%d: \"%s\"\n", client_ip, client_port, buffer);

        // Timeout simulation trigger
        if (strcmp(buffer, "timeout.test") == 0) {
            printf("    [!] Simulating server timeout / packet drop for \"%s\"\n\n", buffer);
            continue; // Dropping datagram without response
        }

        // Format validation
        if (!is_valid_domain(buffer)) {
            snprintf(response, sizeof(response), "ERROR:INVALID_FORMAT (Invalid domain syntax)");
            printf("    [x] Response: %s\n\n", response);
        } else {
            const char *ip_result = resolve_dns(buffer);
            if (ip_result != NULL) {
                snprintf(response, sizeof(response), "SUCCESS:%s", ip_result);
                printf("    [+] Resolved \"%s\" -> %s (Status: NOERROR)\n\n", buffer, ip_result);
            } else {
                snprintf(response, sizeof(response), "ERROR:NXDOMAIN (Non-Existent Domain)");
                printf("    [-] Domain \"%s\" not found in database (Status: NXDOMAIN)\n\n", buffer);
            }
        }

        sendto(server_fd, response, strlen(response), 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    close(server_fd);
    return 0;
}
