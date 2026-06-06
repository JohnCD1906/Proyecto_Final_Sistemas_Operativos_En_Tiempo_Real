/*
 * swpwm.c - PWM de usuario por software con GPTimer (doble alarma).
 * ESP-IDF v5.x.
 */
#include "swpwm.h"
#include "driver/gptimer.h"
#include "driver/gpio.h"
#include "esp_attr.h"

/* Handles de los dos temporizadores de proposito general */
static gptimer_handle_t s_motor_tmr = NULL;
static gptimer_handle_t s_servo_tmr = NULL;

/* Setpoint (ancho de pulso en alto, en us) que escribe la tarea y lee el ISR.
 * Escritura de 32 bits = atomica en el ESP32, un solo escritor por canal. */
static volatile uint32_t s_motor_high_req = 0;                 /* arranca apagado */
static volatile uint32_t s_servo_high_req = SWPWM_SERVO_MIN_US; /* arranca en 0 grados */

/* Nivel actual del pin de cada canal (0 = bajo, 1 = alto) */
static volatile uint8_t  s_motor_level = 0;
static volatile uint8_t  s_servo_level = 0;

/* Reprograma la siguiente alarma del timer 't' dentro de 'us' microsegundos.
 * auto_reload + reload_count=0 hace que el contador vuelva a 0 tras cada
 * alarma, asi que solo hay que fijar el siguiente intervalo. */
static inline void IRAM_ATTR rearm(gptimer_handle_t t, uint32_t us)
{
    gptimer_alarm_config_t a = {
        .alarm_count = (us == 0) ? 1 : us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(t, &a);
}

/* ISR del motor: genera el PWM por doble alarma.
 *   - en flanco de bajada (estaba en alto): baja y programa el tiempo en bajo
 *   - en flanco de subida (estaba en bajo): latchea el duty, sube y programa
 *     el tiempo en alto. Casos extremos 0% / 100% se quedan en su nivel. */
static bool IRAM_ATTR motor_isr(gptimer_handle_t t,
                                const gptimer_alarm_event_data_t *e, void *ctx)
{
    uint32_t high = s_motor_high_req;                 /* latch del setpoint */
    if (high > SWPWM_MOTOR_PERIOD_US) high = SWPWM_MOTOR_PERIOD_US;
    uint32_t next;

    if (s_motor_level == 0) {                         /* inicio de periodo */
        if (high == 0) {                              /* 0%: queda en bajo */
            next = SWPWM_MOTOR_PERIOD_US;
        } else {
            gpio_set_level(SWPWM_MOTOR_GPIO, 1);
            s_motor_level = 1;
            next = high;
        }
    } else {                                          /* estaba en alto */
        uint32_t low = SWPWM_MOTOR_PERIOD_US - high;
        if (low == 0) {                               /* 100%: queda en alto */
            next = SWPWM_MOTOR_PERIOD_US;
        } else {
            gpio_set_level(SWPWM_MOTOR_GPIO, 0);
            s_motor_level = 0;
            next = low;
        }
    }
    rearm(t, next);
    return false;                                     /* no se desperto tarea */
}

/* ISR del servo: misma logica de doble alarma. */
static bool IRAM_ATTR servo_isr(gptimer_handle_t t,
                                const gptimer_alarm_event_data_t *e, void *ctx)
{
    uint32_t high = s_servo_high_req;
    if (high > SWPWM_SERVO_PERIOD_US) high = SWPWM_SERVO_PERIOD_US;
    uint32_t next;

    if (s_servo_level == 0) {
        if (high == 0) {
            next = SWPWM_SERVO_PERIOD_US;
        } else {
            gpio_set_level(SWPWM_SERVO_GPIO, 1);
            s_servo_level = 1;
            next = high;
        }
    } else {
        uint32_t low = SWPWM_SERVO_PERIOD_US - high;
        if (low == 0) {
            next = SWPWM_SERVO_PERIOD_US;
        } else {
            gpio_set_level(SWPWM_SERVO_GPIO, 0);
            s_servo_level = 0;
            next = low;
        }
    }
    rearm(t, next);
    return false;
}

/* Crea un GPTimer a 1 MHz (1 cuenta = 1 us), le registra su ISR y arranca. */
static void crear_timer(gptimer_handle_t *out, gptimer_alarm_cb_t cb,
                        uint32_t periodo_us)
{
    gptimer_config_t cfg = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,            /* 1 us por cuenta */
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&cfg, out));

    gptimer_event_callbacks_t cbs = { .on_alarm = cb };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(*out, &cbs, NULL));

    /* Primera alarma al final de un periodo para que el ISR arranque el ciclo */
    gptimer_alarm_config_t a = {
        .alarm_count = periodo_us,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(*out, &a));
    ESP_ERROR_CHECK(gptimer_enable(*out));
    ESP_ERROR_CHECK(gptimer_start(*out));
}

void swpwm_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << SWPWM_MOTOR_GPIO) | (1ULL << SWPWM_SERVO_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    gpio_set_level(SWPWM_MOTOR_GPIO, 0);
    gpio_set_level(SWPWM_SERVO_GPIO, 0);

    crear_timer(&s_motor_tmr, motor_isr, SWPWM_MOTOR_PERIOD_US);
    crear_timer(&s_servo_tmr, servo_isr, SWPWM_SERVO_PERIOD_US);
}

void swpwm_motor_set_duty(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_motor_high_req = (uint32_t)percent * SWPWM_MOTOR_PERIOD_US / 100u;
}

void swpwm_servo_set_angle(uint8_t degrees)
{
    if (degrees > 180) degrees = 180;
    s_servo_high_req = SWPWM_SERVO_MIN_US +
        (uint32_t)degrees * (SWPWM_SERVO_MAX_US - SWPWM_SERVO_MIN_US) / 180u;
}