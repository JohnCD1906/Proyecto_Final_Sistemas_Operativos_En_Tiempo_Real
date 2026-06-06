/*
 * touch_pads.h - Lectura de los 4 pads capacitivos (driver legacy, V1).
 *
 * Reutiliza la configuracion ya probada: el valor DISMINUYE al tocar.
 * Dos pads suben/bajan la velocidad del motor y dos el angulo del servo.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Canales touch (numero de canal, no GPIO). El driver V1 acepta 0-9. */
#define TOUCH_CH_MOTOR_UP     9    /* GPIO 32 */
#define TOUCH_CH_MOTOR_DOWN   4    /* GPIO 13 */
#define TOUCH_CH_SERVO_UP     6    /* GPIO 14 */
#define TOUCH_CH_SERVO_DOWN   7    /* GPIO 27 */

/* Umbral sobre el valor filtrado.
 *   Pad libre  ~ 700 - 1500
 *   Pad tocado ~ 100 - 400
 * Calibra a la mitad entre tu valor libre y el tocado (ver logs). */
#define TOUCH_DETECT_THR      500

void     touch_pads_init(void);              /* init + filtro + espera */
uint32_t touch_get_value(int channel);       /* valor filtrado (calibrar) */
bool     touch_is_pressed(int channel);      /* true si esta tocado */