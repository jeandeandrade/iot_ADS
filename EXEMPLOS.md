# Exemplos de Uso - mqtt_system 💡

Este documento apresenta exemplos práticos de como usar a biblioteca `mqtt_system` em diferentes cenários.

## Exemplo 1: Inicialização Básica

### Aplicação Mínima

```c
#include "mqtt_system.h"
#include "esp_log.h"

void app_main(void)
{
    // Uma linha inicializa tudo!
    esp_err_t ret = mqtt_system_init();
    
    if (ret == ESP_OK) {
        ESP_LOGI("APP", "Sistema rodando!");
        
        // Loop infinito
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
```

## Exemplo 2: Publicação de Dados Customizados

### Sensor de Temperatura DHT22

```c
#include "mqtt_system.h"
#include "dht22_driver.h"  // Seu driver de sensor

void app_main(void)
{
    mqtt_system_init();
    
    // Inicializar sensor DHT22 no GPIO 4
    dht22_init(GPIO_NUM_4);
    
    while (1) {
        if (mqtt_system_is_connected()) {
            // Ler sensor
            float temp, hum;
            if (dht22_read(&temp, &hum) == ESP_OK) {
                
                // Opção 1: Publicação simples
                char msg[64];
                snprintf(msg, sizeof(msg), "%.1f", temp);
                mqtt_publish_data("casa/sala/temperatura", msg, 0, 0, false);
                
                // Opção 2: Publicação estruturada (JSON)
                char json[128];
                snprintf(json, sizeof(json),
                        "{\"temp\":%.1f,\"hum\":%.1f,\"sensor\":\"DHT22\"}",
                        temp, hum);
                mqtt_publish_data("casa/sala/clima", json, 0, 1, false);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000));  // 30 segundos
    }
}
```

## Exemplo 3: Recebendo Comandos MQTT

### Sistema de Controle de LED

```c
#include "mqtt_system.h"
#include "driver/gpio.h"

#define LED_GPIO GPIO_NUM_2

// Handler customizado para processar mensagens recebidas
static void process_command(const char *topic, const char *data, int len)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "%.*s", len, data);
    
    if (strcmp(topic, "casa/led/comando") == 0) {
        if (strcmp(cmd, "LIGAR") == 0) {
            gpio_set_level(LED_GPIO, 1);
            ESP_LOGI("CMD", "LED ligado");
            
            // Confirmar ação
            mqtt_publish_data("casa/led/status", "ligado", 0, 1, true);
            
        } else if (strcmp(cmd, "DESLIGAR") == 0) {
            gpio_set_level(LED_GPIO, 0);
            ESP_LOGI("CMD", "LED desligado");
            
            mqtt_publish_data("casa/led/status", "desligado", 0, 1, true);
        }
    }
}

void app_main(void)
{
    // Configurar GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    
    // Inicializar MQTT
    mqtt_system_init();
    
    // Aguardar conexão
    while (!mqtt_system_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Subscrever em tópico de comandos
    mqtt_subscribe_topic("casa/led/comando", 1);
    
    // Publicar status inicial
    mqtt_publish_data("casa/led/status", "desligado", 0, 1, true);
    
    // Loop principal processa comandos
    // (mensagens chegam via mqtt_event_handler e chamariam process_command)
}
```

## Exemplo 4: Sistema Multi-Sensor com Alertas

### Monitoramento de Estufa

