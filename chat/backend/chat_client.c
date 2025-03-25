#define _DEFAULT_SOURCE   // Para asegurarnos de que TCP_NODELAY esté declarado
#include <libwebsockets.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <cjson/cJSON.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_MSG_LEN 512

static struct lws *client_wsi = NULL;
static struct lws_context *context = NULL;
static volatile int force_exit = 0;
static char send_buffer[LWS_PRE + MAX_MSG_LEN];
static char user_name[50];

/* --- Sincronización para esperar respuestas tipo "list_users", etc. --- */
static pthread_mutex_t resp_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t resp_cond = PTHREAD_COND_INITIALIZER;
static int response_ready = 0;

/* --------------------------------------------------------------------- *
 *  Función para obtener el timestamp actual (con microsegundos)        *
 * --------------------------------------------------------------------- */
void get_current_timestamp(char *buffer, size_t buflen) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    strftime(buffer, buflen, "%Y-%m-%d %H:%M:%S", tm_info);
    size_t len = strlen(buffer);
    snprintf(buffer + len, buflen - len, ".%06ld", (long)tv.tv_usec);
}

/* --------------------------------------------------------------------- *
 *  Impresión de menú                                                   *
 * --------------------------------------------------------------------- */
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

/* --------------------------------------------------------------------- *
 *  Esperar a que llegue (por callback) una respuesta del servidor      *
 * --------------------------------------------------------------------- */
void wait_for_response() {
    pthread_mutex_lock(&resp_mutex);
    while (!response_ready) {
        pthread_cond_wait(&resp_cond, &resp_mutex);
    }
    response_ready = 0;
    pthread_mutex_unlock(&resp_mutex);
}

/* --------------------------------------------------------------------- *
 *  Procesar las respuestas (mensajes) que llegan del servidor          *
 * --------------------------------------------------------------------- */
void process_server_response(const char *json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        printf("[SERVER] Error al parsear la respuesta JSON.\n");
        fflush(stdout);
        return;
    }

    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (!type || !cJSON_IsString(type)) {
        printf("[SERVER] Respuesta sin tipo definido.\n");
        fflush(stdout);
        cJSON_Delete(json);
        return;
    }

    /* Broadcast o privado */
    if (strcmp(type->valuestring, "broadcast") == 0 ||
        strcmp(type->valuestring, "private") == 0) {

        cJSON *sender    = cJSON_GetObjectItem(json, "sender");
        cJSON *content   = cJSON_GetObjectItem(json, "content");
        cJSON *timestamp = cJSON_GetObjectItem(json, "timestamp");

        if (sender && content && timestamp &&
            cJSON_IsString(sender) && cJSON_IsString(content) && cJSON_IsString(timestamp)) {
            printf("\n%s: %s\n%s\n",
                   sender->valuestring,
                   content->valuestring,
                   timestamp->valuestring);
        } else {
            printf("[SERVER] Mensaje de chat con campos faltantes.\n");
        }
    }
    else if (strcmp(type->valuestring, "register_success") == 0) {
        cJSON *content  = cJSON_GetObjectItem(json, "content");
        cJSON *userList = cJSON_GetObjectItem(json, "userList");
        if (content && cJSON_IsString(content)) {
            printf("\n[SERVER] %s\n", content->valuestring);
        }
        if (userList && cJSON_IsArray(userList)) {
            printf("Usuarios en línea: ");
            int size = cJSON_GetArraySize(userList);
            for (int i = 0; i < size; i++) {
                cJSON *user = cJSON_GetArrayItem(userList, i);
                if (cJSON_IsString(user)) {
                    printf("%s ", user->valuestring);
                }
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
                if (cJSON_IsString(user)) {
                    printf(" - %s\n", user->valuestring);
                }
            }
        }
    }
    else if (strcmp(type->valuestring, "user_info_response") == 0) {
        cJSON *target  = cJSON_GetObjectItem(json, "target");
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (target && cJSON_IsString(target)) {
            printf("\n[SERVER] Información de %s:\n", target->valuestring);
        }
        if (content) {
            cJSON *ip     = cJSON_GetObjectItem(content, "ip");
            cJSON *status = cJSON_GetObjectItem(content, "status");
            if (ip && cJSON_IsString(ip)) {
                printf("   IP: %s\n", ip->valuestring);
            }
            if (status && cJSON_IsString(status)) {
                printf("   Estado: %s\n", status->valuestring);
            }
        }
    }
    else if (strcmp(type->valuestring, "status_update") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (content) {
            cJSON *user   = cJSON_GetObjectItem(content, "user");
            cJSON *status = cJSON_GetObjectItem(content, "status");
            if (user && status && cJSON_IsString(user) && cJSON_IsString(status)) {
                printf("\n[SERVER] %s cambió su estado a %s.\n", user->valuestring, status->valuestring);
            }
        }
    }
    else if (strcmp(type->valuestring, "user_disconnected") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (content && cJSON_IsString(content)) {
            printf("\n[SERVER] %s\n", content->valuestring);
        }
    }
    else if (strcmp(type->valuestring, "error") == 0) {
        cJSON *content = cJSON_GetObjectItem(json, "content");
        if (content && cJSON_IsString(content)) {
            printf("\n[SERVER] ERROR: %s\n", content->valuestring);
        }
    }
    else {
        /* En caso de que llegue algo desconocido */
        printf("\n[SERVER] %s\n", json_str);
    }

    fflush(stdout);
    cJSON_Delete(json);
}

