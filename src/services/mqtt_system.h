/**
 * @file mqtt_system.h
 * @brief Sistema IoT MQTT para ESP32 - Interface Pública
 *
 * Este header define a interface pública do sistema MQTT IoT, incluindo
 * estruturas de dados, funções de inicialização, e APIs para publicação
 * de telemetria e monitoramento.
 *
 * @author Moacyr Francischetti Correa
 * @date 2025
 */

#ifndef MQTT_SYSTEM_H
#define MQTT_SYSTEM_H

/*
 * =============================================================================
 * INCLUDES
 * =============================================================================
 */
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/*
 * =============================================================================
 * CONFIGURAÇÕES E DEFINIÇÕES PÚBLICAS
 * =============================================================================
 */

/**
 * Configurações padrão do sistema
 *
 * Estas podem ser sobrescritas via menuconfig ou definidas antes de incluir
 * este header
 */
#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID "SuaRedeWiFi"
#endif

#ifndef CONFIG_WIFI_PASSWORD
#define CONFIG_WIFI_PASSWORD "SuaSenha"
#endif

#ifndef CONFIG_MQTT_BROKER_URI
#define CONFIG_MQTT_BROKER_URI "mqtt://10.0.2.2:1883"
#endif

#ifndef CONFIG_MQTT_CLIENT_ID
#define CONFIG_MQTT_CLIENT_ID "esp32_device_001"
#endif

#ifndef CONFIG_MQTT_USERNAME
#define CONFIG_MQTT_USERNAME ""
#endif

#ifndef CONFIG_MQTT_PASSWORD
#define CONFIG_MQTT_PASSWORD ""
#endif

/* Configurações de comportamento do sistema */
#define MQTT_KEEPALIVE_SEC 60				 ///< Intervalo de keep-alive MQTT
#define MQTT_BUFFER_SIZE 2048				 ///< Tamanho do buffer MQTT
#define MQTT_TIMEOUT_MS 10000				 ///< Timeout de operações MQTT
#define WIFI_MAX_RETRY 5					 ///< Tentativas de reconexão WiFi
#define TELEMETRY_INTERVAL_MS 10000		 ///< Intervalo de telemetria
#define HEALTH_CHECK_INTERVAL_MS 60000	 ///< Intervalo de health check
#define WIFI_WATCHDOG_INTERVAL_MS 30000 ///< Intervalo de verificação WiFi

/*
 * =============================================================================
 * TIPOS E ESTRUTURAS PÚBLICAS
 * =============================================================================
 */

/**
 * @brief Estrutura de estatísticas MQTT
 *
 * Mantém contadores e métricas sobre o funcionamento do sistema MQTT
 * para monitoramento e debug.
 */
typedef struct
{
	uint32_t total_publicadas;		  ///< Total de mensagens publicadas
	uint32_t total_recebidas;		  ///< Total de mensagens recebidas
	uint32_t falhas_publicacao;	  ///< Número de falhas ao publicar
	uint32_t desconexoes;			  ///< Contador de desconexões MQTT
	uint32_t tempo_desconectado_ms; ///< Tempo total desconectado (ms)
	uint32_t ultima_mensagem_ts;	  ///< Timestamp da última mensagem (ms)
} mqtt_statistics_t;

/**
 * @brief Níveis de QoS MQTT
 *
 * Define os níveis de qualidade de serviço suportados
 */
typedef enum
{
	MQTT_QOS_0 = 0, ///< At most once - sem confirmação
	MQTT_QOS_1 = 1, ///< At least once - confirmação obrigatória
	MQTT_QOS_2 = 2	 ///< Exactly once - handshake completo
} mqtt_qos_level_t;

/**
 * @brief Estrutura de dados de telemetria
 *
 * Encapsula dados típicos de sensores para facilitar publicação
 */
typedef struct
{
	float temperatura;  ///< Temperatura em graus Celsius
	float umidade;		  ///< Umidade relativa (%)
	uint32_t contador;  ///< Contador de amostras
	uint64_t timestamp; ///< Timestamp da leitura (ms)
} telemetry_data_t;

/**
 * @brief Estrutura de health check do sistema
 *
 * Contém métricas vitais para monitoramento remoto
 */
typedef struct
{
	uint32_t free_heap;		///< Memória heap livre (bytes)
	uint32_t min_free_heap; ///< Menor heap livre desde boot (bytes)
	int wifi_rssi;				///< Força do sinal WiFi (dBm)
	uint64_t uptime_sec;		///< Tempo desde boot (segundos)
	bool mqtt_connected;		///< Status da conexão MQTT
} health_status_t;

/*
 * =============================================================================
 * FUNÇÕES PÚBLICAS - INICIALIZAÇÃO E CONTROLE
 * =============================================================================
 */

/**
 * @brief Inicializa todo o sistema IoT MQTT
 *
 * Esta função orquestra a inicialização completa do sistema:
 * - Subsistemas base (NVS, netif, event loop)
 * - WiFi (configuração e conexão)
 * - Cliente MQTT (criação e conexão)
 * - Tasks da aplicação (telemetria, health, watchdog)
 *
 * @return ESP_OK em sucesso, código de erro caso contrário
 *
 * @note Esta função bloqueia até WiFi conectar ou timeout
 * @warning Deve ser chamada apenas uma vez durante inicialização
 */
esp_err_t mqtt_system_init(void);

