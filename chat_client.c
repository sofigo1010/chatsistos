#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <cjson/cJSON.h>

#define MAX_MSG_LEN 512

static struct lws *client_wsi = NULL;
static struct lws_context *context = NULL;
static volatile int force_exit = 0;

static char send_buffer[LWS_PRE + MAX_MSG_LEN];
static char user_name[50] = "";
static int is_registered = 0;
static volatile int waiting_for_response = 0;

static int callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len)
{
    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            printf("[CLIENT] Conexión establecida con el servidor.\n");
            client_wsi = wsi;
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE: {
            char *payload = (char *)in;
            payload[len] = '\0';
            printf("[SERVER->CLIENT] %s\n", payload);

            cJSON *response = cJSON_Parse(payload);
            if (response) {
                const cJSON *type = cJSON_GetObjectItem(response, "type");
                if (type && cJSON_IsString(type)) {
                    if (strcmp(type->valuestring, "register") == 0) {
                        is_registered = 1;
                    }
                }
                cJSON_Delete(response);
            }

            waiting_for_response = 0;
            break;
        }

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (strlen(&send_buffer[LWS_PRE]) > 0) {
                size_t msg_len = strlen(&send_buffer[LWS_PRE]);
                int n = lws_write(wsi,
                                  (unsigned char *)&send_buffer[LWS_PRE],
                                  msg_len,
                                  LWS_WRITE_TEXT);
                if (n < 0) {
                    fprintf(stderr, "[CLIENT] Error al enviar mensaje.\n");
                }
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

static const struct lws_protocols protocols[] = {
    { "chat-protocol", callback_client, 0, MAX_MSG_LEN },
    { NULL, NULL, 0, 0 }
};

static void request_write(const char *json_str) {
    size_t len_json = strlen(json_str);
    if (len_json > MAX_MSG_LEN - 1) {
        fprintf(stderr, "[CLIENT] Mensaje demasiado largo.\n");
        return;
    }
    memset(send_buffer, 0, sizeof(send_buffer));
    strncpy(&send_buffer[LWS_PRE], json_str, len_json);

    if (client_wsi)
        lws_callback_on_writable(client_wsi);
}

static const char* get_timestamp() {
    static char buffer[30];
    snprintf(buffer, sizeof(buffer), "2025-03-21T00:00:00Z");
    return buffer;
}

void show_menu() {
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

void *menu_thread(void *arg) {
    (void)arg;
    while (!force_exit) {
        if (waiting_for_response == 0) {
            show_menu();
        }

        char choice[10];
        if (!fgets(choice, sizeof(choice), stdin)) continue;
        int opt = atoi(choice);

        if (waiting_for_response != 0) continue;

        cJSON *json;
        char message[200], target[50], new_status[20];

        switch (opt) {
            case 1:
                printf("Ingresa tu nombre de usuario: ");
                fgets(user_name, sizeof(user_name), stdin);
                user_name[strcspn(user_name, "\n")] = '\0';

                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "register");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "timestamp", get_timestamp());

                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                waiting_for_response = 1;
                break;

            case 2:
                printf("Mensaje a enviar (broadcast): ");
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = '\0';

                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "broadcast");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "content", message);

                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                waiting_for_response = 1;
                break;

            case 3:
                printf("Destinatario: ");
                fgets(target, sizeof(target), stdin);
                target[strcspn(target, "\n")] = '\0';

                printf("Mensaje privado: ");
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = '\0';

                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "private");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "target", target);
                cJSON_AddStringToObject(json, "content", message);

                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                waiting_for_response = 1;
                break;

            case 4:
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "list_users");
                cJSON_AddStringToObject(json, "sender", user_name);

                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                waiting_for_response = 1;
                break;

            case 5:
                printf("Usuario a consultar: ");
                fgets(target, sizeof(target), stdin);
                target[strcspn(target, "\n")] = '\0';

                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "user_info");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "target", target);

                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                waiting_for_response = 1;
                break;

            case 6:
                printf("Nuevo estado (ACTIVO, OCUPADO, INACTIVO): ");
                fgets(new_status, sizeof(new_status), stdin);
                new_status[strcspn(new_status, "\n")] = '\0';

                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "change_status");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "content", new_status);

                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                waiting_for_response = 1;
                break;

            case 7:
                force_exit = 1;
                break;

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

    struct lws_context_creation_info info = {0};
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Error al crear el contexto.\n");
        return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, menu_thread, NULL);

    while (!force_exit) lws_service(context, 0);

    lws_context_destroy(context);
    pthread_join(tid, NULL);

    return 0;
}
