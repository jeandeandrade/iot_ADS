/**
 * @file mqtt_system.c
 * @brief Sistema IoT MQTT para ESP32 - Implementação
 *
 * Implementação completa do sistema MQTT IoT incluindo:
 * - Inicialização de WiFi e MQTT
 * - Tasks de telemetria e monitoramento
 * - Handlers de eventos
 * - Funções auxiliares
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

/*
 * =============================================================================
 * INCLUDES
 * =============================================================================
 */
#include "mqtt_system.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "driver/gpio.h"

/*
 * =============================================================================
 * DEFINIÇÕES PRIVADAS
 * =============================================================================
 */

/** Tag para logging */
static const char *TAG = "MQTT_SYSTEM";

/** Event bits para sincronização WiFi */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/*
 * =============================================================================
 * VARIÁVEIS PRIVADAS (static)
 * =============================================================================
 */

/** Instância global de estatísticas */
static mqtt_statistics_t s_stats = {0};

/** Handle do cliente MQTT */
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

/** Flag indicando se MQTT está conectado */
static bool s_mqtt_connected = false;

/** Contador de tentativas de reconexão WiFi */
static int s_wifi_retry_num = 0;

/** Variável para controle do tempo de temperatura baixa do AC */
static uint64_t s_temp_low_start_time_ms = 0;

/** Handles das tasks do sistema */
static TaskHandle_t s_task_telemetry = NULL;
static TaskHandle_t s_task_health = NULL;
static TaskHandle_t s_task_wifi_watchdog = NULL;
static TaskHandle_t s_task_ac_monitor = NULL;

/** Flag indicando se sistema foi inicializado */
static bool s_system_initialized = false;

/*
 * =============================================================================
 * DECLARAÇÕES FORWARD DE FUNÇÕES PRIVADAS
 * =============================================================================
 */

/* Handlers de eventos */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data);

/* Funções de inicialização */
static esp_err_t init_nvs(void);
static esp_err_t init_gpio(void);

static esp_err_t init_wifi(void);
static esp_err_t init_mqtt(void);
static esp_err_t create_tasks(void);

/* Tasks */
static void telemetry_task(void *pvParameters);
static void health_monitoring_task(void *pvParameters);
static void wifi_watchdog_task(void *pvParameters);
static void ac_monitor_task(void *pvParameters);

/* Funções auxiliares */
static esp_err_t wait_for_wifi_connection(uint32_t timeout_sec);
static esp_err_t wait_for_mqtt_connection(uint32_t timeout_sec);

/*
 * =============================================================================
 * IMPLEMENTAÇÃO DAS FUNÇÕES PÚBLICAS
 * =============================================================================
 */

esp_err_t mqtt_system_init(void)
{
    if (s_system_initialized)
    {
        ESP_LOGW(TAG, "Sistema ja inicializado");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "   Sistema IoT MQTT - Inicializacao");
    ESP_LOGI(TAG, "===========================================");

    esp_err_t ret;

    /* Fase 1: Subsistemas base */
    ESP_LOGI(TAG, "FASE 1: Inicializando subsistemas base...");

    ret = init_gpio();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar GPIOs");
        return ret;
    }

    ret = init_nvs();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar NVS");
        return ret;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "  Netif inicializado");

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "  Event loop criado");

    memset(&s_stats, 0, sizeof(mqtt_statistics_t));
    ESP_LOGI(TAG, "  Estatisticas inicializadas");

    /* Fase 2: WiFi */
#ifdef CONFIG_QEMU_MODE
    ESP_LOGW(TAG, "FASE 2: MODO QEMU - WiFi desabilitado");
    ESP_LOGW(TAG, "  Executando em emulacao, funcionalidades de rede limitadas");
#else
    ESP_LOGI(TAG, "FASE 2: Configurando WiFi...");

    ret = init_wifi();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar WiFi");
        return ret;
    }

    ret = wait_for_wifi_connection(30);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Timeout aguardando conexão WiFi");
        return ret;
    }
#endif

    /* Fase 3: MQTT */
#ifdef CONFIG_QEMU_MODE
    ESP_LOGW(TAG, "FASE 3: MODO QEMU - MQTT desabilitado");
