/**
 * @file system_monitor_task.h
 * @brief Task de monitoramento do sistema
 *
 * Esta task é responsável por monitorar periodicamente:
 * - Status de conectividade MQTT
 * - Estatísticas de mensagens
 * - Saúde do sistema (heap, WiFi, uptime)
 * - Emitir alertas quando necessário
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

#ifndef SYSTEM_MONITOR_TASK_H
#define SYSTEM_MONITOR_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * =============================================================================
 * CONFIGURAÇÕES DA TASK
 * =============================================================================
 */

/** @brief Intervalo de monitoramento em milissegundos (1 minuto) */
#define MONITOR_INTERVAL_MS 60000

/** @brief Tamanho da stack da task em bytes */
#define MONITOR_TASK_STACK_SIZE 3072

/** @brief Prioridade da task de monitoramento */
#define MONITOR_TASK_PRIORITY 3

/** @brief Nome da task para debug */
#define MONITOR_TASK_NAME "SystemMonitor"

/*
 * =============================================================================
 * PROTÓTIPOS DE FUNÇÕES
 * =============================================================================
 */

/**
 * @brief Função da task de monitoramento do sistema
 *
 * Esta task executa periodicamente verificações de:
 * - Conectividade MQTT
 * - Estatísticas de comunicação
 * - Status de saúde do sistema
 * - Alertas de memória e WiFi
 *
 * @param pvParameters Parâmetros passados para a task (não utilizado)
 */
void system_monitor_task(void *pvParameters);

#endif /* SYSTEM_MONITOR_TASK_H */
