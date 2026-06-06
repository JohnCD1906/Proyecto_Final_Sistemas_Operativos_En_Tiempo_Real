/*
 * Proyecto 4 - Sistemas Operativos en Tiempo Real, FI UNAM 2026-2
 * Esqueleto ESP32 (ESP-IDF v5.x / FreeRTOS) - Paso 1: estructura de tareas.
 *
 * Reparto de cores:
 *   Core 1 (APP_CPU) -> tiempo real: touch, PWM motor, PWM servo, LEDs
 *   Core 0 (PRO_CPU) -> servicios:   sensores, display, senal xs(n), UART
 *
 * En este paso las tareas solo "laten" (ESP_LOGI) para validar el reparto
 * entre cores. El estado del sistema vive en un struct protegido por mutex;
 * la logica real de cada tarea se ira llenando en los pasos siguientes.
 */

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "swpwm.h"
#include "touch_pad.h"


#define TOUCH_POLL_MS    80
#define VEL_STEP          5     /* % de velocidad por pulso de poll */
#define ANG_STEP          5     /* grados de servo por pulso de poll */
#define TOUCH_CALIBRATE   0     /* 1 = imprime RAW cada ~1 s para calibrar THR */


static const char *TAG = "PROY4";

/* ------------------------------------------------------------------ */
/*  Estado compartido del sistema                                      */
/* ------------------------------------------------------------------ */
typedef enum {
    SENTIDO_CW  = 0,   /* horario     -> LEDs cuentan hacia arriba */
    SENTIDO_CCW = 1    /* antihorario -> LEDs cuentan hacia abajo  */
} sentido_t;

typedef struct {
    uint8_t   vel_motor;    /* duty del motor de DC, 0..255          */
    uint16_t  ang_servo;    /* angulo del servo, 0..180 grados       */
    sentido_t sentido;      /* sentido de giro actual del servo      */
    float     temperatura;  /* C (sensor analogico por ADC / DHT)    */
    float     humedad;      /* % HR (DHT)                            */
    bool      stop;         /* paro de motor y servo (local o RPI)   */
} estado_sistema_t;

static estado_sistema_t  g_estado;
static SemaphoreHandle_t g_estado_mux;

/* Copia atomica del estado: las tareas lectoras (display, comm) trabajan
 * sobre un snapshot consistente sin retener el mutex mas de lo necesario. */
static estado_sistema_t estado_leer(void)
{
    estado_sistema_t copia;
    xSemaphoreTake(g_estado_mux, portMAX_DELAY);
    copia = g_estado;
    xSemaphoreGive(g_estado_mux);
    return copia;
}

/* ------------------------------------------------------------------ */
/*  Tareas - Core 1 (tiempo real)                                      */
/* ------------------------------------------------------------------ */

/* Lee los 4 puertos touch y ajusta monotonamente vel_motor (+/-) y
 * ang_servo (+/-). Al soltar el puerto, el valor se congela.
 *
 * Patron de escritura segura que usaran estas tareas:
 *     xSemaphoreTake(g_estado_mux, portMAX_DELAY);
 *     g_estado.vel_motor = nuevo_valor;
 *     xSemaphoreGive(g_estado_mux);
 */
static void tarea_touch(void *arg)
{
    ESP_LOGI(TAG, "[C1] touch: control de velocidad y angulo");
    int dbg = 0;
    for (;;) {
        bool m_up = touch_is_pressed(TOUCH_CH_MOTOR_UP);
        bool m_dn = touch_is_pressed(TOUCH_CH_MOTOR_DOWN);
        bool s_up = touch_is_pressed(TOUCH_CH_SERVO_UP);
        bool s_dn = touch_is_pressed(TOUCH_CH_SERVO_DOWN);

#if TOUCH_CALIBRATE
        if (++dbg >= (1000 / TOUCH_POLL_MS)) {
            dbg = 0;
            ESP_LOGI(TAG, "RAW  M+:%4u  M-:%4u  S+:%4u  S-:%4u  (thr=%d)",
                     (unsigned)touch_get_value(TOUCH_CH_MOTOR_UP),
                     (unsigned)touch_get_value(TOUCH_CH_MOTOR_DOWN),
                     (unsigned)touch_get_value(TOUCH_CH_SERVO_UP),
                     (unsigned)touch_get_value(TOUCH_CH_SERVO_DOWN),
                     TOUCH_DETECT_THR);
        }
#endif
        /* Lee-modifica-escribe en una sola seccion critica: respeta cambios
         * externos (p.ej. el STOP del Paso 10 que pondra vel_motor=0). */
        if (xSemaphoreTake(g_estado_mux, portMAX_DELAY) == pdTRUE) {
            int vel = g_estado.vel_motor;
            int ang = g_estado.ang_servo;
            if (m_up) vel += VEL_STEP;
            if (m_dn) vel -= VEL_STEP;
            if (s_up) ang += ANG_STEP;
            if (s_dn) ang -= ANG_STEP;
            if (vel < 0)   vel = 0;
            if (vel > 100) vel = 100;
            if (ang < 0)   ang = 0;
            if (ang > 180) ang = 180;
            g_estado.vel_motor = vel;
            g_estado.ang_servo = ang;
            xSemaphoreGive(g_estado_mux);
        }
        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_MS));
    }
}

