#include "connection_manager.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static client_node_t *client_list = NULL;

void add_client(struct lws *wsi, const char *username) {
    client_node_t *new_node = malloc(sizeof(client_node_t));
    if (!new_node) {
        log_error("Error al asignar memoria para el cliente");
        return;
    }
    new_node->wsi = wsi;
    new_node->username = strdup(username);
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
            free(to_remove->username);
            free(to_remove);
            return;
        }
        current = &((*current)->next);
    }
}

void broadcast_message(const char *message, size_t message_len) {
    client_node_t *current = client_list;
    unsigned char buf[LWS_PRE + message_len];
    memcpy(&buf[LWS_PRE], message, message_len);
    while (current) {
        lws_write(current->wsi, &buf[LWS_PRE], message_len, LWS_WRITE_TEXT);
        current = current->next;
    }
    log_info("Mensaje broadcast enviado a todos los clientes");
}

void send_private_message(const char *target, const char *message, size_t message_len) {
    client_node_t *current = client_list;
    unsigned char buf[LWS_PRE + message_len];
    memcpy(&buf[LWS_PRE], message, message_len);
    while (current) {
        if (strcmp(current->username, target) == 0) {
            lws_write(current->wsi, &buf[LWS_PRE], message_len, LWS_WRITE_TEXT);
            log_info("Mensaje privado enviado a %s", target);
            return;
        }
        current = current->next;
    }
    log_error("Usuario destino %s no encontrado", target);
}

// Función que retorna un array JSON con el nombre de cada usuario conectado.
cJSON* get_user_list(void) {
    cJSON *array = cJSON_CreateArray();
    client_node_t *current = client_list;
    while (current) {
        cJSON_AddItemToArray(array, cJSON_CreateString(current->username));
        current = current->next;
    }
    return array;
}

cJSON* get_user_info(const char *target) {
    client_node_t *current = client_list;
    while(current) {
       if(strcmp(current->username, target) == 0) {
         cJSON *info = cJSON_CreateObject();
         // Se simula una IP y estado; en una implementación real se extraería de la conexión.
         cJSON_AddStringToObject(info, "ip", "127.0.0.1");
         cJSON_AddStringToObject(info, "status", "ACTIVO");
         return info;
       }
       current = current->next;
    }
    return NULL;
}
