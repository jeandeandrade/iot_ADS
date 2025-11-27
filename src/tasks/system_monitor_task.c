/**
 * @file system_monitor_task.c
 * @brief Implementação da task de monitoramento do sistema
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

#include "tasks/system_monitor_task.h"
#include "services/mqtt_system.h"
#include "esp_log.h"

static const char *TAG = "MONITOR_TASK";

void system_monitor_task(void *pvParameters)
{
    uint32_t loop_count = 0;

    ESP_LOGI(TAG, "Task de monitoramento iniciada");

    while (1)
    {
        /* Aguardar intervalo de monitoramento */
        vTaskDelay(pdMS_TO_TICKS(MONITOR_INTERVAL_MS));
        loop_count++;

        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "════════════════════════════════════════");
        ESP_LOGI(TAG, "  Status do Sistema (Loop #%lu)", loop_count);
        ESP_LOGI(TAG, "════════════════════════════════════════");

        /* Verificar se MQTT está conectado */
        if (mqtt_system_is_connected())
        {
            ESP_LOGI(TAG, "MQTT: Conectado e operacional");

            /* Obter e exibir estatísticas */
            mqtt_statistics_t stats;
            if (mqtt_get_statistics(&stats) == ESP_OK)
            {
                ESP_LOGI(TAG, "Mensagens publicadas: %lu",
                         stats.total_publicadas);
                ESP_LOGI(TAG, "Mensagens recebidas: %lu",
                         stats.total_recebidas);
                ESP_LOGI(TAG, "Falhas de publicacao: %lu",
                         stats.falhas_publicacao);
                ESP_LOGI(TAG, "Desconexoes: %lu",
                         stats.desconexoes);
            }

            /* Obter status de saúde */
            health_status_t health;
            if (mqtt_get_health_status(&health) == ESP_OK)
            {
                ESP_LOGI(TAG, "Heap livre: %lu bytes", health.free_heap);
                ESP_LOGI(TAG, "WiFi RSSI: %d dBm", health.wifi_rssi);
                ESP_LOGI(TAG, "Uptime: %llu segundos", health.uptime_sec);

                /* Verificar alertas */
                if (health.free_heap < 30000)
                {
                    ESP_LOGW(TAG, "Alerta: Memoria heap abaixo de 30KB!");
                }

                if (health.wifi_rssi < -80)
                {
                    ESP_LOGW(TAG, "Alerta: Sinal WiFi fraco!");
                }
            }
        }
        else
        {
            ESP_LOGW(TAG, "MQTT: Desconectado");
            ESP_LOGI(TAG, "Sistema tentando reconectar automaticamente...");
        }

        ESP_LOGI(TAG, "════════════════════════════════════════");
        ESP_LOGI(TAG, "");
    }

    /* Task nunca deve chegar aqui */
    vTaskDelete(NULL);
}
