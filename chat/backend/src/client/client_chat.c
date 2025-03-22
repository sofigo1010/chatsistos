#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <cjson/cJSON.h>

// Tamaño máximo de mensaje a enviar
#define MAX_MSG_LEN 512

// Variables globales para WebSocket
static struct lws *client_wsi = NULL;
static struct lws_context *context = NULL;
static volatile int force_exit = 0;

// Buffer para almacenar el mensaje a enviar
static char send_buffer[LWS_PRE + MAX_MSG_LEN];

// Nombre de usuario (una vez registrado)
static char user_name[50] = "";

// Indica si ya se realizó el registro
static int is_registered = 0;

// Callback del cliente
static int callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("[CLIENT] Conexión establecida con el servidor.\n");
            client_wsi = wsi;
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            // Mensaje recibido
            char *payload = (char *)in;
            payload[len] = '\0';
            printf("[SERVER->CLIENT] %s\n", payload);

            break;
        }

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            // Si hay algo en send_buffer, enviarlo
            if (strlen(&send_buffer[LWS_PRE]) > 0) {
                size_t msg_len = strlen(&send_buffer[LWS_PRE]);
                int n = lws_write(wsi,
                                  (unsigned char *)&send_buffer[LWS_PRE],
                                  msg_len,
                                  LWS_WRITE_TEXT);
                if (n < 0) {
                    fprintf(stderr, "[CLIENT] Error al enviar mensaje.\n");
                }
                // Limpiar buffer
                memset(&send_buffer[LWS_PRE], 0, MAX_MSG_LEN);
            }
            break;

        case LWS_CALLBACK_CLOSED:
            printf("[CLIENT] Conexión cerrada por el servidor.\n");
            client_wsi = NULL;
            force_exit = 1;
            break;

        default:
            break;
    }
    return 0;
}

// Protocolos
static const struct lws_protocols protocols[] = {
    {
        "chat-protocol",
        callback_client,
        0,
        MAX_MSG_LEN,
    },
    { NULL, NULL, 0, 0 }
};

// Función para solicitar al WebSocket que se escriba
static void request_write(const char *json_str) {
    // Copiar el mensaje JSON a nuestro buffer (después de LWS_PRE)
    size_t len_json = strlen(json_str);
    if (len_json > MAX_MSG_LEN - 1) {
        fprintf(stderr, "[CLIENT] Mensaje demasiado largo.\n");
        return;
    }
    memset(send_buffer, 0, sizeof(send_buffer));
    strncpy(&send_buffer[LWS_PRE], json_str, len_json);

    // Pedir al contexto que el wsi sea WRITABLE
    if (client_wsi)
        lws_callback_on_writable(client_wsi);
}

// Función para crear un JSON con el timestamp (opcional)
static const char* get_timestamp() {
    static char buffer[30];
    // Para simplificar, un timestamp fijo o real
    snprintf(buffer, sizeof(buffer), "2025-03-21T00:00:00Z");
    return buffer;
}

// Función para mostrar el menú
static void show_menu() {
    printf("\n--- MENÚ DE OPCIONES ---\n");
    printf("1) Registrar usuario\n");
    printf("2) Enviar broadcast\n");
    printf("3) Enviar mensaje privado\n");
    printf("4) Listar usuarios\n");
    printf("5) Obtener info de usuario\n");
    printf("6) Cambiar estado\n");
    printf("7) Desconectarse\n");
    printf("8) Salir del programa\n");
    printf("Seleccione una opción: ");
}

