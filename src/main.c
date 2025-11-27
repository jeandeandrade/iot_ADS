/**
 * @file main.c
 * @brief Aplicação principal com arquitetura baseada em Tasks FreeRTOS
 *
 * Este arquivo contém apenas a inicialização do sistema e criação das tasks.
 * Toda a lógica está distribuída em tasks modulares e independentes:
 * - Task de monitoramento do sistema
 * - Task de publicação de dados customizados
 *
 * A biblioteca mqtt_system.h cuida da infraestrutura de comunicação IoT.
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "services/mqtt_system.h"
#include "tasks/system_monitor_task.h"
#include "tasks/custom_publish_task.h"

/*
 * =============================================================================
 * CONFIGURAÇÕES DA APLICAÇÃO
 * =============================================================================
 */

static const char *TAG = "MAIN_APP";

/*
 * =============================================================================
 * FUNÇÃO PRINCIPAL
 * =============================================================================
 */

/**
 * @brief Ponto de entrada da aplicação
 *
 * Responsável por:
 * 1. Inicializar o sistema (WiFi, MQTT, telemetria)
 * 2. Criar as tasks da aplicação usando xTaskCreate
 * 3. Retornar o controle para o FreeRTOS
 *
 * Após a inicialização, o FreeRTOS assume o controle do sistema.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔═════════════════════════════════╗");
    ESP_LOGI(TAG, "║   Sistema de Demonstracao IoT   ║");
    ESP_LOGI(TAG, "║     Baseado em ESP32 + MQTT     ║");
    ESP_LOGI(TAG, "║   Arquitetura: FreeRTOS Tasks   ║");
    ESP_LOGI(TAG, "╚═════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    /*
     * PASSO 1: Inicializar sistema completo
     *
     * Esta função cuida de:
     * - Inicialização NVS
     * - Conexão WiFi
     * - Cliente MQTT
     * - Tasks de telemetria e monitoramento
     * - Watchdog de conectividade
     */
    esp_err_t ret = mqtt_system_init();

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao inicializar sistema MQTT");
        ESP_LOGE(TAG, "Sistema nao pode continuar");
        return;
    }

    ESP_LOGI(TAG, "Sistema MQTT inicializado com sucesso");
    ESP_LOGI(TAG, "");

    /*
     * PASSO 2: Criar tasks da aplicação
     *
     * Cada task executa de forma independente e concorrente.
     * O FreeRTOS gerencia o escalonamento e prioridades.
     */

    ESP_LOGI(TAG, "Criando tasks da aplicacao...");

    /* Task 1: Monitoramento do Sistema */
    BaseType_t task_created = xTaskCreate(
        system_monitor_task,     // Ponteiro para a função da Task
        MONITOR_TASK_NAME,       // Nome da Task (para debug)
        MONITOR_TASK_STACK_SIZE, // Tamanho da Stack em bytes
        NULL,                    // Parâmetros passados (não usado)
        MONITOR_TASK_PRIORITY,   // Prioridade (0-24, maior = mais prioritário)
        NULL                     // Handle da Task (não usado)
    );

    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Falha ao criar task de monitoramento");
        return;
    }

    ESP_LOGI(TAG, "   [OK] Task: %s (Prioridade: %d)",
             MONITOR_TASK_NAME, MONITOR_TASK_PRIORITY);

    /* Task 2: Publicação de Dados Customizados */
    task_created = xTaskCreate(
        custom_publish_task,            // Ponteiro para a função da Task
        CUSTOM_PUBLISH_TASK_NAME,       // Nome da Task (para debug)
        CUSTOM_PUBLISH_TASK_STACK_SIZE, // Tamanho da Stack em bytes
        NULL,                           // Parâmetros passados (não usado)
        CUSTOM_PUBLISH_TASK_PRIORITY,   // Prioridade (0-24, maior = mais prioritário)
        NULL                            // Handle da Task (não usado)
    );

    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Falha ao criar task de publicacao customizada");
        return;
    }

    ESP_LOGI(TAG, "   [OK] Task: %s (Prioridade: %d)",
             CUSTOM_PUBLISH_TASK_NAME, CUSTOM_PUBLISH_TASK_PRIORITY);

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "  Sistema Inicializado com Sucesso!");
    ESP_LOGI(TAG, "════════════════════════════════════════");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Funcionalidades ativas:");
    ESP_LOGI(TAG, "   - Telemetria automatica a cada %d segundos",
             TELEMETRY_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "   - Health check a cada %d segundos",
             HEALTH_CHECK_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "   - Watchdog WiFi monitorando conectividade");
    ESP_LOGI(TAG, "   - Monitoramento do sistema a cada %d segundos",
             MONITOR_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "   - Publicacao customizada a cada %d segundos",
             CUSTOM_PUBLISH_INTERVAL_MS / 1000);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Tasks criadas: 2");
    ESP_LOGI(TAG, "   1. %s (P%d)", MONITOR_TASK_NAME, MONITOR_TASK_PRIORITY);
    ESP_LOGI(TAG, "   2. %s (P%d)", CUSTOM_PUBLISH_TASK_NAME, CUSTOM_PUBLISH_TASK_PRIORITY);
    ESP_LOGI(TAG, "");

    /*
     * PASSO 3: app_main termina, FreeRTOS assume controle
     *
     * A partir daqui, o scheduler do FreeRTOS gerencia a execução
     * das tasks criadas conforme suas prioridades e estados.
     *
     * As tasks continuam executando indefinidamente em seus loops.
     */

    ESP_LOGI(TAG, "app_main() finalizando...");
    ESP_LOGI(TAG, "FreeRTOS scheduler assumiu o controle");
    ESP_LOGI(TAG, "");

    /* app_main retorna, mas as tasks continuam executando */
}
