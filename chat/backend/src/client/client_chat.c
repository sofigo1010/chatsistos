#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <cjson/cJSON.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_MSG_LEN 512

static struct lws *client_wsi;
static struct lws_context *context;
static volatile int force_exit;
static char send_buffer[LWS_PRE + MAX_MSG_LEN];
static char user_name[50];

static pthread_mutex_t resp_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t resp_cond = PTHREAD_COND_INITIALIZER;
static int response_ready;


void show_menu(void) {
    printf("\n--- MENÚ DE OPCIONES ---\n");
    printf("1. Chat Broadcast\n");
    printf("2. Chat Mensaje Privado\n");
    printf("3. Listado de Usuarios\n");
    printf("4. Información de Usuario\n");
    printf("5. Cambio de Estado\n");
    printf("6. Desconectar\n");
    printf("Opción: ");
    fflush(stdout);
}

--------------------- */
void wait_for_response() {
    pthread_mutex_lock(&resp_mutex);
    while (!response_ready && !force_exit) {
        pthread_cond_wait(&resp_cond, &resp_mutex);
    }
    response_ready = 0;
    pthread_mutex_unlock(&resp_mutex);
}

void process_server_response(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        printf("[SERVER] Error al parsear la respuesta JSON.\n");
        return;
    }
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (!type) {
        printf("[SERVER] Respuesta sin tipo definido.\n");
        cJSON_Delete(json);
        return;
    }


    if (strcmp(type->valuestring, "broadcast") == 0 ||
        strcmp(type->valuestring, "private") == 0) {

        cJSON *sender = cJSON_GetObjectItem(json, "sender");
        cJSON *content = cJSON_GetObjectItem(json, "content");
        cJSON *timestamp = cJSON_GetObjectItem(json, "timestamp");

        if (sender && content && timestamp) {
            printf("\n%s: %s\n%s\n",
                   sender->valuestring,
                   content->valuestring,
                   timestamp->valuestring);
        } else {
            printf("[SERVER] Mensaje de chat con campos faltantes.\n");
        }
    }
    else if (strcmp(type->valuestring, "register_success") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        cJSON *userList = cJSON_GetObjectItem(json, "userList");
        printf("\n[SERVER] %s\n", content->valuestring);
        if (userList && cJSON_IsArray(userList)) {
            printf("Usuarios en línea: ");
            int size = cJSON_GetArraySize(userList);
            for (int i = 0; i < size; i++) {
                cJSON *user = cJSON_GetArrayItem(userList, i);
                printf("%s ", user->valuestring);
            }
            printf("\n");
        }
    }
    else if (strcmp(type->valuestring, "list_users_response") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (content && cJSON_IsArray(content)) {
            printf("\n[SERVER] Listado de usuarios conectados:\n");
            int size = cJSON_GetArraySize(content);
            for (int i = 0; i < size; i++) {
                cJSON *user = cJSON_GetArrayItem(content, i);
                printf(" - %s\n", user->valuestring);
            }
        }
    }
    else if (strcmp(type->valuestring, "user_info_response") == 0) {
        cJSON *target = cJSON_GetObjectItem(json, "target");
        cJSON *content = cJSON_GetObjectItem(json, "content");
        printf("\n[SERVER] Información de %s:\n", target->valuestring);
        if (content) {
            cJSON *ip = cJSON_GetObjectItem(content, "ip");
            cJSON *status = cJSON_GetObjectItem(content, "status");
            printf("   IP: %s\n", ip->valuestring);
            printf("   Estado: %s\n", status->valuestring);
        }
    }
    else if (strcmp(type->valuestring, "status_update") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (content) {
            cJSON *user = cJSON_GetObjectItem(content, "user");
            cJSON *status = cJSON_GetObjectItem(content, "status");
            printf("\n[SERVER] %s cambió su estado a %s.\n", user->valuestring, status->valuestring);
        }
    }
    else if (strcmp(type->valuestring, "user_disconnected") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        printf("\n[SERVER] %s\n", content->valuestring);
    }
    else if (strcmp(type->valuestring, "error") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        printf("\n[SERVER] ERROR: %s\n", content->valuestring);
    }
    else {
        printf("\n[SERVER] %s\n", json_str);
    }
    cJSON_Delete(json);
}


void request_write(const char *json_str) {
    size_t len = strlen(json_str);
    if (len >= MAX_MSG_LEN) {
        fprintf(stderr, "[CLIENT] Mensaje demasiado largo.\n");
        return;
    }
    memset(send_buffer, 0, sizeof(send_buffer));
    memcpy(&send_buffer[LWS_PRE], json_str, len);
    lws_callback_on_writable(client_wsi);
}


