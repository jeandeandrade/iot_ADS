/**
 * @file custom_publish_task.h
 * @brief Task de publicação de dados customizados
 *
 * Esta task é responsável por publicar periodicamente dados
 * customizados da aplicação via MQTT, incluindo:
 * - Contadores de loops
 * - Status operacional
 * - Dados específicos da aplicação
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

#ifndef CUSTOM_PUBLISH_TASK_H
#define CUSTOM_PUBLISH_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*
 * =============================================================================
 * CONFIGURAÇÕES DA TASK
 * =============================================================================
 */

/** @brief Intervalo de publicação em milissegundos (5 minutos) */
#define CUSTOM_PUBLISH_INTERVAL_MS 300000

/** @brief Tamanho da stack da task em bytes */
#define CUSTOM_PUBLISH_TASK_STACK_SIZE 2560

/** @brief Prioridade da task de publicação */
#define CUSTOM_PUBLISH_TASK_PRIORITY 2

/** @brief Nome da task para debug */
#define CUSTOM_PUBLISH_TASK_NAME "CustomPublish"

/** @brief Tópico MQTT para publicação customizada */
#define CUSTOM_PUBLISH_TOPIC "demo/central/custom"

/*
 * =============================================================================
 * PROTÓTIPOS DE FUNÇÕES
 * =============================================================================
 */

/**
 * @brief Função da task de publicação de dados customizados
 *
 * Esta task publica periodicamente dados específicos da aplicação
 * via MQTT, permitindo monitoramento remoto do estado operacional.
 *
 * @param pvParameters Parâmetros passados para a task (não utilizado)
 */
void custom_publish_task(void *pvParameters);

#endif /* CUSTOM_PUBLISH_TASK_H */
