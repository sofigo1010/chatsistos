#include "thread_manager.h"
#include "logger.h"
#include "time_utils.h"
#include "user_manager.h"
#include "connection_manager.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include <libwebsockets.h>

// Estructura para representar una tarea en la cola
typedef struct task_s {
    struct lws *wsi;
    char *msg;
    size_t msg_len;
    struct task_s *next;
} task_t;

// Cola de tareas (FIFO)
static task_t *task_queue = NULL;
static task_t *task_queue_tail = NULL;

// Mecanismos de sincronización
static pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

// Arreglo de hilos
static pthread_t *threads = NULL;
static size_t thread_count = 0;
static bool stop_pool = false;

static pthread_t monitor_thread;

// Prototipo de la función que procesará la lógica del mensaje
static void process_message(struct lws *wsi, const char *msg, size_t msg_len);

// Función principal de cada hilo en el pool
static void *worker_thread(void *arg) {
    while (true) {
        pthread_mutex_lock(&queue_mutex);

        // Espera hasta que haya una tarea o se indique stop_pool
        while (!stop_pool && task_queue == NULL) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }

        // Si se está cerrando el pool y no hay más tareas, salimos
        if (stop_pool && task_queue == NULL) {
            pthread_mutex_unlock(&queue_mutex);
            break;
        }

        // Extraer la primera tarea de la cola
        task_t *t = task_queue;
        task_queue = t->next;
        if (task_queue == NULL) {
            task_queue_tail = NULL;
        }

        pthread_mutex_unlock(&queue_mutex);

        // Procesar la tarea en la función de lógica
        process_message(t->wsi, t->msg, t->msg_len);
        free(t->msg);
        free(t);
    }
    return NULL;
}

// Hilo que monitorea la inactividad de los usuarios
static void *inactivity_monitor(void *arg) {
    (void)arg;
    while (!stop_pool) {
        sleep(5);  // Revisa cada 5 segundos
        time_t now = time(NULL);
        check_inactive_users(now);
    }
    return NULL;
}

// Inicializa el pool de hilos
void init_thread_pool(size_t num_threads) {
    thread_count = num_threads;
    threads = malloc(sizeof(pthread_t) * thread_count);
    stop_pool = false;

    for (size_t i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            log_error("No se pudo crear el hilo %zu", i);
        }
    }

    // Crear el hilo de monitoreo de inactividad
    if (pthread_create(&monitor_thread, NULL, inactivity_monitor, NULL) != 0) {
        log_error("No se pudo crear el hilo de monitoreo de inactividad");
    }

    log_info("Pool de hilos inicializado con %zu hilos", thread_count);
}

