#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Registra un usuario nuevo.
// Retorna true si el usuario se registró exitosamente,
// false si el usuario ya existía o hubo un error.
bool register_user(const char *username);

// Cambia el estado del usuario. Retorna true si se actualizó correctamente.
bool change_user_status(const char *username, const char *new_status);

// Función para eliminar un usuario de la lista (opcional).
void remove_user(const char *username);

// Libera toda la memoria asignada a la lista de usuarios.
void free_all_users(void);

#ifdef __cplusplus
}
#endif

#endif // USER_MANAGER_H