void chat_session(int mode, const char *target) {
    printf("\n=== MODO CHAT %s ===\n", mode == 1 ? "BROADCAST" : "MENSAJE PRIVADO");
    printf("Escribe tus mensajes y presiona Enter para enviarlos.\n");
    printf("Escribe '/salir' para volver al menú.\n");
    char buf[256];
    while (1) {
        printf("chat> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin))
            break;
        char *p = strchr(buf, '\n');
        if (p) *p = '\0';

        if (strcmp(buf, "/salir") == 0)
            break;

        if (strlen(buf) == 0)
            continue;

        cJSON *json = cJSON_CreateObject();
        if (mode == 1) { // broadcast
            cJSON_AddStringToObject(json, "type", "broadcast");
            cJSON_AddStringToObject(json, "sender", user_name);
            cJSON_AddStringToObject(json, "content", buf);
        } else if (mode == 2) { // privado
            cJSON_AddStringToObject(json, "type", "private");
            cJSON_AddStringToObject(json, "sender", user_name);
            cJSON_AddStringToObject(json, "target", target);
            cJSON_AddStringToObject(json, "content", buf);
        }
        char *msg = cJSON_PrintUnformatted(json);
        request_write(msg);
        free(msg);
        cJSON_Delete(json);

    }
    printf("Saliendo del modo chat...\n");
}


static int callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len) {
    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED: {
        int flag = 1;
        setsockopt(lws_get_socket_fd(wsi), IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        printf("\n[CLIENT] Conexión establecida.\n");
        cJSON *reg = cJSON_CreateObject();
        cJSON_AddStringToObject(reg, "type", "register");
        cJSON_AddStringToObject(reg, "sender", user_name);
        cJSON_AddNullToObject(reg, "content");
        request_write(cJSON_PrintUnformatted(reg));
        cJSON_Delete(reg);
        break;
    }
    case LWS_CALLBACK_CLIENT_RECEIVE: {
        ((char*)in)[len] = '\0';
        process_server_response((char*)in);

        pthread_mutex_lock(&resp_mutex);
        response_ready = 1;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
        break;
    }
    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        size_t msg_len = strlen(&send_buffer[LWS_PRE]);
        if (msg_len) {
            lws_write(wsi, (unsigned char*)&send_buffer[LWS_PRE], msg_len, LWS_WRITE_TEXT);
            memset(&send_buffer[LWS_PRE], 0, MAX_MSG_LEN);
        }
        break;
    }
    case LWS_CALLBACK_CLOSED:
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


void *menu_thread(void *_) {
    char choice[10], buf[256];

    while (!force_exit) {
        show_menu();
        if (!fgets(choice, sizeof(choice), stdin))
            continue;
        int opt = atoi(choice);

        cJSON *json = NULL;
        switch(opt) {
            case 1: // Chat Broadcast
                chat_session(1, NULL);
                wait_for_response();
                break;

            case 2: { // Chat Mensaje Privado
                printf("Destinatario: ");
                if (!fgets(buf, sizeof(buf), stdin))
                    continue;
                char *target = strtok(buf, "\n");
                if (!target) continue;
                /* Sesión de chat privado */
                chat_session(2, target);
                wait_for_response();
                break;
            }

            case 3: { // Listado de Usuarios
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "list_users");
                cJSON_AddStringToObject(json, "sender", user_name);
                break;
            }
            case 4: { // Información de Usuario
                printf("Usuario info: ");
                if (!fgets(buf, sizeof(buf), stdin))
                    continue;
                char *target = strtok(buf, "\n");
                if (!target) continue;
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "user_info");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "target", target);
                break;
            }
            case 5: { // Cambio de Estado
                printf("Nuevo estado: ");
                if (!fgets(buf, sizeof(buf), stdin))
                    continue;
                char *p = strchr(buf, '\n');
                if (p) *p = '\0';

                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "change_status");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "content", buf);
                break;
            }
            case 6: { // Desconectar
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type", "disconnect");
                cJSON_AddStringToObject(json, "sender", user_name);
                /* Enviar y salir */
                request_write(cJSON_PrintUnformatted(json));
                cJSON_Delete(json);
                force_exit = 1;
                break;
            }
            default:
                printf("[CLIENT] Opción inválida.\n");
                break;
        }

        /* 3) Si se generó un JSON (excepto en chat_session), enviarlo y esperar la respuesta */
        if (json) {
            char *json_str = cJSON_PrintUnformatted(json);
            request_write(json_str);
            free(json_str);
            cJSON_Delete(json);
            if (!force_exit) {
                wait_for_response();
            }
        }
    }

    return NULL;
}


void *service_thread(void *_) {
    while (!force_exit) {
        lws_service(context, 0);
        usleep(1000);
    }
    return NULL;
}


int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <nombre_del_cliente> <nombre_de_usuario> <IP_del_servidor> <puerto_del_servidor>\n", argv[0]);
        return 1;
    }

    strcpy(user_name, argv[2]);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "Error creando contexto\n");
        return 1;
    }

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context  = context;
    ccinfo.address  = argv[3];
    ccinfo.port     = atoi(argv[4]);
    ccinfo.path     = "/chat";
    ccinfo.host     = argv[3];
    ccinfo.origin   = argv[3];
    ccinfo.protocol = protocols[0].name;

    client_wsi = lws_client_connect_via_info(&ccinfo);
    if (!client_wsi) {
        fprintf(stderr, "[CLIENT] No se pudo conectar a %s:%s%s\n", argv[3], argv[4], ccinfo.path);
        return 1;
    }

    pthread_t t1, t2;
    pthread_create(&t1, NULL, menu_thread, NULL);
    pthread_create(&t2, NULL, service_thread, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    lws_context_destroy(context);
    return 0;
}
