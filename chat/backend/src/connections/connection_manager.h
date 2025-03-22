#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <libwebsockets.h>
#include <cjson/cJSON.h>

/* Estructura para representar un mensaje pendiente de envío */
typedef struct pending_msg_s {
    char *data;
    size_t len;
    struct pending_msg_s *next;
} pending_msg_t;

/* Estructura para representar un cliente */
typedef struct client_node {
    struct lws *wsi;
    char *username;
    pending_msg_t *pending_head;  // Cola de mensajes pendientes
    pending_msg_t *pending_tail;
    struct client_node *next;
} client_node_t;

/* Funciones de manejo de conexiones */
void add_client(struct lws *wsi, const char *username);
void remove_client(struct lws *wsi);
void broadcast_message(const char *message, size_t message_len);
void send_private_message(const char *target, const char *message, size_t message_len);
cJSON* get_user_list(void);

/* La función get_user_info se implementa en user_manager.c,
   por lo que aquí solo se declara para que otros módulos la usen */
cJSON* get_user_info(const char *target);

/* Funciones para encolar y enviar mensajes pendientes */
void enqueue_pending_message(struct lws *wsi, const char *msg, size_t msg_len);
void write_pending_messages(struct lws *wsi);

#endif