#define SWPWM_SELFTEST 0 

/* Genera el PWM del motor de DC por software. */
static void tarea_pwm_motor(void *arg)
{
    uint8_t d = 0;
    for (;;) {
#if SWPWM_SELFTEST
        swpwm_motor_set_duty(d);
        d = (d >= 100) ? 0 : d + 10;
#else
        swpwm_motor_set_duty(estado_leer().vel_motor);
#endif
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* Genera el PWM del servo por software. */
static void tarea_pwm_servo(void *arg)
{
    int ang = 0, paso = 10;
    for (;;) {
#if SWPWM_SELFTEST
        swpwm_servo_set_angle((uint8_t)ang);
        ang += paso;
        if (ang >= 180 || ang <= 0) paso = -paso;
#else
        swpwm_servo_set_angle(estado_leer().ang_servo);
#endif
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* 4 LEDs que cuentan hacia arriba/abajo segun el sentido del servo,
 * sincronizados via notificacion desde tarea_pwm_servo. */
static void tarea_leds(void *arg)
{
    for (;;) {
        ESP_LOGI(TAG, "[C1] leds");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ------------------------------------------------------------------ */
/*  Tareas - Core 0 (servicios)                                        */
/* ------------------------------------------------------------------ */

/* Lee el DHT (temp + humedad) y el ADC de temperatura para el buffer xT(n). */
static void tarea_sensores(void *arg)
{
    for (;;) {
        ESP_LOGI(TAG, "[C0] sensores");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Despliega en el LCD: velocidad del motor, angulo del servo,
 * humedad y temperatura. */
static void tarea_display(void *arg)
{
    for (;;) {
        estado_sistema_t e = estado_leer();
        ESP_LOGI(TAG, "[C0] display  vel=%u ang=%u",
                 e.vel_motor, e.ang_servo);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Genera la senal sinusoidal xs(n): M=1000, fo=10 Hz, fs=1 kHz. */
static void tarea_senal(void *arg)
{
    for (;;) {
        ESP_LOGI(TAG, "[C0] senal xs(n)");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* UART hacia la RPI: stream de estado continuo (espejo) + bloques
 * xs(n)/xT(n) cada 30 s o por push-button. Recibe el comando STOP. */
static void tarea_comm(void *arg)
{
    for (;;) {
        ESP_LOGI(TAG, "[C0] comm uart");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ------------------------------------------------------------------ */
/*  Punto de entrada                                                   */
/* ------------------------------------------------------------------ */
void app_main(void)
{
    g_estado_mux = xSemaphoreCreateMutex();
    g_estado = (estado_sistema_t){
        .vel_motor   = 0,
        .ang_servo   = 0,
        .sentido     = SENTIDO_CW,
        .temperatura = 0.0f,
        .humedad     = 0.0f,
        .stop        = false,
    };
    
    swpwm_init();
    touch_pads_init();      

    ESP_LOGI(TAG, "Iniciando Proyecto 4 (esqueleto)");

    /* Core 1 (APP_CPU) - tareas de tiempo real (prioridad alta) */
    xTaskCreatePinnedToCore(tarea_pwm_motor, "pwm_motor", 2048, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(tarea_pwm_servo, "pwm_servo", 2048, NULL, 6, NULL, 1);
    xTaskCreatePinnedToCore(tarea_touch,     "touch",     2048, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tarea_leds,      "leds",      2048, NULL, 4, NULL, 1);

    /* Core 0 (PRO_CPU) - tareas de servicio */
    xTaskCreatePinnedToCore(tarea_comm,      "comm",      4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(tarea_sensores,  "sensores",  4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(tarea_senal,     "senal",     4096, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(tarea_display,   "display",   4096, NULL, 2, NULL, 0);
}