#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <stdbool.h>
#include <cjson/cJSON.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registra un usuario nuevo, almacenando el nombre y la IP de origen.
// Retorna true si se registró exitosamente, false si ya existe o hubo error.
bool register_user(const char *username, const char *ip);

// Cambia el estado del usuario. Retorna true si se actualizó correctamente.
bool change_user_status(const char *username, const char *new_status);

// Actualiza el timestamp de actividad para el usuario.
void update_user_activity(const char *username);

// Revisa la inactividad de los usuarios y actualiza su estado a "INACTIVO" si corresponde.
void check_inactive_users(time_t now);

// Funciones para eliminar y liberar usuarios.
void remove_user(const char *username);
void free_all_users(void);

// Obtiene la información del usuario (IP, status) en formato cJSON.
cJSON* get_user_info(const char *target);

cJSON* get_registered_users(void);


#ifdef __cplusplus
}
#endif

#endif
