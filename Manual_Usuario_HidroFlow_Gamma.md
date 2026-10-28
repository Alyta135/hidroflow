# Documentación Técnica y Operativa

## HidroFlow Gamma (v0.01.00)

### Manual de Usuario, Auditoría de Código y Guía de Despliegue en Campo

- Plataforma: ESP32 Microcontroller
- Zona horaria: UTC-5 (Colombia / Bogotá)
- Ecosistema: Monitoreo Agrícola Inteligente
- Persistencia: Memoria NVS Flash (48 registros)

## Diagnóstico y Evaluación Técnica del Código

El código revisado para el firmware HidroFlow Gamma v0.01.00 presenta un diseño de alta eficiencia adaptado para arquitecturas embebidas sin SO de tiempo real estricto (RTOS directo). Destacan las siguientes características:

- Eficiencia web y no bloqueante: Uso de `PROGMEM` para almacenar la interfaz HTML evitando fragmentación de RAM e inclusión asíncrona de Chart.js vía CDN.
- Procesamiento multitarea asíncrono: Implementación de temporización mediante `millis()` en `readSensorsAsync()` sin uso de `delay()` bloqueantes.
- Resiliencia de datos (NVS): Almacenamiento no volátil del historial circular (48 muestras de 10 minutos) mediante la librería `Preferences`.
- Optimización de red: Desactivación de ahorro de energía Wi‑Fi (`WIFI_PS_NONE`) y transmisión en máxima potencia (19.5 dBm) para prevenir desconexiones en entornos de campo o invernaderos.

## 1. Guía de Conexión de Hardware y Sensórica

El sistema interactúa con cuatro variables físicas principales mediante la siguiente asignación de pines GPIO en la placa ESP32:

| Componente / Sensor | Pin GPIO (ESP32) | Tipo de señal | Rango / calibración |
| --- | --- | --- | --- |
| Sensor DHT11 (temperatura / humedad ambiente) | GPIO 4 | Digital (One-Wire) | Temperatura 0–50 °C; humedad 20–90 % |
| Fotorresistencia (LDR) | GPIO 32 | Analógica (ADC1_CH4) | Mapeo de 0 a 4095 → 0 % a 100 % |
| Sensor de humedad de suelo | GPIO 34 | Analógica (ADC1_CH6) | Seco: 4095 raw (0 %); húmedo: 1200 raw (100 %) |

### Nota de calibración de campo (humedad del suelo)

Si utiliza un sensor capacitivo de humedad de suelo, se recomienda ajustar las constantes `SOIL_DRY_RAW` y `SOIL_WET_RAW` en la cabecera del código midiendo los valores analógicos directos con el sensor completamente seco al aire y sumergido en agua.

## 2. Puesta en Marcha y Configuración Wi‑Fi

1. Encendido inicial: al energizar el dispositivo, el ESP32 inicia sus módulos.
2. Apertura del portal de configuración: si no hay una red conocida o al iniciar por primera vez, el ESP32 emitirá un punto de acceso llamado `ESP32_Config_AP` con contraseña `12345678`.
3. Asignación de red:
   - Conéctese desde su teléfono o computadora a la red `ESP32_Config_AP`.
   - El portal se abrirá en `http://192.168.4.1`.
   - Seleccione su red Wi‑Fi local/invernadero e ingrese la contraseña.
4. Obtención de dirección IP: una vez conectado, el monitor serie imprimirá la IP asignada. Acceda a esta IP desde cualquier navegador en la misma red.

## 3. Operación del Dashboard Web (Interfaz de Usuario)

La interfaz de usuario del dashboard en vivo incluye múltiples indicadores de salud agronómica que se actualizan automáticamente cada 5 segundos:

| Métrica | Estado | Rango evaluado | Acción / significado |
| --- | --- | --- | --- |
| Humedad de suelo | ¡Riego urgente! | < 20 % | Estrés hídrico severo |
| Humedad de suelo | Humedad baja | 20–39 % | Zona de pre-riego |
| Humedad de suelo | Óptimo | 40–75 % | Nivel ideal |
| Humedad de suelo | Saturado | > 75 % | Riesgo de anoxia |
| Temperatura ambiente | Frío | < 15 °C | Crecimiento lento |
| Temperatura ambiente | Óptimo | 15–30 °C | Rango ideal |
| Temperatura ambiente | Calor elevado | > 30 °C | Riesgo de deshidratación |
| Humedad relativa | Ambiente seco | < 30 % | Alta evaporación |
| Humedad relativa | Óptimo | 30–70 % | Balance adecuado |
| Humedad relativa | Ambiente húmedo | > 70 % | Riesgo de fitopatógenos |
| Luz solar | Sombra | < 20 % | Baja luz activa |
| Luz solar | Luz adecuada | 20–80 % | Iluminación óptima |
| Luz solar | Sol intenso | > 80 % | Radiación alta |

## 4. Funcionalidades Avanzadas e Historial de Datos

