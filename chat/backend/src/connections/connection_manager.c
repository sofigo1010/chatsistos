#include "connection_manager.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static client_node_t *client_list = NULL;

/* Busca un cliente por su wsi */
static client_node_t* find_client_by_wsi(struct lws *wsi) {
    client_node_t *current = client_list;
    while (current) {
        if (current->wsi == wsi)
            return current;
        current = current->next;
    }
    return NULL;
}

/* Busca un cliente por su nombre de usuario */
static client_node_t* find_client_by_username(const char *username) {
    client_node_t *current = client_list;
    while (current) {
        if (strcmp(current->username, username) == 0)
            return current;
        current = current->next;
    }
    return NULL;
}

void add_client(struct lws *wsi, const char *username) {
    client_node_t *new_node = malloc(sizeof(client_node_t));
    if (!new_node) {
        log_error("Error al asignar memoria para el cliente");
        return;
    }
    new_node->wsi = wsi;
    new_node->username = strdup(username);
    new_node->pending_head = NULL;
    new_node->pending_tail = NULL;
    new_node->next = client_list;
    client_list = new_node;
    log_info("Cliente agregado: %s", username);
}

void remove_client(struct lws *wsi) {
    client_node_t **current = &client_list;
    while (*current) {
        if ((*current)->wsi == wsi) {
            client_node_t *to_remove = *current;
            *current = to_remove->next;
            log_info("Cliente removido: %s", to_remove->username);
            // Liberar mensajes pendientes
            pending_msg_t *msg = to_remove->pending_head;
            while (msg) {
                pending_msg_t *tmp = msg;
                msg = msg->next;
                free(tmp->data);
                free(tmp);
            }
            free(to_remove->username);
            free(to_remove);
            return;
        }
        current = &((*current)->next);
    }
}

void enqueue_pending_message(struct lws *wsi, const char *message, size_t message_len) {
    client_node_t *client = find_client_by_wsi(wsi);
    if (!client) {
        log_error("enqueue_pending_message: cliente no encontrado");
        return;
    }
    // Agregar un '\n' al final del mensaje.
    size_t new_len = message_len + 1; // espacio adicional para el newline
    pending_msg_t *new_msg = malloc(sizeof(pending_msg_t));
    if (!new_msg) {
        log_error("Error al asignar memoria para pending_msg");
        return;
    }
    new_msg->data = malloc(new_len);
    if (!new_msg->data) {
        log_error("Error al asignar memoria para pending_msg->data");
        free(new_msg);
        return;
    }
    memcpy(new_msg->data, message, message_len);
    new_msg->data[message_len] = '\n';  // Agrega el salto de línea
    new_msg->len = new_len;
    new_msg->next = NULL;
    
    if (client->pending_tail == NULL) {
        client->pending_head = new_msg;
        client->pending_tail = new_msg;
    } else {
        client->pending_tail->next = new_msg;
        client->pending_tail = new_msg;
    }
    
    // Solicitar que se invoque el callback de escritura para este wsi.
    lws_callback_on_writable(wsi);
}


void write_pending_messages(struct lws *wsi) {
    client_node_t *client = find_client_by_wsi(wsi);
    if (!client)
        return;
    
    while (client->pending_head) {
        pending_msg_t *msg = client->pending_head;
        client->pending_head = msg->next;
        if (client->pending_head == NULL)
            client->pending_tail = NULL;
        
        unsigned char buffer[LWS_PRE + msg->len];
        memcpy(&buffer[LWS_PRE], msg->data, msg->len);
        int n = lws_write(wsi, &buffer[LWS_PRE], msg->len, LWS_WRITE_TEXT);
        if (n < (int)msg->len) {
            log_error("lws_write retornó %d (se esperaba %zu)", n, msg->len);
        } else {
            log_info("Se enviaron %zu bytes a %s", msg->len, client->username);
        }
        free(msg->data);
        free(msg);
    }
}

void broadcast_message(const char *message, size_t message_len) {
    client_node_t *current = client_list;
    while (current) {
        enqueue_pending_message(current->wsi, message, message_len);
        current = current->next;
    }
    log_info("Mensaje broadcast encolado para todos los clientes");
}

void send_private_message(const char *target, const char *message, size_t message_len) {
    client_node_t *client = find_client_by_username(target);
    if (client) {
        enqueue_pending_message(client->wsi, message, message_len);
        log_info("Mensaje privado encolado para %s", target);
    } else {
        log_error("Usuario destino %s no encontrado", target);
    }
}

cJSON* get_user_list(void) {
    cJSON *array = cJSON_CreateArray();
    client_node_t *current = client_list;
    while (current) {
        cJSON_AddItemToArray(array, cJSON_CreateString(current->username));
        current = current->next;
    }
    return array;
}

