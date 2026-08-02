/*
 * Soroka 5x5 WS2812 matrix animation.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#define STRIP_NODE DT_NODELABEL(led_strip)
#define POWER_NODE DT_NODELABEL(matrix_power)
#define MATRIX_WIDTH 5
#define MATRIX_HEIGHT 5
#define MATRIX_LED_COUNT (MATRIX_WIDTH * MATRIX_HEIGHT)
#define FRAME_TIME_MS 16

/* Animation tuning. Values are in the 0..255 LED channel range. */
#define MIN_BRIGHTNESS 3
#define MAX_BRIGHTNESS 25
#define BREATH_PHASE_STEP 2
#define HUE_PHASE_STEP 1
#define CYCLE_LENGTH_MULTIPLIER 3
#define COLOR_SATURATION 235

/* Set to 1 if every second physical row is wired in reverse. */
#define MATRIX_SERPENTINE 0

/*
 * Heart intensity mask. Zero means an unlit pixel; larger values make a pixel
 * brighter. The renderer below turns this shape into a new RGB frame at run
 * time, so no ready-made animation frames are stored in flash.
 */
static const uint8_t heart_mask[MATRIX_HEIGHT][MATRIX_WIDTH] = {
    {0, 210, 0, 210, 0},
    {210, 255, 235, 255, 210},
    {210, 245, 255, 245, 210},
    {0, 210, 245, 210, 0},
    {0, 0, 210, 0, 0},
};

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static const struct gpio_dt_spec matrix_power = GPIO_DT_SPEC_GET(POWER_NODE, gpios);
static struct led_rgb pixels[MATRIX_LED_COUNT];
static uint16_t breath_phase;
static uint16_t hue_phase;

static size_t physical_index(size_t row, size_t column) {
    if (MATRIX_SERPENTINE && (row % 2U) != 0U) {
        column = MATRIX_WIDTH - 1U - column;
    }

    return row * MATRIX_WIDTH + column;
}

/* Integer smoothstep: converts a linear 0..255 ramp into a soft S-curve. */
static uint8_t ease_in_out(uint8_t value) {
    uint32_t squared = (uint32_t)value * value;
    return (uint8_t)((squared * (765U - (2U * value))) / 65025U);
}

static uint8_t breathing_brightness(uint8_t phase) {
    uint8_t triangle = phase < 128U ? phase * 2U : (255U - phase) * 2U;
    uint8_t eased = ease_in_out(triangle);

    return MIN_BRIGHTNESS + (((MAX_BRIGHTNESS - MIN_BRIGHTNESS) * eased) / 255U);
}

/* HSV conversion without float: suitable for calculating every animation frame. */
static struct led_rgb hsv_to_rgb(uint8_t hue, uint8_t saturation, uint8_t value) {
    if (saturation == 0U) {
        return (struct led_rgb){.r = value, .g = value, .b = value};
    }

    uint8_t region = hue / 43U;
    uint8_t remainder = (hue - (region * 43U)) * 6U;
    uint8_t p = ((uint16_t)value * (255U - saturation)) >> 8;
    uint8_t q = ((uint16_t)value * (255U - (((uint16_t)saturation * remainder) >> 8))) >> 8;
    uint8_t t =
        ((uint16_t)value * (255U - (((uint16_t)saturation * (255U - remainder)) >> 8))) >> 8;

    switch (region) {
    case 0:
        return (struct led_rgb){.r = value, .g = t, .b = p};
    case 1:
        return (struct led_rgb){.r = q, .g = value, .b = p};
    case 2:
        return (struct led_rgb){.r = p, .g = value, .b = t};
    case 3:
        return (struct led_rgb){.r = p, .g = q, .b = value};
    case 4:
        return (struct led_rgb){.r = t, .g = p, .b = value};
    default:
        return (struct led_rgb){.r = value, .g = p, .b = q};
    }
}

/*
 * Procedural feature: breathing controls value, time controls the base hue,
 * and a small coordinate offset creates a color gradient across the heart.
 */
static void render_procedural_heart(uint8_t breathing, uint8_t hue) {
    uint8_t brightness = breathing_brightness(breathing);

    for (size_t row = 0; row < MATRIX_HEIGHT; row++) {
        for (size_t column = 0; column < MATRIX_WIDTH; column++) {
            uint8_t intensity = heart_mask[row][column];
            size_t index = physical_index(row, column);

            if (intensity == 0U) {
                pixels[index] = (struct led_rgb){0};
                continue;
            }

            uint8_t pixel_brightness = ((uint16_t)brightness * intensity) / 255U;
            uint8_t pixel_hue = hue + (row * 5U) + (column * 3U);
            pixels[index] = hsv_to_rgb(pixel_hue, COLOR_SATURATION, pixel_brightness);
        }
    }
}

static void matrix_animation_step(struct k_work *work) {
    struct k_work_delayable *animation_work = k_work_delayable_from_work(work);

    render_procedural_heart(breath_phase / CYCLE_LENGTH_MULTIPLIER,
                            hue_phase / CYCLE_LENGTH_MULTIPLIER);

    (void)led_strip_update_rgb(strip, pixels, MATRIX_LED_COUNT);
    breath_phase += BREATH_PHASE_STEP;
    hue_phase += HUE_PHASE_STEP;

    breath_phase %= 256U * CYCLE_LENGTH_MULTIPLIER;
    hue_phase %= 256U * CYCLE_LENGTH_MULTIPLIER;
    k_work_reschedule(animation_work, K_MSEC(FRAME_TIME_MS));
}

K_WORK_DELAYABLE_DEFINE(matrix_animation_work, matrix_animation_step);

static int matrix_animation_init(void) {
    if (!device_is_ready(strip) || !device_is_ready(matrix_power.port)) {
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&matrix_power, GPIO_OUTPUT_ACTIVE);
    if (err != 0) {
        return err;
    }

    k_work_schedule(&matrix_animation_work, K_MSEC(100));
    return 0;
}

SYS_INIT(matrix_animation_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