#else
    ESP_LOGI(TAG, "FASE 3: Inicializando MQTT...");

    ret = init_mqtt();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar MQTT");
        return ret;
    }

    ret = wait_for_mqtt_connection(20);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "Timeout MQTT - continuando em modo degradado");
    }
#endif

    /* Fase 4: Tasks */
    ESP_LOGI(TAG, "FASE 4: Criando tasks da aplicacao...");

    ret = create_tasks();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao criar tasks");
        return ret;
    }

    /* Publicar status online */
    if (s_mqtt_connected)
    {
        mqtt_publish_status(true);

        char boot_info[256];
        snprintf(boot_info, sizeof(boot_info),
                 "{\"device\":\"esp32_central\","
                 "\"firmware\":\"1.0.0\","
                 "\"reset_reason\":%d,"
                 "\"free_heap\":%lu,"
                 "\"idf_version\":\"%s\"}",
                 esp_reset_reason(),
                 esp_get_free_heap_size(),
                 esp_get_idf_version());

        mqtt_publish_data(MQTT_TOPIC_BOOT, boot_info, 0, 1, false);
    }

    s_system_initialized = true;

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "  Sistema IoT MQTT Inicializado!");
    ESP_LOGI(TAG, "===========================================");
    ESP_LOGI(TAG, "");

    return ESP_OK;
}

esp_err_t mqtt_system_shutdown(void)
{
    if (!s_system_initialized)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Desligando sistema MQTT...");

    /* Publicar offline */
    if (s_mqtt_connected)
    {
        mqtt_publish_status(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    /* Parar tasks */
    if (s_task_telemetry)
    {
        vTaskDelete(s_task_telemetry);
        s_task_telemetry = NULL;
    }
    if (s_task_health)
    {
        vTaskDelete(s_task_health);
        s_task_health = NULL;
    }
    if (s_task_wifi_watchdog)
    {
        vTaskDelete(s_task_wifi_watchdog);
        s_task_wifi_watchdog = NULL;
    }
    if (s_task_ac_monitor)
    {
        vTaskDelete(s_task_ac_monitor);
        s_task_ac_monitor = NULL;
    }

    /* Desconectar MQTT */
    if (s_mqtt_client)
    {
        esp_mqtt_client_stop(s_mqtt_client);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
    }

    s_system_initialized = false;
    s_mqtt_connected = false;

    ESP_LOGI(TAG, "Sistema desligado");

    return ESP_OK;
}

bool mqtt_system_is_connected(void)
{
    return s_mqtt_connected;
}

int mqtt_publish_data(const char *topic, const char *data,
                      int len, int qos, bool retain)
{
    if (s_mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "Cliente MQTT nao inicializado");
        s_stats.falhas_publicacao++;
        return -1;
    }

    if (!s_mqtt_connected)
    {
        ESP_LOGW(TAG, "MQTT desconectado, não e possível publicar");
        s_stats.falhas_publicacao++;
        return -1;
    }

    if (len == 0)
    {
        len = strlen(data);
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client,
                                         topic,
                                         data,
                                         len,
                                         qos,
                                         retain ? 1 : 0);

    if (msg_id >= 0)
    {
        s_stats.total_publicadas++;
        ESP_LOGD(TAG, "Publicado em '%s' (msg_id=%d, QoS=%d)",
                 topic, msg_id, qos);
    }
    else
    {
        s_stats.falhas_publicacao++;
        ESP_LOGE(TAG, "Falha ao publicar em '%s'", topic);
    }

    return msg_id;
}

int mqtt_publish_telemetry(const telemetry_data_t *data)
{
    if (data == NULL)
    {
        return -1;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer),
             "{"
             "\"temperatura\":%.2f,"
             "\"umidade\":%.2f,"
             "\"contador\":%lu,"
             "\"timestamp\":%llu"
             "}",
             data->temperatura,
             data->umidade,
             data->contador,
             data->timestamp);

    return mqtt_publish_data(MQTT_TOPIC_TELEMETRY, buffer, 0, 1, false);
}

