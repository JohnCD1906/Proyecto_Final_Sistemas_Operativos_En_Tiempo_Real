/*
 * touch_pads.c - Driver legacy de touch (ESP-IDF v6, ESP32 V1).
 *
 * El driver nuevo (driver/touch_sens.h) se atasca en V1 con el filtro IIR;
 * el legacy (driver/touch_sensor_legacy.h) sigue en v6 y funciona bien.
 */
#include "touch_pad.h"
#include "driver/touch_sensor_legacy.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "TOUCH";

#define TOUCH_FILTER_PERIOD_MS  10

static void registrar(int ch)
{
    /* umbral 0: no usamos interrupcion por hardware, leemos manual */
    touch_pad_config(ch, 0);
}

void touch_pads_init(void)
{
    ESP_ERROR_CHECK(touch_pad_init());

    /* Referencias: alto 2.7 V, bajo 0.5 V, atenuacion 1 V */
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);

    registrar(TOUCH_CH_MOTOR_UP);
    registrar(TOUCH_CH_MOTOR_DOWN);
    registrar(TOUCH_CH_SERVO_UP);
    registrar(TOUCH_CH_SERVO_DOWN);

    touch_pad_filter_start(TOUCH_FILTER_PERIOD_MS);

    ESP_LOGI(TAG, "Touch listo (T%d T%d T%d T%d, thr=%d)",
             TOUCH_CH_MOTOR_UP, TOUCH_CH_MOTOR_DOWN,
             TOUCH_CH_SERVO_UP, TOUCH_CH_SERVO_DOWN, TOUCH_DETECT_THR);

    /* Deja que el filtro se asiente antes de leer */
    vTaskDelay(pdMS_TO_TICKS(500));
}

uint32_t touch_get_value(int channel)
{
    if (channel < 0 || channel > 9) return 0;
    uint16_t v = 0;
    touch_pad_read_filtered(channel, &v);
    return (uint32_t)v;
}

bool touch_is_pressed(int channel)
{
    uint32_t v = touch_get_value(channel);
    /* En V1 el valor baja al tocar. v=0 => lectura aun no valida. */
    return (v > 10) && (v < TOUCH_DETECT_THR);
}