```c
#include "mqtt_system.h"

typedef struct {
    float temp_min;
    float temp_max;
    float hum_min;
    float hum_max;
} limites_t;

static limites_t limites = {
    .temp_min = 18.0,
    .temp_max = 28.0,
    .hum_min = 40.0,
    .hum_max = 80.0
};

static void publicar_alerta(const char *tipo, float valor, float limite)
{
    char alerta[256];
    snprintf(alerta, sizeof(alerta),
            "{"
            "\"tipo\":\"%s\","
            "\"valor\":%.2f,"
            "\"limite\":%.2f,"
            "\"timestamp\":%llu"
            "}",
            tipo, valor, limite, esp_timer_get_time() / 1000ULL);
    
    mqtt_publish_data("estufa/alertas", alerta, 0, 1, false);
}

static void monitorar_sensores_task(void *pvParameters)
{
    while (1) {
        if (mqtt_system_is_connected()) {
            // Simular leitura (substitua por leitura real)
            float temp = 20.0 + (esp_random() % 200) / 10.0;
            float hum = 50.0 + (esp_random() % 400) / 10.0;
            
            // Publicar dados
            char dados[128];
            snprintf(dados, sizeof(dados),
                    "{\"temp\":%.1f,\"hum\":%.1f}", temp, hum);
            mqtt_publish_data("estufa/sensores/principal", dados, 0, 0, false);
            
            // Verificar limites e gerar alertas
            if (temp > limites.temp_max) {
                publicar_alerta("TEMP_ALTA", temp, limites.temp_max);
                ESP_LOGW("ALERTA", "Temperatura alta: %.1f°C", temp);
            }
            
            if (temp < limites.temp_min) {
                publicar_alerta("TEMP_BAIXA", temp, limites.temp_min);
                ESP_LOGW("ALERTA", "Temperatura baixa: %.1f°C", temp);
            }
            
            if (hum > limites.hum_max) {
                publicar_alerta("HUM_ALTA", hum, limites.hum_max);
            }
            
            if (hum < limites.hum_min) {
                publicar_alerta("HUM_BAIXA", hum, limites.hum_min);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    mqtt_system_init();
    
    // Criar task customizada
    xTaskCreate(monitorar_sensores_task, "Monitor",
                4096, NULL, 5, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
```

## Exemplo 5: Sistema de Irrigação Completo

### Controlador Inteligente