int mqtt_publish_health_check(void)
{
    health_status_t health;

    if (mqtt_get_health_status(&health) != ESP_OK)
    {
        return -1;
    }

    char buffer[512];
    snprintf(buffer, sizeof(buffer),
             "{"
             "\"free_heap\":%lu,"
             "\"min_free_heap\":%lu,"
             "\"wifi_rssi\":%d,"
             "\"uptime_sec\":%llu,"
             "\"mqtt_connected\":%d,"
             "\"msgs_sent\":%lu,"
             "\"msgs_received\":%lu,"
             "\"mqtt_failures\":%lu,"
             "\"disconnects\":%lu"
             "}",
             health.free_heap,
             health.min_free_heap,
             health.wifi_rssi,
             health.uptime_sec,
             health.mqtt_connected ? 1 : 0,
             s_stats.total_publicadas,
             s_stats.total_recebidas,
             s_stats.falhas_publicacao,
             s_stats.desconexoes);

    return mqtt_publish_data(MQTT_TOPIC_HEALTH, buffer, 0, 0, false);
}

int mqtt_publish_status(bool online)
{
    const char *status = online ? "online" : "offline";
    return mqtt_publish_data(MQTT_TOPIC_STATUS, status, 0, 1, true);
}

int mqtt_subscribe_topic(const char *topic, int qos)
{
    if (s_mqtt_client == NULL || !s_mqtt_connected)
    {
        return -1;
    }

    int msg_id = esp_mqtt_client_subscribe(s_mqtt_client, topic, qos);

    if (msg_id >= 0)
    {
        ESP_LOGI(TAG, "Subscrito em '%s' (QoS=%d, msg_id=%d)",
                 topic, qos, msg_id);
    }
    else
    {
        ESP_LOGE(TAG, "Falha ao subscrever em '%s'", topic);
    }

    return msg_id;
}

int mqtt_unsubscribe_topic(const char *topic)
{
    if (s_mqtt_client == NULL || !s_mqtt_connected)
    {
        return -1;
    }

    int msg_id = esp_mqtt_client_unsubscribe(s_mqtt_client, topic);

    if (msg_id >= 0)
    {
        ESP_LOGI(TAG, "Cancelada subscricao em '%s' (msg_id=%d)",
                 topic, msg_id);
    }

    return msg_id;
}

esp_err_t mqtt_get_statistics(mqtt_statistics_t *stats)
{
    if (stats == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &s_stats, sizeof(mqtt_statistics_t));
    return ESP_OK;
}

void mqtt_reset_statistics(void)
{
    uint32_t desconexoes = s_stats.desconexoes;
    uint32_t tempo_desconectado = s_stats.tempo_desconectado_ms;

    memset(&s_stats, 0, sizeof(mqtt_statistics_t));

    s_stats.desconexoes = desconexoes;
    s_stats.tempo_desconectado_ms = tempo_desconectado;

    ESP_LOGI(TAG, "Estatisticas resetadas");
}

esp_err_t mqtt_get_health_status(health_status_t *health)
{
    if (health == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    health->free_heap = esp_get_free_heap_size();
    health->min_free_heap = esp_get_minimum_free_heap_size();
    health->uptime_sec = esp_timer_get_time() / 1000000ULL;
    health->mqtt_connected = s_mqtt_connected;

#ifndef CONFIG_QEMU_MODE
    esp_wifi_sta_get_rssi(&health->wifi_rssi);
#else
    health->wifi_rssi = -127; // Valor inválido para indicar modo QEMU
#endif

    return ESP_OK;
}

void mqtt_print_statistics(void)
{
    ESP_LOGI(TAG, "=== Estatisticas MQTT ===");
    ESP_LOGI(TAG, "Publicadas   : %lu", s_stats.total_publicadas);
    ESP_LOGI(TAG, "Recebidas    : %lu", s_stats.total_recebidas);
    ESP_LOGI(TAG, "Falhas       : %lu", s_stats.falhas_publicacao);
    ESP_LOGI(TAG, "Desconexoes  : %lu", s_stats.desconexoes);
    ESP_LOGI(TAG, "Tempo offline: %lu ms", s_stats.tempo_desconectado_ms);
    ESP_LOGI(TAG, "========================");
}

/*
 * =============================================================================
 * IMPLEMENTAÇÃO DAS FUNÇÕES PRIVADAS
 * =============================================================================
 */

static esp_err_t init_nvs(void)
{
    ESP_LOGI(TAG, "  -> Inicializando NVS...");

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "  NVS corrompido, apagando...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "  NVS inicializado");
    }

    return ret;
}

