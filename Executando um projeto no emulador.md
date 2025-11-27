# Executando um projeto no emulador QEMU

Este projeto possui dois ambientes de compilação:

## 🖥️ Ambiente 1: Hardware Real (esp32-hardware)

Para compilar e executar em um ESP32 físico com WiFi e MQTT funcionais:

```bash
# Compilar
pio run -e esp32-hardware

# Gravar no ESP32
pio run -e esp32-hardware -t upload

# Monitor serial
pio device monitor -e esp32-hardware
```

**Recursos disponíveis:**
- ✅ WiFi completo
- ✅ MQTT funcional
- ✅ Todas as tasks (telemetria, health, watchdog)

---

## 🔧 Ambiente 2: Emulador QEMU (esp32-qemu)

Para compilar e testar no emulador QEMU (sem WiFi/MQTT):

### Passo 1: Compilar para QEMU

```bash
pio run -e esp32-qemu
```

Este comando irá:
- Compilar o código com `-DCONFIG_QEMU_MODE=1`
- Gerar a imagem: `.pio/build/esp32-qemu/qemu_flash.bin`

### Passo 2: Executar no QEMU

Na pasta raiz do projeto:

```bash
qemu-system-xtensa -nographic -machine esp32 -serial mon:stdio -drive file=.pio/build/esp32-qemu/qemu_flash.bin,if=mtd,format=raw,id=flash
```

**Recursos disponíveis no QEMU:**
- ✅ FreeRTOS e tasks funcionam
- ✅ Logs e debugging
- ✅ Lógica de aplicação (simulação de dados)
- ❌ WiFi desabilitado (hardware não emulado)
- ❌ MQTT desabilitado (depende de WiFi)
- ❌ Task de WiFi watchdog desabilitada

**Você verá nos logs:**
```
W (xxxx) MQTT_SYSTEM: FASE 2: MODO QEMU - WiFi desabilitado
W (xxxx) MQTT_SYSTEM:   Executando em emulacao, funcionalidades de rede limitadas
W (xxxx) MQTT_SYSTEM: FASE 3: MODO QEMU - MQTT desabilitado
```

### Para sair do QEMU

Pressione: `Ctrl + A`, depois `X`

---

## 📝 Notas Importantes

1. **Sempre especifique o ambiente** com `-e esp32-hardware` ou `-e esp32-qemu`
2. O QEMU é útil para testar lógica de tasks sem precisar de hardware
3. Para testes completos de IoT (WiFi/MQTT), use o ambiente `esp32-hardware`