```c
#include "mqtt_system.h"

typedef struct {
    bool bomba_ativa;
    bool modo_automatico;
    float limiar_umidade;
    uint8_t valvulas_abertas;  // Bitmask de válvulas
} estado_irrigacao_t;

static estado_irrigacao_t estado = {
    .bomba_ativa = false,
    .modo_automatico = true,
    .limiar_umidade = 30.0,
    .valvulas_abertas = 0x00
};

static void controlar_bomba(bool ligar)
{
    // Código para controlar GPIO da bomba
    estado.bomba_ativa = ligar;
    
    const char *status = ligar ? "ligada" : "desligada";
    mqtt_publish_data("irrigacao/bomba/status", status, 0, 1, true);
    
    ESP_LOGI("IRRIGACAO", "Bomba %s", status);
}

static void controlar_valvula(uint8_t numero, bool abrir)
{
    if (numero > 7) return;  // Máximo 8 válvulas
    
    if (abrir) {
        estado.valvulas_abertas |= (1 << numero);
    } else {
        estado.valvulas_abertas &= ~(1 << numero);
    }
    
    char topico[64];
    snprintf(topico, sizeof(topico), "irrigacao/valvula/%d/status", numero);
    
    const char *status = abrir ? "aberta" : "fechada";
    mqtt_publish_data(topico, status, 0, 1, true);
    
    ESP_LOGI("IRRIGACAO", "Válvula %d %s", numero, status);
}

static void logica_irrigacao_task(void *pvParameters)
{
    while (1) {
        if (mqtt_system_is_connected() && estado.modo_automatico) {
            
            // Simular leituras de sensores de umidade do solo
            float umidade_zona1 = 40.0 + (esp_random() % 400) / 10.0;
            float umidade_zona2 = 35.0 + (esp_random() % 450) / 10.0;
            
            // Publicar umidade
            char msg[64];
            snprintf(msg, sizeof(msg), "%.1f", umidade_zona1);
            mqtt_publish_data("irrigacao/zona1/umidade", msg, 0, 0, false);
            
            snprintf(msg, sizeof(msg), "%.1f", umidade_zona2);
            mqtt_publish_data("irrigacao/zona2/umidade", msg, 0, 0, false);
            
            // Lógica de controle
            bool precisa_irrigar = false;
            
            if (umidade_zona1 < estado.limiar_umidade) {
                controlar_valvula(0, true);
                precisa_irrigar = true;
            } else {
                controlar_valvula(0, false);
            }
            
            if (umidade_zona2 < estado.limiar_umidade) {
                controlar_valvula(1, true);
                precisa_irrigar = true;
            } else {
                controlar_valvula(1, false);
            }
            
            // Controlar bomba baseado em válvulas abertas
            if (precisa_irrigar && !estado.bomba_ativa) {
                controlar_bomba(true);
            } else if (!precisa_irrigar && estado.bomba_ativa) {
                controlar_bomba(false);
            }
            
            // Publicar estado completo
            char estado_json[256];
            snprintf(estado_json, sizeof(estado_json),
                    "{"
                    "\"bomba\":%d,"
                    "\"modo_auto\":%d,"
                    "\"valvulas\":\"0x%02X\","
                    "\"limiar\":%.1f"
                    "}",
                    estado.bomba_ativa,
                    estado.modo_automatico,
                    estado.valvulas_abertas,
                    estado.limiar_umidade);
            
            mqtt_publish_data("irrigacao/estado", estado_json, 0, 1, false);
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000));  // 30 segundos
    }
}

void app_main(void)
{
    mqtt_system_init();
    
    // Aguardar conexão MQTT
    while (!mqtt_system_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Subscrever comandos
    mqtt_subscribe_topic("irrigacao/comandos/#", 1);
    mqtt_subscribe_topic("irrigacao/config/#", 0);
    
    // Publicar estado inicial
    controlar_bomba(false);
    
    // Criar task de controle
    xTaskCreate(logica_irrigacao_task, "Irrigacao",
                4096, NULL, 6, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Exemplo 6: Integração com Dashboard Web

### Publicação de Métricas para Grafana/Node-RED

```c
#include "mqtt_system.h"
#include "esp_adc/adc_oneshot.h"

static void publicar_metricas_dashboard(void)
{
    health_status_t health;
    mqtt_statistics_t stats;
    
    mqtt_get_health_status(&health);
    mqtt_get_statistics(&stats);
    
    // Formato compatível com Telegraf/InfluxDB
    char metricas[512];
    snprintf(metricas, sizeof(metricas),
            "esp32_metrics,"
            "device_id=central "
            "heap_free=%lu,"
            "heap_min=%lu,"
            "wifi_rssi=%d,"
            "uptime=%llu,"
            "mqtt_msgs_sent=%lu,"
            "mqtt_msgs_received=%lu,"
            "mqtt_failures=%lu "
            "%llu",
            health.free_heap,
            health.min_free_heap,
            health.wifi_rssi,
            health.uptime_sec,
            stats.total_publicadas,
            stats.total_recebidas,
            stats.falhas_publicacao,
            esp_timer_get_time() / 1000ULL);  // timestamp em ms
    
    mqtt_publish_data("telegraf/esp32", metricas, 0, 0, false);
}

static void dashboard_task(void *pvParameters)
{
    while (1) {
        if (mqtt_system_is_connected()) {
            publicar_metricas_dashboard();
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));  // 5 segundos
    }
}

