# Proyecto Final — Sistemas Operativos en Tiempo Real

**FI UNAM · 2026-2 · Prof. Larry Escobar**

Sistema multiproceso que comunica una tarjeta **ESP32 dual-core** (FreeRTOS) con una **Raspberry Pi 4B** (Python) vía UART serie. El ESP32 controla motores, servo, LEDs y sensores; la Raspberry Pi actúa como espejo en tiempo real y grafica la telemetría recibida.

---

## Arquitectura del sistema

```
┌──────────────────────────────────────────┐          ┌──────────────────────────┐
│              ESP32 (FreeRTOS)            │          │      Raspberry Pi 4B     │
│                                          │          │         (Python)         │
│  Core 1 — tiempo real                    │  UART    │                          │
│    • PWM software motor DC               │ ──────►  │  • Espejo motor/servo    │
│    • PWM software servomotor             │ ◄──────  │  • Espejo LEDs/display   │
│    • 4 puertos touch (vel y ángulo)      │  STOP    │  • Gnuplot: xs(n), xT(n) │
│    • Contador de LEDs (sentido servo)    │          │  • Control STOP local    │
│                                          │          └──────────────────────────┘
│  Core 0 — servicios                      │
│    • Sensores: DHT (temp/humedad) + ADC  │
│    • Display LCD (I²C)                   │
│    • Generación señal xs(n), M=1000      │
│    • Comm UART: stream estado + bloques  │
└──────────────────────────────────────────┘
```

La UART es **bidireccional**: el ESP32 envía un stream continuo de estado (velocidad, ángulo, sentido) para el espejo en tiempo real, más bloques de 1000 muestras (`xs(n)` y `xT(n)`) cada 30 s o por push-button. La Raspberry Pi envía de vuelta el comando `STOP` para detener motores y servo en ambas tarjetas.

---

## Hardware

| Componente              | Descripción                                      |
|-------------------------|--------------------------------------------------|
| ESP32 dual-core         | FreeRTOS, ESP-IDF v5.x                           |
| Raspberry Pi 4B         | Raspbian, Python 3, pigpio                       |
| Motor DC + driver       | Controlado por PWM software (GPIO ESP32)         |
| Servomotor              | PWM software 50 Hz, 0–180°                       |
| 4 LEDs                  | Contador binario sincronizado con servo          |
| 4 puertos touch         | ESP32 capacitivos (vel+/vel−, ang+/ang−)         |
| Push-button             | ISR GPIO → envío de bloques por UART             |
| Sensor DHT              | Temperatura + humedad (display y xT log)         |
| Sensor analógico temp   | LM35/TMP36 → ADC ESP32 → buffer xT(n), M=1000   |
| LCD de caracteres       | HD44780 vía backpack I²C (PCF8574)               |

---

## Stack de software

- **ESP32**: ESP-IDF v5.x, FreeRTOS (tareas, mutex, semáforos, notificaciones, software timers)
- **Raspberry Pi**: Python 3, `pigpio`, `pyserial`, `Gnuplot`

---

## Señal sinusoidal xs(n)

```
xs(n) = sin(2π · fo · n / fs)    n = 0, 1, …, 999

  fo = 10 Hz     fs = 1000 Hz     M = 1000 muestras
  → 100 muestras/ciclo · 10 ciclos completos · ventana de 1 s
```

---

## Compilar y flashear (ESP32)

Requiere [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/) instalado y el entorno activado.

```bash
# Configurar el target
idf.py set-target esp32

# Compilar
idf.py build

# Flashear y abrir monitor serie (115200 baud)
idf.py flash monitor
```

---

## Estado del desarrollo

| Paso | Descripción                                    | Estado      |
|------|------------------------------------------------|-------------|
| 1    | Esqueleto: tareas en ambos cores + estado      | ✅ Completo  |
| 2    | PWM software motor DC y servomotor             | 🔲 Pendiente |
| 3    | Puertos touch + rampas monótonas               | 🔲 Pendiente |
| 4    | Contador de LEDs (sentido servo)               | 🔲 Pendiente |
| 5    | Sensores DHT + ADC temperatura                 | 🔲 Pendiente |
| 6    | Display LCD                                    | 🔲 Pendiente |
| 7    | Generación xs(n)                               | 🔲 Pendiente |
| 8    | UART: stream de estado + bloques               | 🔲 Pendiente |
| 9    | RPI: RX serial + espejo de actuadores          | 🔲 Pendiente |
| 10   | RPI: Gnuplot + control STOP bidireccional      | 🔲 Pendiente |

---

## Estructura del repositorio

```
.
├── main/
│   ├── main.c          # Código principal ESP32 (FreeRTOS)
│   └── CMakeLists.txt
├── rpi/                # (por agregar) Scripts Python Raspberry Pi
├── CMakeLists.txt      # CMake raíz del proyecto ESP-IDF
├── .gitignore
└── README.md
```