#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

#include <libwebsockets.h>
#include <stddef.h>

/**
 * Inicializa el pool de hilos con num_threads hilos.
 */
void init_thread_pool(size_t num_threads);

/**
 * Apaga el pool de hilos, esperando a que terminen las tareas en cola.
 */
void shutdown_thread_pool(void);

/**
 * Encola un mensaje (recibido por libwebsockets) para que sea procesado
 * en uno de los hilos del pool.
 */
void dispatch_message(struct lws *wsi, const char *msg, size_t msg_len);

#endif