void app_main(void)
{
    mqtt_system_init();
    
    xTaskCreate(dashboard_task, "Dashboard",
                3072, NULL, 4, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Exemplo 7: Sistema com OTA via MQTT

### Atualização Remota de Firmware

```c
#include "mqtt_system.h"
#include "esp_ota_ops.h"

static void processar_comando_ota(const char *data, int len)
{
    char url[256];
    snprintf(url, sizeof(url), "%.*s", len, data);
    
    ESP_LOGI("OTA", "Iniciando OTA de: %s", url);
    
    // Publicar status
    mqtt_publish_data("device/ota/status", "downloading", 0, 1, false);
    
    // Aqui você implementaria o download e instalação do OTA
    // Ver exemplos do ESP-IDF sobre OTA
    
    // Após sucesso
    mqtt_publish_data("device/ota/status", "success", 0, 1, true);
    
    // Reiniciar após 5 segundos
    vTaskDelay(pdMS_TO_TICKS(5000));
    esp_restart();
}

void app_main(void)
{
    mqtt_system_init();
    
    // Subscrever comando OTA
    mqtt_subscribe_topic("device/ota/update", 2);  // QoS 2 para garantir entrega
    
    // Publicar versão atual
    char version[64];
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    snprintf(version, sizeof(version),
            "{\"version\":\"%s\",\"date\":\"%s %s\"}",
            app_desc->version,
            app_desc->date,
            app_desc->time);
    
    mqtt_publish_data("device/info/version", version, 0, 1, true);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Exemplo 8: Testes e Debug

### Modo Debug com Estatísticas Detalhadas

```c
#include "mqtt_system.h"

static void debug_task(void *pvParameters)
{
    uint32_t loop = 0;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000));  // 30 segundos
        loop++;
        
        ESP_LOGI("DEBUG", "========== Loop #%lu ==========", loop);
        
        // Imprimir estatísticas MQTT
        mqtt_print_statistics();
        
        // Informações de memória
        ESP_LOGI("DEBUG", "Heap livre: %lu bytes", 
                 esp_get_free_heap_size());
        ESP_LOGI("DEBUG", "Menor heap livre: %lu bytes", 
                 esp_get_minimum_free_heap_size());
        
        // Status de conexão
        if (mqtt_system_is_connected()) {
            ESP_LOGI("DEBUG", "✅ MQTT: Conectado");
        } else {
            ESP_LOGW("DEBUG", "❌ MQTT: Desconectado");
        }
        
        // Teste de publicação
        char test_msg[64];
        snprintf(test_msg, sizeof(test_msg), "test_loop_%lu", loop);
        int msg_id = mqtt_publish_data("debug/test", test_msg, 0, 0, false);
        ESP_LOGI("DEBUG", "Teste publicação: msg_id=%d", msg_id);
        
        ESP_LOGI("DEBUG", "================================");
    }
}

void app_main(void)
{
    // Aumentar nível de log
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_SYSTEM", ESP_LOG_DEBUG);
    
    mqtt_system_init();
    
    xTaskCreate(debug_task, "Debug",
                3072, NULL, 2, NULL);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Dicas Gerais

### 1. Sempre Verifique Conexão Antes de Publicar
```c
if (mqtt_system_is_connected()) {
    mqtt_publish_data(...);
}
```

### 2. Use QoS Apropriado
- **QoS 0**: Telemetria não crítica, dados frequentes
- **QoS 1**: Comandos importantes, alertas
- **QoS 2**: Comandos críticos (raros, use com moderação)

### 3. Use Retain para Estado
```c
// Bom para status que deve persistir
mqtt_publish_data("device/status", "online", 0, 1, true);
```

### 4. Organize Tópicos Hierarquicamente
```
projeto/
├── device_id/
│   ├── status
│   ├── telemetria/
│   │   ├── temperatura
│   │   └── umidade
│   └── comandos/
│       ├── ligar
│       └── desligar
```

### 5. Implemente Timeout em Operações Críticas
```c
uint32_t timeout = 30;
while (!mqtt_system_is_connected() && timeout > 0) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    timeout--;
}

if (timeout == 0) {
    ESP_LOGE("APP", "Timeout aguardando MQTT");
    // Implementar fallback
}
```

---

Estes exemplos cobrem os casos de uso mais comuns. Adapte-os às necessidades do seu projeto!
