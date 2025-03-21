#include "user_manager.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definición de la estructura para cada nodo de usuario.
typedef struct user_node {
    char *username;
    char *status;   // Nuevo campo para almacenar el estado (ACTIVO, OCUPADO, INACTIVO)
    struct user_node *next;
} user_node_t;

// Lista global de usuarios.
static user_node_t *user_list = NULL;

bool register_user(const char *username) {
    // Verificar si el usuario ya existe.
    user_node_t *current = user_list;
    while (current != NULL) {
        if (strcmp(current->username, username) == 0) {
            return false;
        }
        current = current->next;
    }
    
    // Crear un nuevo nodo para el usuario.
    user_node_t *new_node = malloc(sizeof(user_node_t));
    if (new_node == NULL) {
        return false;
    }
    
    new_node->username = strdup(username);
    if (new_node->username == NULL) {
        free(new_node);
        return false;
    }
    
    // Establecer el estado por defecto a "ACTIVO"
    new_node->status = strdup("ACTIVO");
    if (new_node->status == NULL) {
        free(new_node->username);
        free(new_node);
        return false;
    }
    
    // Insertar el nuevo nodo al inicio de la lista.
    new_node->next = user_list;
    user_list = new_node;
    
    return true;
}

bool change_user_status(const char *username, const char *new_status) {
    user_node_t *current = user_list;
    while (current != NULL) {
        if (strcmp(current->username, username) == 0) {
            // Actualiza el estado: libera el anterior y asigna el nuevo
            free(current->status);
            current->status = strdup(new_status);
            if (current->status == NULL) {
                return false;
            }
            return true;
        }
        current = current->next;
    }
    return false;
}

void remove_user(const char *username) {
    user_node_t **current = &user_list;
    while (*current != NULL) {
        if (strcmp((*current)->username, username) == 0) {
            user_node_t *to_delete = *current;
            *current = to_delete->next;
            free(to_delete->username);
            free(to_delete->status);
            free(to_delete);
            return;
        }
        current = &((*current)->next);
    }
}

void free_all_users(void) {
    user_node_t *current = user_list;
    while (current != NULL) {
        user_node_t *next = current->next;
        free(current->username);
        free(current->status);
        free(current);
        current = next;
    }
    user_list = NULL;
}
