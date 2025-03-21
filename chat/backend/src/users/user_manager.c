#include "user_manager.h"
#include "logger.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>

typedef struct user_node {
    char *username;
    char *ip;
    char *status;         // "ACTIVO", "OCUPADO", "INACTIVO"
    time_t last_activity; // Última actividad (timestamp)
    struct user_node *next;
} user_node_t;

static user_node_t *user_list = NULL;

bool register_user(const char *username, const char *ip) {
    user_node_t *current = user_list;
    while (current) {
        if (strcmp(current->username, username) == 0)
            return false;
        current = current->next;
    }
    user_node_t *new_node = malloc(sizeof(user_node_t));
    if (!new_node)
        return false;
    new_node->username = strdup(username);
    new_node->ip = strdup(ip);
    new_node->status = strdup("ACTIVO");
    new_node->last_activity = time(NULL);
    if (!new_node->username || !new_node->ip || !new_node->status) {
        free(new_node->username);
        free(new_node->ip);
        free(new_node->status);
        free(new_node);
        return false;
    }
    new_node->next = user_list;
    user_list = new_node;
    return true;
}

bool change_user_status(const char *username, const char *new_status) {
    user_node_t *current = user_list;
    while (current) {
        if (strcmp(current->username, username) == 0) {
            free(current->status);
            current->status = strdup(new_status);
            return current->status != NULL;
        }
        current = current->next;
    }
    return false;
}

void update_user_activity(const char *username) {
    user_node_t *current = user_list;
    time_t now = time(NULL);
    while (current) {
        if (strcmp(current->username, username) == 0) {
            current->last_activity = now;
            // Si el usuario estaba inactivo, reactívalo
            if (strcmp(current->status, "INACTIVO") == 0) {
                free(current->status);
                current->status = strdup("ACTIVO");
                log_info("Usuario %s reactivado", username);
            }
            return;
        }
        current = current->next;
    }
}


void check_inactive_users(time_t now) {
    user_node_t *current = user_list;
    while (current) {
        if (strcmp(current->status, "INACTIVO") != 0) {
            if ((now - current->last_activity) >= INACTIVITY_TIMEOUT) {
                log_info("Usuario %s inactivo por %ld segundos, cambiando estado a INACTIVO",
                         current->username, now - current->last_activity);
                free(current->status);
                current->status = strdup("INACTIVO");
            }
        }
        current = current->next;
    }
}

void remove_user(const char *username) {
    user_node_t **current = &user_list;
    while (*current) {
        if (strcmp((*current)->username, username) == 0) {
            user_node_t *to_delete = *current;
            *current = to_delete->next;
            free(to_delete->username);
            free(to_delete->ip);
            free(to_delete->status);
            free(to_delete);
            return;
        }
        current = &((*current)->next);
    }
}

void free_all_users(void) {
    user_node_t *current = user_list;
    while (current) {
        user_node_t *next = current->next;
        free(current->username);
        free(current->ip);
        free(current->status);
        free(current);
        current = next;
    }
    user_list = NULL;
}

cJSON* get_user_info(const char *target) {
    user_node_t *current = user_list;
    while (current) {
        if (strcmp(current->username, target) == 0) {
            cJSON *info = cJSON_CreateObject();
            cJSON_AddStringToObject(info, "ip", current->ip);
            cJSON_AddStringToObject(info, "status", current->status);
            return info;
        }
        current = current->next;
    }
    return NULL;
}