// hiilo para el menu
void *menu_thread(void *arg) {
    (void)arg;
    while (!force_exit) {
        show_menu();
        char choice[10];
        if (!fgets(choice, sizeof(choice), stdin)) {
            continue;
        }
        int opt = atoi(choice);

        switch (opt) {
            case 1: {
                if (is_registered) {
                    printf("[CLIENT] Ya estás registrado.\n");
                    break;
                }
                printf("Ingresa tu nombre de usuario: ");
                char name[50];
                if (fgets(name, sizeof(name), stdin)) {
                    name[strcspn(name, "\n")] = '\0';
                    strncpy(user_name, name, sizeof(user_name) - 1);

                    // registro
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "type", "register");
                    cJSON_AddStringToObject(json, "sender", user_name);
                    cJSON_AddNullToObject(json, "content");
                    cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                    char *json_str = cJSON_PrintUnformatted(json);
                    request_write(json_str);
                    cJSON_Delete(json);
                    free(json_str);

                    // está registrado es true
                    is_registered = 1;
                }
                break;
            }

            case 2: {
                if (!is_registered) {
                    printf("[CLIENT] Debes registrarte primero.\n");
                    break;
                }
                printf("Mensaje a enviar (broadcast): ");
                char message[200];
                if (fgets(message, sizeof(message), stdin)) {
                    message[strcspn(message, "\n")] = '\0';
                    cJSON *json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "type", "broadcast");
                    cJSON_AddStringToObject(json, "sender", user_name);
                    cJSON_AddStringToObject(json, "content", message);
                    cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                    char *json_str = cJSON_PrintUnformatted(json);
                    request_write(json_str);
                    cJSON_Delete(json);
                    free(json_str);
                }
                break;
            }

            case 3: {
                if (!is_registered) {
                    printf("[CLIENT] Debes registrarte primero.\n");
                    break;
                }
                printf("Destinatario: ");
                char target[50];
                if (!fgets(target, sizeof(target), stdin)) break;
                target[strcspn(target, "\n")] = '\0';

                printf("Mensaje privado: ");
                char pm[200];
                if (!fgets(pm, sizeof(pm), stdin)) break;
                pm[strcspn(pm, "\n")] = '\0';

                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "private");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "target", target);
                cJSON_AddStringToObject(json, "content", pm);
                cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                char *json_str = cJSON_PrintUnformatted(json);
                request_write(json_str);
                cJSON_Delete(json);
                free(json_str);
                break;
            }

            case 4: {
                // ver listado de usuarios
                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "list_users");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                char *json_str = cJSON_PrintUnformatted(json);
                request_write(json_str);
                cJSON_Delete(json);
                free(json_str);
                break;
            }

            case 5: {
                // info de usuario
                printf("Usuario a consultar: ");
                char target[50];
                if (!fgets(target, sizeof(target), stdin)) break;
                target[strcspn(target, "\n")] = '\0';

                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "user_info");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "target", target);
                cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                char *json_str = cJSON_PrintUnformatted(json);
                request_write(json_str);
                cJSON_Delete(json);
                free(json_str);
                break;
            }

            case 6: {
                // cambiar estado
                printf("Nuevo estado (ACTIVO, OCUPADO, INACTIVO): ");
                char new_status[20];
                if (!fgets(new_status, sizeof(new_status), stdin)) break;
                new_status[strcspn(new_status, "\n")] = '\0';

                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "change_status");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "content", new_status);
                cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                char *json_str = cJSON_PrintUnformatted(json);
                request_write(json_str);
                cJSON_Delete(json);
                free(json_str);
                break;
            }

            case 7: {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "disconnect");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "content", "Cierre de sesión");
                cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                char *json_str = cJSON_PrintUnformatted(json);
                request_write(json_str);
                cJSON_Delete(json);
                free(json_str);
                // Esperar a que el server cierre la conexión
                break;
            }

            case 8: {
                force_exit = 1;
                break;
            }

            default:
                printf("[CLIENT] Opción inválida.\n");
                break;
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <IP-del-servidor> <puerto>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    // Crear contexto
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;  
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Error al crear el contexto.\n");
        return 1;
    }

    // Crear hilo para el menú interactivo
    pthread_t tid;
    pthread_create(&tid, NULL, menu_thread, NULL);

    // conexion con le cliente
    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = context;
    ccinfo.address = server_ip;
    ccinfo.port = server_port;
    ccinfo.path = "/chat";
    ccinfo.host = lws_canonical_hostname(context);
    ccinfo.origin = "origin";
    ccinfo.protocol = "chat-protocol"; 
    ccinfo.ssl_connection = 0;         

    if (!lws_client_connect_via_info(&ccinfo)) {
        fprintf(stderr, "Error al iniciar la conexión del cliente.\n");
        lws_context_destroy(context);
        return 1;
    }

    printf("[CLIENT] Conectando a ws://%s:%d/chat\n", server_ip, server_port);
    while (!force_exit) {
        lws_service(context, 50);
    }
    lws_context_destroy(context);
    pthread_join(tid, NULL);

    printf("[CLIENT] Finalizado.\n");
    return 0;
}
