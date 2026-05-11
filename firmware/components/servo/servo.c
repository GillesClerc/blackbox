#include "servo.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

#define TAG "servo"

#define SERVO_COUNT         2
#define MCPWM_FREQ_HZ       50          // 50 Hz
#define MCPWM_RESOLUTION_HZ 1000000     // 1 MHz → résolution 1µs
#define MCPWM_PERIOD_TICKS  (MCPWM_RESOLUTION_HZ / MCPWM_FREQ_HZ)  // 20000 ticks = 20ms

static const int s_gpio[SERVO_COUNT] = { SERVO_GPIO_0, SERVO_GPIO_1 };

static mcpwm_timer_handle_t   s_timer        = NULL;
static mcpwm_oper_handle_t    s_oper[SERVO_COUNT];
static mcpwm_cmpr_handle_t    s_cmp[SERVO_COUNT];
static mcpwm_gen_handle_t     s_gen[SERVO_COUNT];

static uint32_t angle_to_ticks(float angle)
{
    if (angle < 0.0f)   angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    return (uint32_t)(SERVO_PULSE_MIN_US +
           (angle / 180.0f) * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US));
}

esp_err_t servo_init(void)
{
    // Timer partagé entre les deux servos
    mcpwm_timer_config_t timer_cfg = {
        .group_id      = 0,
        .clk_src       = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = MCPWM_RESOLUTION_HZ,
        .period_ticks  = MCPWM_PERIOD_TICKS,
        .count_mode    = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_cfg, &s_timer));

    for (int i = 0; i < SERVO_COUNT; i++) {
        mcpwm_operator_config_t oper_cfg = { .group_id = 0 };
        ESP_ERROR_CHECK(mcpwm_new_operator(&oper_cfg, &s_oper[i]));
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(s_oper[i], s_timer));

        mcpwm_comparator_config_t cmp_cfg = {
            .flags.update_cmp_on_tez = true,
        };
        ESP_ERROR_CHECK(mcpwm_new_comparator(s_oper[i], &cmp_cfg, &s_cmp[i]));

        mcpwm_generator_config_t gen_cfg = { .gen_gpio_num = s_gpio[i] };
        ESP_ERROR_CHECK(mcpwm_new_generator(s_oper[i], &gen_cfg, &s_gen[i]));

        // Haut sur timer empty, bas sur comparateur
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(s_gen[i],
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                         MCPWM_TIMER_EVENT_EMPTY,
                                         MCPWM_GEN_ACTION_HIGH)));
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(s_gen[i],
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                            s_cmp[i],
                                            MCPWM_GEN_ACTION_LOW)));

        // Position initiale : fermé
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(s_cmp[i],
                        angle_to_ticks(SERVO_ANGLE_CLOSED)));
    }

    ESP_ERROR_CHECK(mcpwm_timer_enable(s_timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(s_timer, MCPWM_TIMER_START_NO_STOP));

    ESP_LOGI(TAG, "2 servos SG90 initialisés (GPIO%d, GPIO%d)",
             SERVO_GPIO_0, SERVO_GPIO_1);
    return ESP_OK;
}

esp_err_t servo_set_angle(uint8_t id, float angle)
{
    if (id >= SERVO_COUNT || !s_cmp[id]) return ESP_ERR_INVALID_ARG;
    uint32_t ticks = angle_to_ticks(angle);
    ESP_LOGI(TAG, "servo%d → %.0f° (%luµs)", id, angle, (unsigned long)ticks);
    return mcpwm_comparator_set_compare_value(s_cmp[id], ticks);
}

esp_err_t servo_open(uint8_t id)  { return servo_set_angle(id, SERVO_ANGLE_OPEN); }
esp_err_t servo_close(uint8_t id) { return servo_set_angle(id, SERVO_ANGLE_CLOSED); }
