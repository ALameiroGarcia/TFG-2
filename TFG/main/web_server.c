#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "wifi_ap.h"
#include "driver/gpio.h"

static const char *TAG = "web_server";
httpd_handle_t server = NULL;
#define LED_GPIO GPIO_NUM_2  // LED conectado al pin G35

// Página principal con formulario y botón para LED
esp_err_t get_handler(httpd_req_t *req) {
    const char *html_page = "<!DOCTYPE html>"
                            "<html lang='es'>"
                            "<head>"
                            "<meta charset='UTF-8'>"
                            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                            "<title>Configurar WiFi y Controlar LED</title>"
                            "<style>"
                            "body { font-family: Arial, sans-serif; text-align: center; padding: 20px; }"
                            ".container { max-width: 350px; margin: auto; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); }"
                            "input, select { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 5px; }"
                            "button { background: #28a745; color: white; padding: 10px; border: none; border-radius: 5px; cursor: pointer; }"
                            "button:hover { background: #218838; }"
                            ".toggle-pass { cursor: pointer; position: absolute; right: 10px; top: 12px; }"
                            ".password-wrapper { position: relative; display: flex; align-items: center; }"
                            "#loading { display: none; margin-top: 10px; font-size: 14px; color: #007bff; }"
                            "</style>"
                            "</head>"
                            "<body>"
                            "<div class='container'>"
                            "<h2>Ingrese los datos de WiFi</h2>"
                            "<form id='wifiForm'>"
                            "<label>SSID:</label>"
                            "<input type='text' id='ssid' name='ssid' placeholder='Escribe el nombre de la red WiFi'><br>"
                            "<label>Contraseña:</label>"
                            "<div class='password-wrapper'>"
                            "<input type='password' id='password' name='password' placeholder='Escribe la contraseña'><span class='toggle-pass' onclick='togglePassword()'>👁️</span>"
                            "</div>"
                            "<br>"
                            "<button type='submit'>Conectar</button>"
                            "<p id='loading'>Conectando...</p>"
                            "</form>"
                            "<br>"
                            "<h3>Control del LED</h3>"
                            "<button id='ledToggleButton' onclick='toggleLED()'>Encender LED</button>"
                            "</div>"

                            "<script>"
                            "function togglePassword() {"
                            "  var passInput = document.getElementById('password');"
                            "  passInput.type = passInput.type === 'password' ? 'text' : 'password';"
                            "} "
                            "function toggleLED() {"
                            "  fetch('/led_toggle')"
                            "    .then(response => response.text())"
                            "    .then(data => {"
                            "      alert(data);"
                            "      var button = document.getElementById('ledToggleButton');"
                            "      button.innerHTML = button.innerHTML === 'Encender LED' ? 'Apagar LED' : 'Encender LED';"
                            "    });"
                            "}"
                            
                            "document.getElementById('wifiForm').addEventListener('submit', function(event) {"
                            "  event.preventDefault();"
                            "  document.getElementById('loading').style.display = 'block';"
                            "  var formData = new FormData(this);"
                            "  fetch('/connect', { method: 'POST', body: new URLSearchParams(formData) })"
                            "    .then(response => response.text())"
                            "    .then(data => alert(data))"
                            "    .finally(() => document.getElementById('loading').style.display = 'none');"
                            "});"
                            "</script>"
                            "</body></html>";

    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Manejador para recibir credenciales WiFi
esp_err_t post_handler(httpd_req_t *req) {
    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    content[ret] = '\0';

    ESP_LOGI(TAG, "Datos recibidos: %s", content);

    char *ssid_param = strstr(content, "ssid=");
    char *pass_param = strstr(content, "&password=");

    if (!ssid_param || !pass_param) {
        httpd_resp_send(req, "Error en los datos", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ssid_param += 5;
    pass_param += 10;

    char ssid[32] = {0}, password[64] = {0};
    strncpy(ssid, ssid_param, pass_param - ssid_param - 10);
    strncpy(password, pass_param, sizeof(password) - 1);

    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "Password: %s", password);

    httpd_resp_send(req, "Conectando a WiFi...", HTTPD_RESP_USE_STRLEN);

    connect_to_wifi(ssid, password);
    return ESP_OK;
}

// Manejador para controlar el LED
esp_err_t led_toggle_handler(httpd_req_t *req) {
    static bool led_state = true;  // El estado inicial del LED es apagado

    // Cambiar el estado del LED
    led_state = !led_state;

    // Controlar el pin GPIO 35
    gpio_set_level(LED_GPIO, led_state ? 1 : 0);
    
    return ESP_OK;
}

// Iniciar servidor web
void start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    esp_err_t err = httpd_start(&server, &config);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Servidor HTTP iniciado con éxito.");
        httpd_uri_t uri_get = { .uri = "/", .method = HTTP_GET, .handler = get_handler };
        httpd_uri_t uri_post = { .uri = "/connect", .method = HTTP_POST, .handler = post_handler };
        httpd_uri_t uri_led = { .uri = "/led_toggle", .method = HTTP_GET, .handler = led_toggle_handler };

        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &uri_post);
        httpd_register_uri_handler(server, &uri_led);
    } else {
        ESP_LOGE(TAG, "Error al iniciar el servidor HTTP: %s", esp_err_to_name(err));
    }
}


void stop_webserver() {
    if (server) {
        ESP_LOGI(TAG, "Esperando antes de detener el servidor HTTP...");
        vTaskDelay(pdMS_TO_TICKS(100));  // Espera de 100 ms para que las conexiones se liberen

        ESP_LOGI(TAG, "Deteniendo servidor HTTP...");
        esp_err_t err = httpd_stop(server);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Servidor HTTP detenido con éxito.");
            server = NULL;
        } else {
            ESP_LOGE(TAG, "Error al detener el servidor HTTP: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "El servidor HTTP ya estaba detenido.");
    }
}

