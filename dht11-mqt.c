
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "dht.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// Configurações de WiFi e MQTT
const char* ssid = "JENIFER2G"; // nome da rede WiFi
const char* password = "32481557"; //  senha da rede WiFi
const char* mqtt_server = "mqtt://test.mosquitto.org"; // Broker MQTT público

// Pino do sensor DHT11 (D4 corresponde ao GPIO2)
#define DHT_PIN GPIO_NUM_2
static const char* TAG = "DHT11";

// Configuração do cliente MQTT
esp_mqtt_client_handle_t mqtt_client = NULL;

void dht_task(void *pvParameter) {
    while (1) {
        int16_t temperature = 0;
        int16_t humidity = 0;

        if (dht_read_data(DHT_TYPE_DHT11, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            ESP_LOGI(TAG, "Umidade: %d.%d%% Temperatura: %d.%d°C",
                humidity / 10, humidity % 10, temperature / 10, temperature % 10);

            // Formata a mensagem para temperatura
            char temp_msg[50];
            sprintf(temp_msg, "{\"temperature\": %d.%d}", temperature / 10, temperature % 10);

            // Publica a mensagem de temperatura
            esp_mqtt_client_publish(mqtt_client, "mestrado/iot/aluno/claudio/temperatura", temp_msg, 0, 1, 0);

            // Formata a mensagem para umidade
            char hum_msg[50];
            sprintf(hum_msg, "{\"humidity\": %d.%d}", humidity / 10, humidity % 10);

            // Publica a mensagem de umidade
            esp_mqtt_client_publish(mqtt_client, "mestrado/iot/aluno/claudio/umidade", hum_msg, 0, 1, 0);
        } else {
            ESP_LOGE(TAG, "Falha ao ler dados do sensor DHT11");
        }

        vTaskDelay(120000 / portTICK_PERIOD_MS);  // Aguarda 2 minutos
    }
}

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI("WiFi", "Desconectado do Wi-Fi, tentando reconectar...");
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init() {
    tcpip_adapter_init(); // Inicializa o adaptador TCP/IP
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); // Define o modo Wi-Fi para estação
    wifi_config_t wifi_config = {};
    strncpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*) wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config)); // Configura o Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start()); // Inicia o Wi-Fi

    // Adiciona log para verificar a conexão Wi-Fi
    if (esp_wifi_connect() == ESP_OK) {
        ESP_LOGI("WiFi", "Conectado ao Wi-Fi");
    } else {
        ESP_LOGE("WiFi", "Falha ao conectar ao Wi-Fi");
    }
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Conectado ao servidor MQTT");
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI("MQTT", "Desconectado do servidor MQTT");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE("MQTT", "Erro na conexão MQTT");
            break;

        default:
            ESP_LOGI("MQTT", "Evento MQTT recebido: %d", event_id);
            break;
    }
}

void mqtt_init() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_server,
        .port = 1883, // Porta padrão para MQTT
        .client_id = "esp8266_client", // ID único do cliente
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void app_main() {
    // Inicializa NVS (necessário para o WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Inicializa WiFi e MQTT
    wifi_init();
    mqtt_init();

    // Inicia a leitura do sensor DHT11
    ESP_LOGI(TAG, "Iniciando leitura do sensor DHT11...");
    xTaskCreate(&dht_task, "dht_task", 2048, NULL, 5, NULL);
}
