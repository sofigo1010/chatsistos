#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "config.h"
#include "utils/logger.h"
#include "utils/time_utils.h"
#include "users/user_manager.h"
#include "connections/connection_manager.h"
#include "thread_manager.h"
#include <cjson/cJSON.h>  // Asegúrate de tener cJSON instalada

static int callback_chat(struct lws *wsi,
                         enum lws_callback_reasons reason,
                         void *user, void *in, size_t len)
{
    switch (reason)
    {
        case LWS_CALLBACK_ESTABLISHED:
            log_info("Nuevo cliente conectado");
            break;

        case LWS_CALLBACK_RECEIVE:
            // Encolar el mensaje para que lo procese el pool de hilos
            dispatch_message(wsi, (const char *)in, len);
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            // Envía los mensajes pendientes que se hayan encolado para este wsi
            write_pending_messages(wsi);
            break;

        case LWS_CALLBACK_CLOSED:
            log_info("Cliente desconectado");
            remove_client(wsi);
            break;

        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "chat-protocol",
        callback_chat,
        0,
        1024,
    },
    { NULL, NULL, 0, 0 }
};

int main(int argc, char *argv[])
{
    int port = SERVER_PORT; // valor por defecto definido en config.h
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) {
            fprintf(stderr, "Puerto inválido: %s\n", argv[1]);
            return -1;
        }
    } else {
        log_info("No se especificó puerto, usando puerto por defecto: %d", port);
    }

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;

    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        log_error("Error al iniciar libwebsockets");
        return -1;
    }
    log_info("Servidor iniciado en el puerto %d", port);

    // Iniciar el pool de hilos (ejemplo: 4 hilos)
    init_thread_pool(4);

    // Loop principal del servicio de libwebsockets
    while (1) {
        lws_service(context, 50);
    }

    shutdown_thread_pool();
    lws_context_destroy(context);
    return 0;
}
