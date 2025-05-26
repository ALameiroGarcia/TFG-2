#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "web_server.h"
#include "esp_http_client.h"
#include "thingsboard_control.h"
#define AP_SSID "ESP32_AP"
#define AP_PASS "12345678"
#define MAX_STA_CONN 4

static const char *TAG = "wifi_manager";
static bool wifi_connected = false;
static esp_netif_t *sta_netif = NULL;
static esp_netif_t *ap_netif = NULL;

// Manejador de eventos WiFi
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi iniciando, intentando conectar...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi conectado.");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi desconectado. Intentando reconectar...");
                wifi_connected = false;
                esp_wifi_connect();
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Dirección IP obtenida.");
        wifi_connected = true;
    }
}

// Verifica si estamos conectados a WiFi
bool wifi_is_connected() {
    return wifi_connected;
}

// Verifica si hay conexión a Internet
bool check_internet_connection() {
    esp_http_client_config_t config = {
        .url = "http://www.google.com",
        .method = HTTP_METHOD_GET,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK) {
        ESP_LOGI("Internet Test", "Conexión a Internet exitosa ✅");
        return true;
    } else {
        ESP_LOGE("Internet Test", "Sin acceso a Internet ❌");
        return false;
    }
}

// Iniciar ESP32 en modo AP
void start_wifi_ap() {
    ESP_LOGI(TAG, "Iniciando en modo AP...");

    esp_netif_init();
    esp_event_loop_create_default();
    
    if (ap_netif == NULL) {
        ap_netif = esp_netif_create_default_wifi_ap();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "Modo AP iniciado. SSID: %s", AP_SSID);
    start_webserver();
}

// Conectarse a una red WiFi con credenciales
void connect_to_wifi(const char *ssid, const char *password) {

    ESP_LOGI(TAG, "Intentando conectar a WiFi: %s...", ssid);
    esp_netif_init();
    esp_event_loop_create_default();

    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    // Esperar 10 segundos para verificar la conexión
    vTaskDelay(pdMS_TO_TICKS(10000));

    if (wifi_is_connected()) {
        ESP_LOGI(TAG, "Conectado a %s", ssid);
        check_internet_connection();
        mqtt_app_start();
    } else {
        ESP_LOGE(TAG, "No se pudo conectar. Volviendo a modo AP...");
        start_wifi_ap();
    }
}

// Función para cargar credenciales WiFi desde NVS
bool load_wifi_credentials(char *ssid, char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t ssid_size = 32, pass_size = 64;
        err = nvs_get_str(nvs_handle, "ssid", ssid, &ssid_size);
        if (err != ESP_OK) {
            nvs_close(nvs_handle);
            return false;
        }
        err = nvs_get_str(nvs_handle, "password", password, &pass_size);
        if (err != ESP_OK) {
            nvs_close(nvs_handle);
            return false;
        }
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "Credenciales WiFi cargadas desde NVS.");
        return true;
    }
    ESP_LOGE(TAG, "No hay credenciales guardadas en NVS.");
    return false;
}

// Intentar conectar automáticamente con credenciales guardadas
void try_auto_connect() {
    char ssid[32] = {0}, password[64] = {0};

    if (load_wifi_credentials(ssid, password)) {
        connect_to_wifi(ssid, password);
    } else {
        start_wifi_ap();
    }
}
