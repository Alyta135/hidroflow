# Documentación Técnica y Arquitectura de Firmware: HidroFlow Gamma

**Versión:** v0.01.00  
**Proyecto:** Sistema de Monitoreo Agrícola Inteligente para Cultivos  
**Plataforma Hardware:** ESP32 Microcontroller (NodeMCU ESP-WROOM-32)  
**Entorno de Desarrollo:** Arduino IDE / C++ / ESP-IDF Wrapper  
**Zona Horaria:** UTC-5 (Colombia / Bogotá)

## 1. Resumen Ejecutivo y Arquitectura del Sistema

El firmware HidroFlow Gamma v0.01.00 ha sido diseñado para operar como una estación IoT autónoma de monitoreo ambiental y edáfico. El sistema recopila datos de temperatura, humedad relativa ambiente, humedad de suelo y radiación solar, procesando métricas en tiempo real e integrando almacenamiento no volátil (NVS) para mantener un historial continuo de hasta 8 horas (48 bloques de 10 minutos).

## 2. Asignación de Hardware y Distribución de GPIOs

| Componente / Sensor | Pin GPIO | Tipo de Interfaz | Mecanismo de Procesamiento |
| --- | --- | --- | --- |
| Sensor DHT11 | GPIO 4 | Digital (One-Wire) | Lectura asíncrona de temperatura y humedad relativa |
| Sensor Humedad de Suelo | GPIO 34 | Analógica (ADC1_CH6) | Oversampling de 16 muestras y mapeo relativo |
| Fotorresistencia (LDR) | GPIO 32 | Analógica (ADC1_CH4) | Oversampling de 16 muestras y conversión a porcentaje |

## 3. Análisis de Funciones Clave del Firmware

### 3.1. Módulo de Procesamiento Asíncrono de Sensores

El bucle principal (`loop()`) delega el sensado a la función `readSensorsAsync()`, la cual evita el uso de bloqueos `delay()` al verificar lapsos mediante `millis()` (intervalo de 3000 ms). Las lecturas analógicas implementan filtrado por promedio (`readAnalogAverage`) tomando 16 muestras por ciclo para mitigar el ruido electromagnético del ADC del ESP32.

### 3.2. Gestión de Memoria NVS y Buffer Circular

Para la persistencia histórica de datos, se define la estructura `HistoryRecord` de 48 bytes por bloque:

```cpp
struct HistoryRecord {
    char timeStr[9];
    unsigned long timestampSec;
    float tempAvg, tempMin, tempMax;
    float humAvg, humMin, humMax;
    float sueloAvg, sueloMin, sueloMax;
    float luzAvg, luzMin, luzMax;
    int samplesCount;
};
```

Cada 10 minutos (`HISTORY_INTERVAL_MS = 600000UL`), la función `processTenMin()` calcula promedios, mínimos y máximos del intervalo y almacena el registro en la memoria Flash mediante la librería `Preferences` en el namespace `"hidro_h"`.

### 3.3. Servidor Web e Interfaz HTML PROGMEM

La interfaz gráfica se almacena en la memoria flash (`HTML_INDEX[] PROGMEM`), lo que ahorra ~9 KB de memoria RAM (DRAM). Además, la librería de graficado Chart.js v4.4.0 se importa dinámicamente vía CDN en el cliente (navegador), eliminando el procesamiento gráfico pesado en el microcontrolador.

## 4. Endpoints de la API REST HTTP

| Método | Ruta | Descripción |
| --- | --- | --- |
| GET | `/` | Entrega la interfaz del dashboard |
| GET | `/api/data` | Devuelve las lecturas en tiempo real |
| GET | `/api/history` | Exporta el arreglo histórico |
| GET | `/reset_wifi` | Formatea credenciales Wi-Fi y reinicia el hardware |

## 5. Consideraciones de Red y Energía

- Gestión Wi‑Fi: integra WiFiManager para aprovisionamiento por punto de acceso local.
- Rendimiento de transmisión: se inhabilita el ahorro de energía del módem Wi‑Fi con `esp_wifi_set_ps(WIFI_PS_NONE)` y se ajusta la potencia a `WIFI_POWER_19_5dBm` para garantizar máxima estabilidad de enlace.
- Reloj en tiempo real (NTP): sincronización automática con `pool.ntp.org` y `time.nist.gov` configurada para el offset de Colombia (UTC‑5 / -18000 segundos).

## 6. Resumen técnico final

HidroFlow Gamma combina sensado ambiental, dashboard local, almacenamiento persistente y gestión de conectividad Wi‑Fi para ofrecer una solución valiosa para el monitoreo de plantas, cultivos domésticos o pequeños invernaderos. Su diseño prioriza estabilidad, bajo consumo, fácil despliegue y capacidad de análisis en campo.
