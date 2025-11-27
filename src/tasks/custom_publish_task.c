/**
 * @file custom_publish_task.c
 * @brief Implementação da task de publicação de dados customizados
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

#include "tasks/custom_publish_task.h"
#include "services/mqtt_system.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_random.h"

static const char *TAG = "CUSTOM_PUB_TASK";

void custom_publish_task(void *pvParameters)
{
    uint32_t publish_count = 0;
    srand(time(NULL));

    ESP_LOGI(TAG, "Task de publicacao customizada iniciada");

    while (1)
    {
        /* Aguardar intervalo de publicação */
        vTaskDelay(pdMS_TO_TICKS(CUSTOM_PUBLISH_INTERVAL_MS));

        /* Verificar se MQTT está conectado antes de publicar */
        if (mqtt_system_is_connected())
        {
            publish_count++;

            int luminosidade = esp_random() % 11; 
            char lum_str[4];
            snprintf(lum_str, sizeof(lum_str), "%d", luminosidade);

            int ret_lum = mqtt_publish_data(
                "casa/externo/luminosidade",
                lum_str,
                0,
                1,
                false);

            if (ret_lum >= 0)
            {
                ESP_LOGI(TAG, "Luminosidade: %d (Publicado)", luminosidade);
            }
            else
            {
                ESP_LOGW(TAG, "Falha ao publicar luminosidade");
            }

            int temperatura = (esp_random() % 49) - 3;
            char temp_str[4];
            snprintf(temp_str, sizeof(temp_str), "%d", temperatura);

            int ret_temp = mqtt_publish_data(
                "casa/sala/temperatura",
                temp_str,
                0,
                1,
                false);

            if (ret_temp >= 0)
            {
                ESP_LOGI(TAG, "Temperatura: %d (Publicado)", temperatura);
            }
            else
            {
                ESP_LOGW(TAG, "Falha ao publicar temperatura");
            }

            /* Preparar mensagem customizada em formato JSON */
            char custom_msg[128];
            snprintf(custom_msg, sizeof(custom_msg),
                     "{\"publish_count\":%lu,\"status\":\"operational\"}",
                     publish_count);

            /* Publicar dados customizados */
            esp_err_t ret = mqtt_publish_data(
                CUSTOM_PUBLISH_TOPIC,
                custom_msg,
                0,      // strlen automático
                0,      // QoS 0
                false); // sem retain

            if (ret == ESP_OK)
            {
                ESP_LOGI(TAG, "Dados customizados publicados (#%lu)", publish_count);
            }
            else
            {
                ESP_LOGW(TAG, "Falha ao publicar dados customizados");
            }
        }
        else
        {
            ESP_LOGW(TAG, "MQTT desconectado, aguardando reconexao...");
        }
    }

    /* Task nunca deve chegar aqui */
    vTaskDelete(NULL);
}
