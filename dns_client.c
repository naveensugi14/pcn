/*
 * File: dns_client.c
 * Assignment 5: Simulation of Domain Name Resolution using UDP Sockets
 * Description: UDP DNS Client with interactive keyboard input, local client-side caching,
 *              resolution latency measurement, and timeout/NXDOMAIN error handling.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_IP "127.0.0.1"
#define PORT 5300
#define BUFFER_SIZE 512
#define MAX_CACHE 16
#define TIMEOUT_SEC 2

typedef struct {
    char domain[128];
    char ip[64];
} cache_entry_t;

cache_entry_t client_cache[MAX_CACHE];
int cache_count = 0;

// High-resolution timestamp helper
double get_time_in_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000.0) + (tv.tv_usec / 1000.0);
}

// Search client-side cache
int search_cache(const char *domain, char *ip_out) {
    for (int i = 0; i < cache_count; i++) {
        if (strcasecmp(client_cache[i].domain, domain) == 0) {
            strncpy(ip_out, client_cache[i].ip, 64);
            return 1; // Cache HIT
        }
    }
    return 0; // Cache MISS
}

// Add entry to client-side cache
void add_to_cache(const char *domain, const char *ip) {
    if (cache_count < MAX_CACHE) {
        strncpy(client_cache[cache_count].domain, domain, 127);
        strncpy(client_cache[cache_count].ip, ip, 63);
        cache_count++;
    }
}

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char domain_query[128], buffer[BUFFER_SIZE];
    char cached_ip[64];
    const char *target_server = (argc > 1) ? argv[1] : SERVER_IP;

    // 1. Create UDP Datagram Socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("[-] socket() failed");
        exit(EXIT_FAILURE);
    }

    // 2. Set Socket Receive Timeout (2 seconds)
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // 3. Configure Server Address
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, target_server, &server_addr.sin_addr);

    printf("====================================================\n");
    printf("        UDP DNS CLIENT WITH LOCAL CACHING           \n");
    printf("====================================================\n");
    printf("[*] Configured DNS Server: %s:%d\n\n", target_server, PORT);

    while (1) {
        printf("----------------------------------------------------\n");
        printf("Enter Domain Name to Resolve (or 'exit' to quit): ");
        if (!fgets(domain_query, sizeof(domain_query), stdin)) break;
        domain_query[strcspn(domain_query, "\r\n")] = 0;

        if (strcmp(domain_query, "exit") == 0) break;
        if (strlen(domain_query) == 0) continue;

        // Step 1: Check Client-Side Cache
        double t_start = get_time_in_ms();
        if (search_cache(domain_query, cached_ip)) {
            double t_cached = get_time_in_ms() - t_start;
            printf("\n[+] [CACHE HIT] Domain found in client-side cache!\n");
            printf("    Domain Name : %s\n", domain_query);
            printf("    IP Address  : %s\n", cached_ip);
            printf("    Lookup Time : %.3f ms (Local Cache Hit)\n\n", t_cached);
            continue;
        }

        printf("[*] [CACHE MISS] Querying authoritative DNS Server...\n");

        // Step 2: Send UDP Datagram to DNS Server
        double t_send = get_time_in_ms();
        sendto(sock_fd, domain_query, strlen(domain_query), 0,
               (struct sockaddr *)&server_addr, addr_len);

        // Step 3: Receive Server Response
        bzero(buffer, BUFFER_SIZE);
        ssize_t bytes_recv = recvfrom(sock_fd, buffer, sizeof(buffer) - 1, 0,
                                      (struct sockaddr *)&server_addr, &addr_len);
        double t_recv = get_time_in_ms();
        double rtt_latency = t_recv - t_send;

        if (bytes_recv < 0) {
            printf("\n[-] [ERROR: TIMEOUT] Request timed out after %d seconds.\n", TIMEOUT_SEC);
            printf("    Server did not respond or packet was dropped.\n\n");
        } else {
            buffer[bytes_recv] = '\0';
            if (strncmp(buffer, "SUCCESS:", 8) == 0) {
                char *resolved_ip = buffer + 8;
                printf("\n[+] [DNS RESOLUTION SUCCESSFUL]\n");
                printf("    Domain Name  : %s\n", domain_query);
                printf("    IP Address   : %s\n", resolved_ip);
                printf("    Response RTT : %.2f ms\n", rtt_latency);
                printf("    Protocol     : UDP (Port %d)\n", PORT);

                // Add to client cache
                add_to_cache(domain_query, resolved_ip);
                printf("    [*] Added \"%s\" -> %s to client cache.\n\n", domain_query, resolved_ip);
            } else if (strncmp(buffer, "ERROR:NXDOMAIN", 14) == 0) {
                printf("\n[-] [ERROR: NXDOMAIN]\n");
                printf("    Domain \"%s\" does not exist on DNS server.\n", domain_query);
                printf("    Response RTT : %.2f ms\n\n", rtt_latency);
            } else {
                printf("\n[-] [SERVER ERROR RESPONSE]\n");
                printf("    Status : %s\n", buffer);
                printf("    RTT    : %.2f ms\n\n", rtt_latency);
            }
        }
    }

    close(sock_fd);
    printf("[*] DNS Client session closed.\n");
    return 0;
}
