#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <libwebsockets.h>
#include <cjson/cJSON.h>

typedef struct client_node {
    struct lws *wsi;
    char *username;
    struct client_node *next;
} client_node_t;

// Agrega un cliente a la lista de conexiones.
void add_client(struct lws *wsi, const char *username);

// Remueve un cliente de la lista de conexiones.
void remove_client(struct lws *wsi);

// Envía un mensaje a todos los clientes conectados.
void broadcast_message(const char *message, size_t message_len);

// Envía un mensaje privado a un cliente específico.
void send_private_message(const char *target, const char *message, size_t message_len);

// Retorna un objeto cJSON que representa un array con los nombres de los usuarios conectados.
cJSON* get_user_list(void);

// Retorna un objeto cJSON con la información del usuario identificado por 'target'.
// Si el usuario no se encuentra, retorna NULL.
cJSON* get_user_info(const char *target);


#endif
