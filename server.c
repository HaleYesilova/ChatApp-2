#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct
{
    int socket;
    char nickname[32];
    bool is_active;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(char *message, int sender_socket);
void handle_client_disconnect(int socket);
void send_client_list(int requester_socket);
void handle_private_message(char *message, int sender_socket);
char *get_client_nickname(int socket);

char *get_client_nickname(int socket)
{
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket == socket && clients[i].is_active)
        {
            return clients[i].nickname;
        }
    }
    return "Unknown";
}

void handle_client_disconnect(int socket)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket == socket)
        {
            char disconnect_msg[BUFFER_SIZE];
            sprintf(disconnect_msg, "%s has left the chat!", clients[i].nickname);
            clients[i].is_active = false;
            broadcast_message(disconnect_msg, socket);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void broadcast_message(char *message, int sender_socket)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].is_active)
        {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_client_list(int requester_socket)
{
    char list[BUFFER_SIZE] = "Çevrimiçi kullanıcılar:\n";
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].is_active)
        {
            char user_entry[64];
            sprintf(user_entry, "- %s\n", clients[i].nickname);
            strcat(list, user_entry);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    send(requester_socket, list, strlen(list), 0);
}

void handle_private_message(char *message, int sender_socket)
{
    char *target = strtok(message + 5, " "); // Skip "/msg "
    if (!target)
    {
        send(sender_socket, "Kullanım: /msg <kullanıcı> <mesaj>", strlen("Kullanım: /msg <kullanıcı> <mesaj>"), 0);
        return;
    }

    char *content = strtok(NULL, "");
    if (!content)
    {
        send(sender_socket, "Kullanım: /msg <kullanıcı> <mesaj>", strlen("Kullanım: /msg <kullanıcı> <mesaj>"), 0);
        return;
    }

    char formatted_msg[BUFFER_SIZE];
    char *sender_nick = get_client_nickname(sender_socket);
    sprintf(formatted_msg, "[Özel Mesaj] %s: %s", sender_nick, content);

    pthread_mutex_lock(&clients_mutex);
    bool found = false;
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].is_active && strcmp(clients[i].nickname, target) == 0)
        {
            send(clients[i].socket, formatted_msg, strlen(formatted_msg), 0);
            // Gönderene de mesajın iletildiğini bildir
            send(sender_socket, formatted_msg, strlen(formatted_msg), 0);
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (!found)
    {
        char error_msg[BUFFER_SIZE];
        sprintf(error_msg, "Kullanıcı '%s' bulunamadı.", target);
        send(sender_socket, error_msg, strlen(error_msg), 0);
    }
}

void *handle_client(void *arg)
{
    int client_socket = *(int *)arg;
    char buffer[BUFFER_SIZE];

    // Request nickname
    send(client_socket, "NICK", 4, 0);
    recv(client_socket, buffer, BUFFER_SIZE, 0);

    pthread_mutex_lock(&clients_mutex);
    strncpy(clients[client_count - 1].nickname, buffer, 31);
    clients[client_count - 1].is_active = true;
    pthread_mutex_unlock(&clients_mutex);

    // Broadcast join message
    char join_msg[BUFFER_SIZE];
    sprintf(join_msg, "%s has joined the chat!", buffer);
    broadcast_message(join_msg, client_socket);

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (bytes_received <= 0)
        {
            handle_client_disconnect(client_socket);
            return NULL;
        }
        printf("%s\n", buffer);
        // Handle commands
        if (buffer[0] == '/')
        {
            if (strncmp(buffer, "/list", 5) == 0)
            {
                send_client_list(client_socket);
            }
            else if (strncmp(buffer, "/msg", 4) == 0)
            {
                handle_private_message(buffer, client_socket);
            }
            continue;
        }

        broadcast_message(buffer, client_socket);
    }
    return NULL;
}

int main()
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        perror("WSAStartup failed");
        return EXIT_FAILURE;
    }
#endif

    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Sohbet sunucusu başlatıldı. Port: %d\n", PORT);

    while (1)
    {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        if (client_count >= MAX_CLIENTS)
        {
            printf("Maksimum bağlantı sayısına ulaşıldı!\n");
            send(new_socket, "Sunucu dolu!", strlen("Sunucu dolu!"), 0);
#ifdef _WIN32
            closesocket(new_socket);
#else
            close(new_socket);
#endif
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }

        clients[client_count].socket = new_socket;
        clients[client_count].is_active = false;
        client_count++;
        pthread_mutex_unlock(&clients_mutex);

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)&new_socket);
        pthread_detach(thread_id);
    }

#ifdef _WIN32
    closesocket(server_fd);
    WSACleanup();
#else
    close(server_fd);
#endif

    return 0;
}