/* --------------------------------------------------------------------- *
 *  Función para encolar (o disparar) la escritura de un mensaje al ws   *
 * --------------------------------------------------------------------- */
void request_write(const char *json_str) {
    size_t len = strlen(json_str);
    if (len >= MAX_MSG_LEN) {
        fprintf(stderr, "[CLIENT] Mensaje demasiado largo.\n");
        return;
    }

    char timebuf[64];
    get_current_timestamp(timebuf, sizeof(timebuf));
    printf("[DEBUG] Sending message at %s: %s\n", timebuf, json_str);
    fflush(stdout);

    /* Copiamos el mensaje en el buffer send_buffer */
    memset(send_buffer, 0, sizeof(send_buffer));
    memcpy(&send_buffer[LWS_PRE], json_str, len);

    /* Pedimos a LWS que nos llame en CLIENT_WRITEABLE */
    lws_callback_on_writable(client_wsi);
}

/* --------------------------------------------------------------------- *
 *  Callbacks para libwebsockets en el lado cliente                      *
 * --------------------------------------------------------------------- */
static int callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len)
{
    char timebuf[64];
    switch (reason) {

    case LWS_CALLBACK_CLIENT_ESTABLISHED: {
        /* Desactiva Nagle en el socket */
        int fd   = lws_get_socket_fd(wsi);
        int flag = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        get_current_timestamp(timebuf, sizeof(timebuf));
        printf("[DEBUG] Connection established at %s.\n", timebuf);
        fflush(stdout);

        /* Al conectar, mandamos automáticamente el "register" */
        cJSON *reg = cJSON_CreateObject();
        cJSON_AddStringToObject(reg, "type",   "register");
        cJSON_AddStringToObject(reg, "sender", user_name);
        cJSON_AddNullToObject(reg,   "content");
        char *reg_str = cJSON_PrintUnformatted(reg);
        request_write(reg_str);
        free(reg_str);
        cJSON_Delete(reg);
        break;
    }

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        /* Asegurarnos de terminar el string con \0 */
        ((char *)in)[len] = '\0';

        get_current_timestamp(timebuf, sizeof(timebuf));
        printf("[DEBUG] Received message at %s: %s\n", timebuf, (char*)in);
        fflush(stdout);

        process_server_response((char*)in);

        /* Avisar que llegó la respuesta (usado por wait_for_response) */
        pthread_mutex_lock(&resp_mutex);
        response_ready = 1;
        pthread_cond_signal(&resp_cond);
        pthread_mutex_unlock(&resp_mutex);
        break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        /* Enviamos lo que haya en send_buffer */
        size_t msg_len = strlen(&send_buffer[LWS_PRE]);
        if (msg_len > 0) {
            int n = lws_write(wsi, (unsigned char*)&send_buffer[LWS_PRE], msg_len, LWS_WRITE_TEXT);

            get_current_timestamp(timebuf, sizeof(timebuf));
            printf("[DEBUG] Message sent at %s.\n", timebuf);
            fflush(stdout);

            /* Limpieza del buffer para la próxima vez */
            memset(&send_buffer[LWS_PRE], 0, MAX_MSG_LEN);

            if (n < (int)msg_len) {
                fprintf(stderr, "[CLIENT] lws_write devolvió %d (esperaba %zu)\n", n, msg_len);
            }
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

/* --------------------------------------------------------------------- *
 *  Protocolos usados en el contexto de LWS                              *
 * --------------------------------------------------------------------- */
static const struct lws_protocols protocols[] = {
    {
        "chat-protocol",
        callback_client,
        0,            // Tamaño de la parte "user" por conexión
        MAX_MSG_LEN,  // Tamaño máx de buffer
    },
    { NULL, NULL, 0, 0 }
};

/* --------------------------------------------------------------------- *
 *  Modo chat interactivo (BROADCAST o PRIVADO)                          *
 * --------------------------------------------------------------------- */
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

        /* Quitar salto de línea final */
        char *p = strchr(buf, '\n');
        if (p) *p = '\0';

        /* Comando para salir del modo chat */
        if (strcmp(buf, "/salir") == 0) {
            break;
        }

        if (strlen(buf) == 0) {
            continue;
        }

        /* Construir JSON y enviar */
        cJSON *json = cJSON_CreateObject();
        if (mode == 1) {
            /* broadcast */
            cJSON_AddStringToObject(json, "type",   "broadcast");
            cJSON_AddStringToObject(json, "sender", user_name);
            cJSON_AddStringToObject(json, "content", buf);
        } else if (mode == 2) {
            /* private */
            cJSON_AddStringToObject(json, "type",   "private");
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
    fflush(stdout);
}

/* --------------------------------------------------------------------- *
 *  Hilo que maneja el input del usuario (menú interactivo)              *
 * --------------------------------------------------------------------- */
void *menu_thread(void *arg) {
    (void)arg;
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
                break;

            case 2: { // Chat Mensaje Privado
                printf("Destinatario: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin))
                    continue;
                char *target = strtok(buf, "\n");
                chat_session(2, target);
                break;
            }

            case 3: // Listado de Usuarios
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type",   "list_users");
                cJSON_AddStringToObject(json, "sender", user_name);
                break;

            case 4: // Información de Usuario
                printf("Usuario info: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin))
                    continue;
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type",   "user_info");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "target", strtok(buf, "\n"));
                break;

            case 5: // Cambio de Estado
                printf("Nuevo estado: ");
                fflush(stdout);
                if (!fgets(buf, sizeof(buf), stdin))
                    continue;
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type",   "change_status");
                cJSON_AddStringToObject(json, "sender", user_name);
                cJSON_AddStringToObject(json, "content", strtok(buf, "\n"));
                break;

            case 6: // Desconectar
                json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "type",   "disconnect");
                cJSON_AddStringToObject(json, "sender", user_name);

                /* Enviar de inmediato y forzar la salida */
                {
                    char *tmp = cJSON_PrintUnformatted(json);
                    request_write(tmp);
                    free(tmp);
                }
                cJSON_Delete(json);
                force_exit = 1;
                return NULL; // Sale del hilo de menú

            default:
                printf("[CLIENT] Opción inválida.\n");
                fflush(stdout);
                break;
        }

        /* Si se construyó un JSON distinto de broadcast o private, enviamos y esperamos respuesta */
        if (json) {
            char *tmp = cJSON_PrintUnformatted(json);
            request_write(tmp);
            free(tmp);
            cJSON_Delete(json);

            /* Esperamos la respuesta del servidor para (list_users, user_info, change_status...) */
            if (opt != 1 && opt != 2) {
                wait_for_response();
            }
        }
    }
    return NULL;
}

/* --------------------------------------------------------------------- *
 *  Hilo que maneja el servicio de libwebsockets (polling)              *
 * --------------------------------------------------------------------- */
void *service_thread(void *arg) {
    (void)arg;
    while (!force_exit) {
        /* lws_service con timeout 0 => no se queda bloqueado */
        lws_service(context, 0);
        usleep(1000);  // Pequeña espera (1 ms) para no consumir CPU al 100%
    }
    return NULL;
}

/* --------------------------------------------------------------------- *
 *  MAIN                                                                 *
 * --------------------------------------------------------------------- */
int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <nombre_del_cliente> <nombre_de_usuario> <IP_del_servidor> <puerto_del_servidor>\n",
                argv[0]);
        return 1;
    }

    /* Guardar nombre de usuario para el "register" automático */
    strncpy(user_name, argv[2], sizeof(user_name) - 1);
    user_name[sizeof(user_name) - 1] = '\0';

    /* Crear el contexto de LWS en modo CLIENTE */
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;  // Modo cliente
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "[CLIENT] Error creando contexto\n");
        return 1;
    }

    /* Datos de conexión al servidor */
    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));

    ccinfo.context = context;
    ccinfo.address = argv[3];
    ccinfo.port    = atoi(argv[4]);
    ccinfo.path    = "/chat";                 // Ruta del websocket
    ccinfo.host    = ccinfo.address;
    ccinfo.origin  = ccinfo.address;
    ccinfo.protocol = protocols[0].name;

    /* Intentar conectar */
    client_wsi = lws_client_connect_via_info(&ccinfo);
    if (!client_wsi) {
        fprintf(stderr, "[CLIENT] No se pudo conectar a %s:%s%s\n",
                argv[3], argv[4], ccinfo.path);
        lws_context_destroy(context);
        return 1;
    }

    /* Crear hilos: uno para el menú interactivo y otro para el lws_service */
    pthread_t t_menu, t_service;
    pthread_create(&t_menu, NULL, menu_thread, NULL);
    pthread_create(&t_service, NULL, service_thread, NULL);

    /* Esperar a que finalicen */
    pthread_join(t_menu, NULL);
    pthread_join(t_service, NULL);

    /* Destruir el contexto LWS */
    lws_context_destroy(context);

    return 0;
}
