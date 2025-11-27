# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS ${app_sources}
    # Componentes do ESP-IDF necessários
    REQUIRES 
        mqtt           # Cliente MQTT
        nvs_flash      # Non-Volatile Storage
        esp_wifi       # Driver WiFi
        esp_event      # Sistema de eventos
        esp_netif      # Interface de rede
)
# idf_component_register(SRCS "desafio2.c" INCLUDE_DIRS ".")

# Registrar componente principal
# idf_component_register(
#     # Arquivos fonte (.c) a serem compilados
#     SRCS 
#         "main.c"
#         "services/mqtt_system.c"
    
#     # Diretórios com headers (.h) a serem incluídos
#     INCLUDE_DIRS 
#         "."
    
#     # Componentes do ESP-IDF necessários
#     REQUIRES 
#         mqtt           # Cliente MQTT
#         nvs_flash      # Non-Volatile Storage
#         esp_wifi       # Driver WiFi
#         esp_event      # Sistema de eventos
#         esp_netif      # Interface de rede
# )

# Configurações adicionais de compilação (opcional)
# target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)