static esp_err_t init_wifi(void)
{
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler,
                                               NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "  WiFi iniciado");

    return ESP_OK;
}

static esp_err_t init_mqtt(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials = {
            .client_id = CONFIG_MQTT_CLIENT_ID,
            .username = CONFIG_MQTT_USERNAME,
            .authentication = {
                .password = CONFIG_MQTT_PASSWORD,
            }},

        .session.last_will.topic = MQTT_TOPIC_STATUS,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,

        .session.keepalive = MQTT_KEEPALIVE_SEC,
        .session.disable_clean_session = false,
        .network.timeout_ms = MQTT_TIMEOUT_MS,

        .buffer.size = MQTT_BUFFER_SIZE,
        .buffer.out_size = MQTT_BUFFER_SIZE,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    if (s_mqtt_client == NULL)
    {
        ESP_LOGE(TAG, "  Falha ao criar cliente MQTT");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  Cliente MQTT criado");

    esp_err_t ret = esp_mqtt_client_register_event(s_mqtt_client,
                                                   ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   NULL);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "  Falha ao registrar handler");
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "  Handler registrado");

    ret = esp_mqtt_client_start(s_mqtt_client);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "  Falha ao iniciar cliente");
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "  Cliente MQTT iniciado");

    return ESP_OK;
}

static esp_err_t create_tasks(void)
{
    BaseType_t ret;

    ret = xTaskCreate(telemetry_task, "Telemetry",
                      4096, NULL, 5, &s_task_telemetry);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "  Falha ao criar task de telemetria");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  Task de telemetria criada");

    ret = xTaskCreate(health_monitoring_task, "HealthMon",
                      3072, NULL, 3, &s_task_health);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "  Falha ao criar task de health");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  Task de health criada");

    ret = xTaskCreate(ac_monitor_task, "ACMonitor",
                      2048, NULL, 3, &s_task_ac_monitor);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "  Falha ao criar task de monitoramento do AC");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  Task de monitoramento do AC criada");

#ifndef CONFIG_QEMU_MODE
    ret = xTaskCreate(wifi_watchdog_task, "WiFiWatchdog",
                      2048, NULL, 4, &s_task_wifi_watchdog);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "  Falha ao criar task de watchdog");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "  Task de watchdog criada");
#else
    ESP_LOGI(TAG, "  Task de watchdog ignorada (modo QEMU)");
#endif

    return ESP_OK;
}

static esp_err_t wait_for_wifi_connection(uint32_t timeout_sec)
{
    ESP_LOGI(TAG, "  Aguardando conexão WiFi...");

    uint32_t count = 0;
    wifi_ap_record_t ap_info;

    while (count < timeout_sec)
    {
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK)
        {
            ESP_LOGI(TAG, "  WiFi conectado!");

            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif != NULL)
            {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK)
                {
                    ESP_LOGI(TAG, "  IP: " IPSTR, IP2STR(&ip_info.ip));
                }
            }

            return ESP_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
        count++;

        if (count % 5 == 0)
        {
            ESP_LOGI(TAG, "  Aguardando... (%lu s)", count);
        }
    }

    return ESP_ERR_TIMEOUT;
}

static esp_err_t init_gpio(void)
{
    // Configura GPIO 18 (Luzes) como saída
    gpio_reset_pin(GPIO_NUM_18);
    gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_18, 0); // Começa desligado

    // Configura GPIO 19 (Ar Condicionado) como saída
    gpio_reset_pin(GPIO_NUM_19);
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_19, 0); // Começa desligado

    ESP_LOGI(TAG, "  GPIOs 18 e 19 inicializados");
    return ESP_OK;
}

