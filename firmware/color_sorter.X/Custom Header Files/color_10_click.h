/**
 * @file color_10_click.h
 * @brief Driver and interface definitions for Color 10 Click sensor using the VEML3328 and WS2812.
 *
 * This header provides:
 *   - I2C register definitions for VEML3328 color channels.
 *   - Function prototypes for sensor initialization, data reading, and color detection.
 *   - WS2812 LED control prototypes.
 */

#ifndef COLOR_10_CLICK_H
#define COLOR_10_CLICK_H

/** @brief 7-bit I2C address of the VEML3328 color sensor. */
#define VEML3328_SLAVE_ADD 0x10
/** @brief VEML3328 Configuration register address. */
#define CONF 0x00
/** @brief Red channel data register address. */
#define R_DATA 0x05
/** @brief Green channel data register address. */
#define G_DATA 0x06
/** @brief Blue channel data register address. */
#define B_DATA 0x07
/** @brief Number of samples to average for noise reduction. */
#define NUM_SAMPLES 5

/** @brief Flag indicating whether simulation mode is enabled. */
extern volatile bool simulationEnabled;
/** @brief Tiles color configuration array.*/
extern char color_array[9];

static void WS2812_Send_Byte(uint8_t);
static void WS2812_SetColor(uint8_t, uint8_t, uint8_t);

void VEML3328_init(void);
void VEML3328_read(void);
void VEML3328_color_detection(uint16_t, uint16_t, uint16_t);

#endif // COLOR_10_CLICK_H
