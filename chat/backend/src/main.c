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
        {
            char *raw_msg = (char *)in;
            log_info("Mensaje recibido: %s", raw_msg);

            // Extraer la parte que corresponde al JSON
            char *start = strchr(raw_msg, '{');
            char *end = strrchr(raw_msg, '}');
            if (!start || !end || end < start) {
                log_error("Mensaje recibido no contiene un JSON válido");
                break;
            }
            size_t json_len = end - start + 1;
            char *json_str = malloc(json_len + 1);
            if (!json_str) {
                log_error("Error de memoria al asignar json_str");
                break;
            }
            strncpy(json_str, start, json_len);
            json_str[json_len] = '\0';

            log_info("Procesando JSON: %s", json_str);

            // Parsear el JSON usando cJSON
            cJSON *json = cJSON_Parse(json_str);
            free(json_str);
            if (json == NULL) {
                log_error("Error al parsear JSON");
                break;
            }

            cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
            if (cJSON_IsString(type) && type->valuestring != NULL) {
                if (strcmp(type->valuestring, "register") == 0) {
                    cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
                    if (cJSON_IsString(sender) && sender->valuestring != NULL) {
                        bool result = register_user(sender->valuestring);
                        if (result) {
                            log_info("Usuario %s registrado exitosamente", sender->valuestring);
                            add_client(wsi, sender->valuestring);
                            
                            // Preparar respuesta de registro exitoso
                            cJSON *response = cJSON_CreateObject();
                            cJSON_AddStringToObject(response, "type", "register_success");
                            cJSON_AddStringToObject(response, "sender", "server");
                            cJSON_AddStringToObject(response, "content", "Registro exitoso");
                            cJSON_AddItemToObject(response, "userList", get_user_list());
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
                            log_error("El usuario %s ya existe", sender->valuestring);
                            
                            // Preparar respuesta de error
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
                        cJSON *user_info = get_user_info(target->valuestring);
                        if (user_info != NULL) {
                            cJSON *response = cJSON_CreateObject();
                            cJSON_AddStringToObject(response, "type", "user_info_response");
                            cJSON_AddStringToObject(response, "sender", "server");
                            cJSON_AddStringToObject(response, "target", target->valuestring);
                            cJSON_AddItemToObject(response, "content", user_info);
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
                            // Usuario no encontrado
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
                            log_info("Estado de %s cambiado a %s", sender->valuestring, new_status->valuestring);
                            
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
                            log_error("No se pudo cambiar el estado de %s", sender->valuestring);
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
                    // Procesar desconexión controlada.
                    cJSON *sender = cJSON_GetObjectItemCaseSensitive(json, "sender");
                    if (cJSON_IsString(sender) && sender->valuestring != NULL) {
                        // Preparar mensaje de notificación de desconexión.
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
                        // Enviar el mensaje de desconexión a todos los clientes.
                        broadcast_message(response_str, response_len);
                        
                        cJSON_Delete(response);
                        free(response_str);
                        
                        // Establecer la razón de cierre y forzar la desconexión.
                        lws_close_reason(wsi, LWS_CLOSE_STATUS_NORMAL, (unsigned char *)"Disconnect", 10);
                        cJSON_Delete(json);
                        return -1;  // Forzar cierre de conexión.
                    }
                }
            }
            cJSON_Delete(json);
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE:
            // Aquí se pueden enviar mensajes pendientes si es necesario.
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

// Definición de los protocolos
static struct lws_protocols protocols[] = {
    {
        "chat-protocol",
        callback_chat,
        0,
        1024,
    },
    { NULL, NULL, 0, 0 }
};

int main(void)
{
    struct lws_context_creation_info info;
    struct lws_context *context;

    memset(&info, 0, sizeof(info));
    info.port = SERVER_PORT;
    info.protocols = protocols;

    context = lws_create_context(&info);
    if (context == NULL) {
        log_error("Error al iniciar libwebsockets");
        return -1;
    }
    log_info("Servidor iniciado en el puerto %d", SERVER_PORT);

    while (1) {
        lws_service(context, 50);
    }
    lws_context_destroy(context);
    return 0;
}