static esp_err_t wait_for_mqtt_connection(uint32_t timeout_sec)
{
    ESP_LOGI(TAG, "  Aguardando conexão MQTT...");

    uint32_t count = 0;

    while (!s_mqtt_connected && count < timeout_sec)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        count++;

        if (count % 5 == 0)
        {
            ESP_LOGI(TAG, "  Aguardando... (%lu s)", count);
        }
    }

    if (s_mqtt_connected)
    {
        ESP_LOGI(TAG, "  MQTT conectado!");
        return ESP_OK;
    }

    return ESP_ERR_TIMEOUT;
}

/*
 * =============================================================================
 * HANDLERS DE EVENTOS
 * =============================================================================
 */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi iniciado, conectando...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_wifi_retry_num < WIFI_MAX_RETRY)
        {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGW(TAG, "Reconectando WiFi... (%d/%d)",
                     s_wifi_retry_num, WIFI_MAX_RETRY);
        }
        else
        {
            ESP_LOGE(TAG, "Falha ao conectar WiFi após %d tentativas",
                     WIFI_MAX_RETRY);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_num = 0;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT conectado ao broker!");
        s_mqtt_connected = true;

        ESP_LOGI(TAG, "Inscrevendo-se nos tópicos do projeto...");
        mqtt_subscribe_topic("casa/externo/luminosidade", 1);
        mqtt_subscribe_topic("casa/sala/temperatura", 1);

        ESP_LOGI(TAG, "Inscrevendo-se nos tópicos padrão do sistema...");
        mqtt_subscribe_topic(MQTT_TOPIC_COMMANDS, 1);
        mqtt_subscribe_topic("demo/config/#", 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT desconectado");
        s_mqtt_connected = false;
        s_stats.desconexoes++;
        break;

    case MQTT_EVENT_DATA: {
        // Bloco de escopo para VLA
        ESP_LOGI(TAG, "Mensagem MQTT:");
        ESP_LOGI(TAG, "  Topico: %.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "  Dados: %.*s", event->data_len, event->data);
        s_stats.total_recebidas++;
        s_stats.ultima_mensagem_ts = esp_timer_get_time() / 1000ULL;

        // Copiar tópico e dados para strings terminadas em null
        char topic[event->topic_len + 1];
        char data[event->data_len + 1];
        strncpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = '\0';
        strncpy(data, event->data, event->data_len);
        data[event->data_len] = '\0';

        // Tenta converter o dado para inteiro
        int value = atoi(data);

        // --- Lógica de Controle de Luminosidade (GPIO 18) ---
        if (strcmp(topic, "casa/externo/luminosidade") == 0)
        {
            if (value < 3)
            {
                // Acender luzes (GPIO 18 para 1)
                gpio_set_level(GPIO_NUM_18, 1);
                ESP_LOGI(TAG, "Luminosidade (%d < 3). Luzes (GPIO 18) ACESAS.", value);
            }
            else
            {
                // Apagar luzes (GPIO 18 para 0)
                gpio_set_level(GPIO_NUM_18, 0);
                ESP_LOGI(TAG, "Luminosidade (%d >= 3). Luzes (GPIO 18) APAGADAS.", value);
            }
        }

        // --- Lógica de Controle de Temperatura (GPIO 19) ---
        else if (strcmp(topic, "casa/sala/temperatura") == 0)
        {
            if (value > 23)
            {
                // Ligar ar condicionado (GPIO 19 para 1)
                gpio_set_level(GPIO_NUM_19, 1);
                s_temp_low_start_time_ms = 0; // Reseta o contador de tempo
                ESP_LOGI(TAG, "Temperatura (%d > 23). Ar Condicionado (GPIO 19) LIGADO.", value);
            }
            else if (value < 20)
            {
                // Temperatura abaixo de 20. Inicia/continua contagem.
                if (gpio_get_level(GPIO_NUM_19) == 1) // Se o AC estiver ligado
                {
                    if (s_temp_low_start_time_ms == 0)
                    {
                        s_temp_low_start_time_ms = esp_timer_get_time() / 1000ULL;
                        ESP_LOGW(TAG, "Temperatura (%d < 20). Iniciando contagem para desligar AC.", value);
                    }
                    else
                    {
                        // A lógica de desligamento por tempo será implementada na Fase 6
                        ESP_LOGD(TAG, "Temperatura (%d < 20). Contagem em andamento.", value);
                    }
                }
                else
                {
                    s_temp_low_start_time_ms = 0; // AC já está desligado
                    ESP_LOGD(TAG, "Temperatura (%d < 20). AC já está desligado.", value);
                }
            }
            else
            {
                // Temperatura entre 20 e 23. Reseta o contador de tempo.
                s_temp_low_start_time_ms = 0;
                ESP_LOGD(TAG, "Temperatura (%d entre 20 e 23). Contador de tempo resetado.", value);
            }
        }

        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Erro MQTT");
        break;

    default:
        ESP_LOGD(TAG, "Evento MQTT: %d", event_id);
        break;
    }
}

/*
 * =============================================================================
 * TASKS
 * =============================================================================
 */

static void telemetry_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task de telemetria iniciada");

    telemetry_data_t data = {0};

    while (1)
    {
        if (s_mqtt_connected)
        {
            data.temperatura = 20.0f + (esp_random() % 150) / 10.0f;
            data.umidade = 40.0f + (esp_random() % 400) / 10.0f;
            data.timestamp = esp_timer_get_time() / 1000ULL;
            data.contador++;

            mqtt_publish_telemetry(&data);

            ESP_LOGI(TAG, "Telemetria: T=%.1f°C, H=%.1f%% (#%lu)",
                     data.temperatura, data.umidade, data.contador);
        }

        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_INTERVAL_MS));
    }
}