- Módulo de tendencias e historial: al hacer clic en `📊 Gráfica e Historial`, se despliega una ventana flotante con gráfica interactiva y tabla con registros promediados cada 10 minutos.
- Sincronización NTP: hora oficial sincronizada con UTC‑5.
- Reconfiguración de red: el botón `⚙️ Reconfigurar Wi‑Fi` borra las credenciales y reinicia el portal.

## 5. Referencia API REST HTTP (End-Points)

El sistema expone endpoints JSON que permiten integrar HidroFlow Gamma con plataformas externas:

### `GET /api/data`

Retorna el estado en tiempo real de todos los sensores en formato JSON.

```json
{
  "luz": 85,
  "suelo": 62.4,
  "temperatura": 22.5,
  "humedad_amb": 58.0,
  "sensor_ok": true
}
```

### `GET /api/history`

Retorna el arreglo completo de hasta 48 bloques históricos de 10 minutos con promedios, máximos, mínimos y estampa de tiempo.

### `GET /reset_wifi`

Ejecuta el formateo de credenciales de red y reinicio remoto del hardware.

# Documentación Técnica y Arquitectura de Firmware: HidroFlow Gamma

**Versión:** v0.01.00  
**Proyecto:** Sistema de Monitoreo Agrícola Inteligente para Cultivos  
**Plataforma Hardware:** ESP32 Microcontroller (NodeMCU ESP-WROOM-32)  
**Entorno de Desarrollo:** Arduino IDE / C++ / ESP-IDF Wrapper  
**Zona Horaria:** UTC-5 (Colombia / Bogotá)

## 1. Resumen Ejecutivo y Arquitectura del Sistema

El firmware HidroFlow Gamma v0.01.00 está diseñado para operar como una estación IoT autónoma de monitoreo ambiental y edáfico. El sistema recopila datos de temperatura, humedad relativa ambiente, humedad de suelo y radiación solar, procesando métricas en tiempo real e integrando almacenamiento no volátil (NVS) para mantener un historial continuo de hasta 8 horas (48 bloques de 10 minutos).

## 2. Asignación de Hardware y Distribución de GPIOs

| Componente / Sensor | Pin GPIO | Tipo de Interfaz | Mecanismo de Procesamiento |
| --- | --- | --- | --- |
| Sensor DHT11 | GPIO 4 | Digital (One-Wire) | Lectura asíncrona de temperatura y humedad relativa |
| Sensor Humedad de Suelo | GPIO 34 | Analógica (ADC1_CH6) | Oversampling de 16 muestras y mapeo relativo |
| Fotorresistencia (LDR) | GPIO 32 | Analógica (ADC1_CH4) | Oversampling de 16 muestras y conversión a porcentaje |

## 3. Análisis de Funciones Clave del Firmware

### 3.1. Módulo de Procesamiento Asíncrono de Sensores

El bucle principal delega el sensado a `readSensorsAsync()`, la cual evita bloqueos con `delay()` usando `millis()`. Las lecturas analógicas implementan filtrado por promedio con `readAnalogAverage`, tomando 16 muestras por ciclo para mitigar el ruido del ADC y asegurar estabilidad.

### 3.2. Gestión de Memoria NVS y Buffer Circular

Para la persistencia histórica, se define la estructura `HistoryRecord`:

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

Cada 10 minutos la función `processTenMin()` calcula promedios, mínimos y máximos del intervalo y almacena el registro en memoria Flash usando la librería `Preferences` en el namespace `hidro_h`.

### 3.3. Servidor Web e Interfaz HTML PROGMEM

La interfaz gráfica se almacena en la memoria flash (`HTML_INDEX[] PROGMEM`), reduciendo uso de RAM. Además, el cliente carga Chart.js desde CDN, evitando procesos pesados en el microcontrolador.

## 4. Endpoints de la API REST HTTP

| Método | Ruta | Descripción |
| --- | --- | --- |
| GET | `/` | Entrega la interfaz del dashboard |
| GET | `/api/data` | Devuelve lecturas en tiempo real |
| GET | `/api/history` | Exporta el arreglo histórico |
| GET | `/reset_wifi` | Formatea credenciales Wi‑Fi y reinicia el hardware |

## 5. Consideraciones de Red y Energía

- Wi‑Fi: integración con WiFiManager para aprovisionamiento por punto de acceso local.
- Rendimiento de transmisión: deshabilitado el ahorro de energía con `WIFI_PS_NONE` y potencia ajustada a `WIFI_POWER_19_5dBm`.
- Reloj en tiempo real (NTP): sincronización con `pool.ntp.org` y `time.nist.gov`, con ajuste `UTC-5`.

## 6. Resumen técnico final

HidroFlow Gamma combina sensado ambiental, dashboard local, almacenamiento persistente y gestión de conectividad Wi‑Fi para ofrecer una solución valiosa para el monitoreo de plantas, cultivos domésticos o pequeños invernaderos. Su diseño prioriza estabilidad, bajo consumo, fácil despliegue y capacidad de análisis en campo.