// Apaga el pool de hilos, esperando a que terminen
void shutdown_thread_pool(void) {
    pthread_mutex_lock(&queue_mutex);
    stop_pool = true;
    // Despertar a todos los hilos para que salgan
    pthread_cond_broadcast(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);

    // Unir (join) todos los hilos
    for (size_t i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    free(threads);
    threads = NULL;
    thread_count = 0;

    // Limpiar la cola de tareas, por si queda algo
    while (task_queue) {
        task_t *tmp = task_queue;
        task_queue = task_queue->next;
        free(tmp->msg);
        free(tmp);
    }
    task_queue_tail = NULL;

    log_info("Pool de hilos finalizado");
}

// Encola un mensaje para ser procesado por algún hilo del pool
void dispatch_message(struct lws *wsi, const char *msg, size_t msg_len) {
    task_t *t = malloc(sizeof(task_t));
    t->wsi = wsi;
    t->msg = malloc(msg_len);
    memcpy(t->msg, msg, msg_len);
    t->msg_len = msg_len;
    t->next = NULL;

    pthread_mutex_lock(&queue_mutex);
    if (task_queue_tail == NULL) {
        task_queue = t;
        task_queue_tail = t;
    } else {
        task_queue_tail->next = t;
        task_queue_tail = t;
    }
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
}

/**
 * Aquí se concentra la lógica que antes tenías en LWS_CALLBACK_RECEIVE de main.c:
 * - Parsear JSON con cJSON
 * - Manejar "register", "broadcast", "private", "list_users", "user_info",
 *   "change_status", "disconnect", etc.
 * - Llamar a las funciones de user_manager y connection_manager
 */
static void process_message(struct lws *wsi, const char *msg, size_t msg_len) {
    log_info("Hilo %lu procesando mensaje: %.*s",
             (unsigned long)pthread_self(), (int)msg_len, msg);

    cJSON *json = cJSON_Parse(msg);
    if (!json) {
        log_error("Error al parsear JSON en hilo %lu", (unsigned long)pthread_self());
        return;
    }

    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (!cJSON_IsString(type) || !type->valuestring) {
        cJSON_Delete(json);
        return;
    }

    cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
    if (cJSON_IsString(sender) && sender->valuestring != NULL &&
       strcmp(type->valuestring, "disconnect") != 0) {
        update_user_activity(sender->valuestring);
    }

    if (strcmp(type->valuestring, "register") == 0) {
        cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
        if (cJSON_IsString(sender) && sender->valuestring != NULL) {
            int fd = lws_get_socket_fd(wsi);
            char ip[46];
            char peer_name[256];
            lws_get_peer_addresses(wsi, fd, peer_name, sizeof(peer_name), ip, sizeof(ip));
            log_info("Conexión desde IP: %s", ip);
            bool result = register_user(sender->valuestring, ip);
            if (result) {
                log_info("Usuario %s registrado exitosamente (hilo %lu)",
                         sender->valuestring, (unsigned long)pthread_self());
                add_client(wsi, sender->valuestring);

                // Construir respuesta de "register_success"
                cJSON *response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "type", "register_success");
                cJSON_AddStringToObject(response, "sender", "server");
                cJSON_AddStringToObject(response, "content", "Registro exitoso");

                cJSON_AddItemToObject(response, "userList", get_registered_users());
                char *timestamp = get_timestamp();
                cJSON_AddStringToObject(response, "timestamp", timestamp);
                free(timestamp);

                char *response_str = cJSON_PrintUnformatted(response);
                size_t response_len = strlen(response_str);
                unsigned char buf[LWS_PRE + response_len];
                memcpy(&buf[LWS_PRE], response_str, response_len);
                lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

                cJSON_Delete(response);
                free(response_str);
            } else {
                log_error("El usuario %s ya existe (hilo %lu)",
                          sender->valuestring, (unsigned long)pthread_self());
                // Respuesta de error
                cJSON *response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "type", "error");
                cJSON_AddStringToObject(response, "sender", "server");
                cJSON_AddStringToObject(response, "content", "El usuario ya existe");

                char *timestamp = get_timestamp();
                cJSON_AddStringToObject(response, "timestamp", timestamp);
                free(timestamp);

                char *response_str = cJSON_PrintUnformatted(response);
                size_t response_len = strlen(response_str);
                unsigned char buf[LWS_PRE + response_len];
                memcpy(&buf[LWS_PRE], response_str, response_len);
                lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

                cJSON_Delete(response);
                free(response_str);
            }
        }
    }
    else if (strcmp(type->valuestring, "broadcast") == 0) {
        cJSON *content = cJSON_GetObjectItemCaseSensitive(json, "content");
        if (cJSON_IsString(content) && content->valuestring != NULL) {
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "type", "broadcast");

            cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
            if (cJSON_IsString(sender) && sender->valuestring != NULL) {
                cJSON_AddItemToObject(response, "sender", cJSON_Duplicate(sender, 1));
            }
            cJSON_AddItemToObject(response, "content", cJSON_Duplicate(content, 1));

            char *timestamp = get_timestamp();
            cJSON_AddStringToObject(response, "timestamp", timestamp);
            free(timestamp);

            char *response_str = cJSON_PrintUnformatted(response);
            size_t response_len = strlen(response_str);
            broadcast_message(response_str, response_len);

            cJSON_Delete(response);
            free(response_str);
        }
    }
    else if (strcmp(type->valuestring, "private") == 0) {
        cJSON *target = cJSON_GetObjectItemCaseSensitive(json, "target");
        cJSON *content = cJSON_GetObjectItemCaseSensitive(json, "content");
        if (cJSON_IsString(target) && target->valuestring != NULL &&
            cJSON_IsString(content) && content->valuestring != NULL) {

            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "type", "private");

            cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
            if (cJSON_IsString(sender) && sender->valuestring != NULL) {
                cJSON_AddItemToObject(response, "sender", cJSON_Duplicate(sender, 1));
            }
            cJSON_AddItemToObject(response, "content", cJSON_Duplicate(content, 1));

            char *timestamp = get_timestamp();
            cJSON_AddStringToObject(response, "timestamp", timestamp);
            free(timestamp);

            char *response_str = cJSON_PrintUnformatted(response);
            size_t response_len = strlen(response_str);
            send_private_message(target->valuestring, response_str, response_len);

            cJSON_Delete(response);
            free(response_str);
        }
    }
    else if (strcmp(type->valuestring, "list_users") == 0) {
        cJSON *response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "type", "list_users_response");
        cJSON_AddStringToObject(response, "sender", "server");
        cJSON_AddItemToObject(response, "content", get_user_list());

        cJSON_AddItemToObject(response, "content", get_registered_users());
        char *timestamp = get_timestamp();
        cJSON_AddStringToObject(response, "timestamp", timestamp);
        free(timestamp);

        char *response_str = cJSON_PrintUnformatted(response);
        size_t response_len = strlen(response_str);
        unsigned char buf[LWS_PRE + response_len];
        memcpy(&buf[LWS_PRE], response_str, response_len);
        lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

        cJSON_Delete(response);
        free(response_str);
    }
    else if (strcmp(type->valuestring, "user_info") == 0) {
        cJSON *target = cJSON_GetObjectItemCaseSensitive(json, "target");
        if (cJSON_IsString(target) && target->valuestring != NULL) {
            cJSON *info = get_user_info(target->valuestring);
            if (info) {
                cJSON *response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "type", "user_info_response");
                cJSON_AddStringToObject(response, "sender", "server");
                cJSON_AddStringToObject(response, "target", target->valuestring);
                cJSON_AddItemToObject(response, "content", info);

                char *timestamp = get_timestamp();
                cJSON_AddStringToObject(response, "timestamp", timestamp);
                free(timestamp);

                char *response_str = cJSON_PrintUnformatted(response);
                size_t response_len = strlen(response_str);
                unsigned char buf[LWS_PRE + response_len];
                memcpy(&buf[LWS_PRE], response_str, response_len);
                lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

                cJSON_Delete(response);
                free(response_str);
            } else {
                // Error: usuario no encontrado
                cJSON *response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "type", "error");
                cJSON_AddStringToObject(response, "sender", "server");
                cJSON_AddStringToObject(response, "content", "Usuario no encontrado");

                char *timestamp = get_timestamp();
                cJSON_AddStringToObject(response, "timestamp", timestamp);
                free(timestamp);

                char *response_str = cJSON_PrintUnformatted(response);
                size_t response_len = strlen(response_str);
                unsigned char buf[LWS_PRE + response_len];
                memcpy(&buf[LWS_PRE], response_str, response_len);
                lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

                cJSON_Delete(response);
                free(response_str);
            }
        }
    }
    else if (strcmp(type->valuestring, "change_status") == 0) {
        cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
        cJSON *new_status = cJSON_GetObjectItemCaseSensitive(json, "content");
        if (cJSON_IsString(sender) && sender->valuestring != NULL &&
            cJSON_IsString(new_status) && new_status->valuestring != NULL) {

            bool status_changed = change_user_status(sender->valuestring, new_status->valuestring);
            if (status_changed) {
                log_info("Estado de %s cambiado a %s (hilo %lu)",
                         sender->valuestring, new_status->valuestring, (unsigned long)pthread_self());

                cJSON *response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "type", "status_update");
                cJSON_AddStringToObject(response, "sender", "server");

                cJSON *content_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(content_obj, "user", sender->valuestring);
                cJSON_AddStringToObject(content_obj, "status", new_status->valuestring);
                cJSON_AddItemToObject(response, "content", content_obj);

                char *timestamp = get_timestamp();
                cJSON_AddStringToObject(response, "timestamp", timestamp);
                free(timestamp);

                char *response_str = cJSON_PrintUnformatted(response);
                size_t response_len = strlen(response_str);
                unsigned char buf[LWS_PRE + response_len];
                memcpy(&buf[LWS_PRE], response_str, response_len);
                lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

                cJSON_Delete(response);
                free(response_str);
            } else {
                log_error("No se pudo cambiar el estado de %s (hilo %lu)",
                          sender->valuestring, (unsigned long)pthread_self());

                cJSON *response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "type", "error");
                cJSON_AddStringToObject(response, "sender", "server");
                cJSON_AddStringToObject(response, "content", "No se pudo cambiar el estado");

                char *timestamp = get_timestamp();
                cJSON_AddStringToObject(response, "timestamp", timestamp);
                free(timestamp);

                char *response_str = cJSON_PrintUnformatted(response);
                size_t response_len = strlen(response_str);
                unsigned char buf[LWS_PRE + response_len];
                memcpy(&buf[LWS_PRE], response_str, response_len);
                lws_write(wsi, &buf[LWS_PRE], response_len, LWS_WRITE_TEXT);

                cJSON_Delete(response);
                free(response_str);
            }
        }
    }
    else if (strcmp(type->valuestring, "disconnect") == 0) {
        // Procesar desconexión controlada
        cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
        if (cJSON_IsString(sender) && sender->valuestring != NULL) {
            // Notificar a todos que este usuario se desconectó
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "type", "user_disconnected");
            cJSON_AddStringToObject(response, "sender", "server");

            char content_str[256];
            snprintf(content_str, sizeof(content_str), "%s ha salido", sender->valuestring);
            cJSON_AddStringToObject(response, "content", content_str);

            char *timestamp = get_timestamp();
            cJSON_AddStringToObject(response, "timestamp", timestamp);
            free(timestamp);

            char *response_str = cJSON_PrintUnformatted(response);
            size_t response_len = strlen(response_str);
            broadcast_message(response_str, response_len);

            cJSON_Delete(response);
            free(response_str);

            // Forzar cierre de la conexión
            lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)"Disconnect", 10);

            // Si deseas terminar inmediatamente este hilo, no es estrictamente necesario;
            // libwebsockets cerrará la conexión en su loop principal.
        }
    }

    cJSON_Delete(json);
}