static void health_monitoring_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task de health monitoring iniciada");

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(HEALTH_CHECK_INTERVAL_MS));

        if (s_mqtt_connected)
        {
            mqtt_publish_health_check();

            health_status_t health;
            mqtt_get_health_status(&health);

            ESP_LOGI(TAG, "Health: Heap=%lu bytes, RSSI=%d dBm",
                     health.free_heap, health.wifi_rssi);

            if (health.free_heap < 20000)
            {
                ESP_LOGW(TAG, "Memoria baixa!");
            }
        }
    }
}

static void ac_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task de monitoramento do AC iniciada");

    const uint64_t TEN_MINUTES_MS = 10 * 60 * 1000; // 10 minutos em milissegundos

    while (1)
    {
        // Verifica a cada 10 segundos
        vTaskDelay(pdMS_TO_TICKS(10000));

        // Verifica se o AC está ligado (GPIO 19 == 1)
        if (gpio_get_level(GPIO_NUM_19) == 1)
        {
            // Verifica se a contagem de tempo de temperatura baixa foi iniciada
            if (s_temp_low_start_time_ms > 0)
            {
                uint64_t current_time_ms = esp_timer_get_time() / 1000ULL;
                uint64_t elapsed_time_ms = current_time_ms - s_temp_low_start_time_ms;

                if (elapsed_time_ms >= TEN_MINUTES_MS)
                {
                    // Desliga o ar condicionado (GPIO 19 para 0)
                    gpio_set_level(GPIO_NUM_19, 0);
                    s_temp_low_start_time_ms = 0; // Reseta o contador
                    ESP_LOGW(TAG, "AC DESLIGADO: Temperatura abaixo de 20 por 10 minutos.");
                }
                else
                {
                    uint64_t remaining_sec = (TEN_MINUTES_MS - elapsed_time_ms) / 1000;
                    ESP_LOGD(TAG, "AC LIGADO: Temp baixa por %llu segundos. Desliga em %llu segundos.",
                             elapsed_time_ms / 1000, remaining_sec);
                }
            }
            else
            {
                ESP_LOGD(TAG, "AC LIGADO: Temperatura OK ou contagem não iniciada.");
            }
        }
        else
        {
            // AC está desligado, garante que o contador está zerado
            s_temp_low_start_time_ms = 0;
            ESP_LOGD(TAG, "AC DESLIGADO: Monitoramento inativo.");
        }
    }
}

static void wifi_watchdog_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Task de WiFi watchdog iniciada");

    wifi_ap_record_t ap_info;

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(WIFI_WATCHDOG_INTERVAL_MS));

        if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK)
        {
            ESP_LOGW(TAG, "WiFi desconectado, reconectando...");
            s_wifi_retry_num = 0;
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGD(TAG, "WiFi OK - RSSI: %d dBm", ap_info.rssi);
        }
    }
}
