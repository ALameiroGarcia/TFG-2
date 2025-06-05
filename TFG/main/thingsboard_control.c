#include <stdio.h>
#include <string.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "oled.h"
#define LED_GPIO GPIO_NUM_2  // LED conectado al pin G2

#define TAG "MQTT_THINGSBOARD"

#define THINGSBOARD_HOST "mqtt://demo.thingsboard.io"
#define ACCESS_TOKEN     "LIJKVkaWPC5wQgn56OkM"

esp_mqtt_client_handle_t mqtt_client = NULL;

void send_data_to_thingsboard_mqtt(uint16_t values[18], int temperature) {
    cJSON *root = cJSON_CreateObject();
    char channels[] = "RSTUVWGHIJKLABCDEF";

    for (int i = 0; i < 18; i++) {
        if (values[i] > 0) {
            cJSON_AddNumberToObject(root, (char[]){channels[i], '\0'}, values[i]);
        }
    }

    if (temperature > 0) {
        cJSON_AddNumberToObject(root, "temperature", temperature);
    }

    char *json_data = cJSON_PrintUnformatted(root);
    int msg_id = esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", json_data, 0, 1, 0);
    if (msg_id >= 0){
        ESP_LOGI(TAG, "Telemetry sent: %s", json_data);
    }

    cJSON_Delete(root);
    free(json_data);
}

// Callback para mensajes entrantes (como RPC)
static void mqtt_event_handler_cb(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_DATA: {
            char topic[event->topic_len + 1];
            char data[event->data_len + 1];

            memcpy(topic, event->topic, event->topic_len);
            topic[event->topic_len] = '\0';

            memcpy(data, event->data, event->data_len);
            data[event->data_len] = '\0';

            ESP_LOGI(TAG, "Incoming message: topic=%s, data=%s", topic, data);

            if (strstr(topic, "rpc/request")) {
                // Procesar el comando RPC
                cJSON *json = cJSON_Parse(data);
                cJSON *method = cJSON_GetObjectItem(json, "method");
                cJSON *params = cJSON_GetObjectItem(json, "params");

                if (cJSON_IsString(method) && strcmp(method->valuestring, "setLed") == 0) {
                    bool led_state = false;

                    if (cJSON_IsBool(params)) {
                        led_state = cJSON_IsTrue(params);
                    } else if (cJSON_IsObject(params)) {
                        cJSON *state = cJSON_GetObjectItem(params, "state");
                        if (cJSON_IsBool(state)) {
                            led_state = cJSON_IsTrue(state);
                        }
                    }

                    ESP_LOGI(TAG, "LED command received: %s", led_state ? "ON" : "OFF");

                    // Aquí puedes encender/apagar el LED físicamente
                    gpio_set_level(LED_GPIO, !led_state);

                    // Obtener el ID de la solicitud del topic: v1/devices/me/rpc/request/<request_id>
                    char *request_id = strrchr(topic, '/');  // Apunta a "/<request_id>"
                    if (request_id != NULL) {
                        request_id++;  // Salta el '/'
                        char response_topic[100];
                        snprintf(response_topic, sizeof(response_topic), "v1/devices/me/rpc/response/%s", request_id);

                        // Publicar la respuesta
                        int ret = esp_mqtt_client_publish(mqtt_client, response_topic, "{\"success\":true}", 0, 1, 0);
                        ESP_LOGI(TAG, "Respuesta RPC publicada. Topic: %s, resultado: %d", response_topic, ret);
                    }

                }

                cJSON_Delete(json);
            }

            break;
        }
        default:
            break;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado");
            hud_display_message("MQTT ON ",7);
            // Suscribir al topic para recibir RPC
            esp_mqtt_client_subscribe(mqtt_client, "v1/devices/me/rpc/request/+", 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT desconectado");
            hud_display_message("MQTT OFF",7);
            break;
        case MQTT_EVENT_DATA:
            // Llamar al callback para procesar el mensaje
            mqtt_event_handler_cb(event);
            break;
        default:
            ESP_LOGI(TAG, "Evento MQTT: %d", event->event_id);
            break;
    }
}

void mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = THINGSBOARD_HOST,
        .credentials.username = ACCESS_TOKEN,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}