/**
 * @brief Desliga graciosamente o sistema MQTT
 *
 * Publica mensagem de "offline", desconecta do broker, para tasks,
 * e libera recursos alocados.
 *
 * @return ESP_OK em sucesso, código de erro caso contrário
 */
esp_err_t mqtt_system_shutdown(void);

/**
 * @brief Verifica se sistema MQTT está conectado e operacional
 *
 * @return true se MQTT conectado, false caso contrário
 */
bool mqtt_system_is_connected(void);

/*
 * =============================================================================
 * FUNÇÕES PÚBLICAS - PUBLICAÇÃO DE DADOS
 * =============================================================================
 */

/**
 * @brief Publica dados arbitrários em um tópico MQTT
 *
 * Função genérica para publicar qualquer dado em qualquer tópico.
 * Útil para publicações customizadas.
 *
 * @param topic  Tópico onde publicar (string null-terminated)
 * @param data   Dados a publicar (pode ser string, JSON, binário)
 * @param len    Comprimento dos dados (0 = usar strlen para strings)
 * @param qos    Nível de QoS (0, 1, ou 2)
 * @param retain Se true, broker mantém última mensagem para novos subscribers
 *
 * @return ID da mensagem (>= 0) em sucesso, -1 em erro
 *
 * @note Se MQTT não estiver conectado, retorna erro
 */
int mqtt_publish_data(const char *topic, const char *data,
							 int len, int qos, bool retain);

/**
 * @brief Publica dados de telemetria estruturados
 *
 * Converte estrutura telemetry_data_t em JSON e publica no tópico
 * padrão de telemetria.
 *
 * @param data Estrutura com dados de telemetria
 *
 * @return ID da mensagem (>= 0) em sucesso, -1 em erro
 */
int mqtt_publish_telemetry(const telemetry_data_t *data);

/**
 * @brief Publica health check do sistema
 *
 * Coleta métricas vitais do sistema (heap, WiFi RSSI, uptime, etc)
 * e publica no tópico de health check.
 *
 * @return ID da mensagem (>= 0) em sucesso, -1 em erro
 */
int mqtt_publish_health_check(void);

/**
 * @brief Publica mensagem de status (online/offline)
 *
 * @param online true para "online", false para "offline"
 *
 * @return ID da mensagem (>= 0) em sucesso, -1 em erro
 */
int mqtt_publish_status(bool online);

/*
 * =============================================================================
 * FUNÇÕES PÚBLICAS - SUBSCRIÇÃO
 * =============================================================================
 */

/**
 * @brief Subscreve em um tópico MQTT
 *
 * @param topic Tópico para subscrever (suporta wildcards + e #)
 * @param qos   Nível de QoS desejado
 *
 * @return ID da mensagem (>= 0) em sucesso, -1 em erro
 */
int mqtt_subscribe_topic(const char *topic, int qos);

/**
 * @brief Cancela subscrição em um tópico
 *
 * @param topic Tópico para cancelar subscrição
 *
 * @return ID da mensagem (>= 0) em sucesso, -1 em erro
 */
int mqtt_unsubscribe_topic(const char *topic);

/*
 * =============================================================================
 * FUNÇÕES PÚBLICAS - ESTATÍSTICAS E MONITORAMENTO
 * =============================================================================
 */

/**
 * @brief Obtém estatísticas atuais do sistema MQTT
 *
 * @param stats Ponteiro para estrutura onde copiar estatísticas
 *
 * @return ESP_OK em sucesso, ESP_ERR_INVALID_ARG se stats é NULL
 */
esp_err_t mqtt_get_statistics(mqtt_statistics_t *stats);

/**
 * @brief Reseta contadores de estatísticas
 *
 * Zera todos os contadores, exceto desconexões e tempo desconectado
 * que são mantidos como histórico.
 */
void mqtt_reset_statistics(void);

/**
 * @brief Obtém status de saúde atual do sistema
 *
 * @param health Ponteiro para estrutura onde copiar status
 *
 * @return ESP_OK em sucesso, ESP_ERR_INVALID_ARG se health é NULL
 */
esp_err_t mqtt_get_health_status(health_status_t *health);

/**
 * @brief Imprime estatísticas no log
 *
 * Útil para debug e monitoramento via serial
 */
void mqtt_print_statistics(void);

/*
 * =============================================================================
 * TÓPICOS MQTT PADRÃO DO SISTEMA
 * =============================================================================
 */

/** Tópico base do sistema */
#define MQTT_TOPIC_BASE "demo/central"

/** Tópico de status (online/offline) */
#define MQTT_TOPIC_STATUS MQTT_TOPIC_BASE "/status"

/** Tópico de telemetria */
#define MQTT_TOPIC_TELEMETRY MQTT_TOPIC_BASE "/telemetria"

/** Tópico de health check */
#define MQTT_TOPIC_HEALTH MQTT_TOPIC_BASE "/health"

/** Tópico de comandos recebidos */
#define MQTT_TOPIC_COMMANDS MQTT_TOPIC_BASE "/comandos"

/** Tópico de configuração */
#define MQTT_TOPIC_CONFIG MQTT_TOPIC_BASE "/config"

/** Tópico de boot/informações iniciais */
#define MQTT_TOPIC_BOOT MQTT_TOPIC_BASE "/boot"

/** Tópico de alertas/erros */
#define MQTT_TOPIC_ALERTS MQTT_TOPIC_BASE "/alertas"

#endif /* MQTT_SYSTEM_H */