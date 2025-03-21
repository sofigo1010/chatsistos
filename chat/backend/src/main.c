#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "thread_manager.h"
#include "logger.h"
#include "config.h"
#include "users/user_manager.h"
#include "connections/connection_manager.h"

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
        {
            // En vez de parsear y responder aquí, delegamos la lógica al pool de hilos
            dispatch_message(wsi, (const char *)in, len);
            break;
        }

        case LWS_CALLBACK_CLOSED:
            log_info("Cliente desconectado");
            remove_client(wsi); 
            break;

        default:
            break;
    }
    return 0;
}

// Definición de protocolos
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
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = SERVER_PORT;
    info.protocols = protocols;

    // Crear el contexto de libwebsockets
    struct lws_context *context = lws_create_context(&info);
    if (context == NULL) {
        log_error("Error al iniciar libwebsockets");
        return -1;
    }

    // Iniciar el pool de hilos con, por ejemplo, 4 hilos
    init_thread_pool(4);

    log_info("Servidor iniciado en el puerto %d", SERVER_PORT);

    // Bucle principal de libwebsockets
    while (1) {
        lws_service(context, 50);
    }

    // Al terminar, apagamos el pool y destruimos el contexto
    shutdown_thread_pool();
    lws_context_destroy(context);

    return 0;
}
