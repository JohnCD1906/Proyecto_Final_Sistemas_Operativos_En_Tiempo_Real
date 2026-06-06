/*
 * swpwm.h - PWM de usuario por software, base de tiempo en GPTimer.
 *
 * Genera dos canales PWM (motor de CD y servomotor) con el esquema de
 * doble alarma: cada canal usa un GPTimer de proposito general que solo
 * se dispara en el flanco de subida y en el de bajada (2 ISRs por periodo).
 * No se usa LEDC: la logica del PWM la implementa el usuario en software.
 *
 * Proyecto Final - Sistemas Operativos en Tiempo Real, FI UNAM 2026-2
 */
#pragma once
#include <stdint.h>

/* ===== Pines (AJUSTA a tu cableado) ===== */
#define SWPWM_MOTOR_GPIO        25
#define SWPWM_SERVO_GPIO        26

/* ===== Parametros de tiempo (microsegundos) ===== */
#define SWPWM_MOTOR_PERIOD_US   1000    /* 1 kHz para el motor de CD     */
#define SWPWM_SERVO_PERIOD_US   20000   /* 50 Hz para el servo           */
#define SWPWM_SERVO_MIN_US      1000    /* ancho de pulso a   0 grados   */
#define SWPWM_SERVO_MAX_US      2000    /* ancho de pulso a 180 grados   */

/* Inicializa GPIOs y los dos GPTimers, y arranca el PWM. */
void swpwm_init(void);

/* Fija el ciclo de trabajo del motor: 0..100 (%). */
void swpwm_motor_set_duty(uint8_t percent);

/* Fija el angulo del servo: 0..180 (grados). */
void swpwm_servo_set_angle(uint8_t degrees);