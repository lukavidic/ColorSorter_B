/**
 * @file color_10_click.c
 * @brief Implementation of Color 10 Click VEML3328 sensor operations and WS2812 LED control.
 *
 * Implements functions for sending SPI data to WS2812 LEDs, initializing the VEML3328
 * sensor, reading color data, and detecting colors to control actuators.
 */

#include "color_10_click.h"
#include "mcc_generated_files/system.h"
#include "wifi.h"
#include "servo.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <xc.h>
#define FCY 16000000UL
#include <libpic30.h>

/** @brief Average red channel value across samples. */
static uint16_t avg_value_red;
/** @brief Average green channel value across samples. */
static uint16_t avg_value_green;
/** @brief Average blue channel value across samples. */
static uint16_t avg_value_blue;

/**
 * @brief Sends a single byte to the WS2812 LED strip via SPI encoding.
 *
 * @param byte Byte to send; each bit is encoded into SPI pattern for WS2812 protocol.
 */
void WS2812_Send_Byte(uint8_t byte) 
{
    for (int i = 7; i >= 0; i--)  
    {
        uint8_t spi_data = (byte & (1 << i)) ? 0b11110000 : 0b11000000; 
        SPI1_Exchange8bit(spi_data);
    }
}

/**
 * @brief Sets the color of the WS2812 LED strip.
 *
 * @param red   Intensity of red channel (0-255).
 * @param green Intensity of green channel (0-255).
 * @param blue  Intensity of blue channel (0-255).
 *
 * Disables and re-enables SPI around the data transmission and sends
 * the color channels in GRB order.
 * (0,0,0) OFF, (128,128,128) WHITE LIGHT
 */
void WS2812_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{  
    SPI1CON1Lbits.SPIEN = 0;
    __delay32(800);
    SPI1CON1Lbits.SPIEN = 1;
    WS2812_Send_Byte(green);
    WS2812_Send_Byte(red);
    WS2812_Send_Byte(blue); 
    SPI1CON1Lbits.SPIEN = 0;
    __delay32(800);
    SPI1CON1Lbits.SPIEN = 1;    
}

/**
 * @brief Initializes the VEML3328 sensor with configured gain and integration time.
 *
 * Configures the sensor registers to shut down for safe configuration,
 * sets gain to 4x and integration time to 400ms, then enables the sensor.
 */
void VEML3328_init(void) 
{
    I2C1_Write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x8011); // Shut down the sensor so configuration can be safely changed
    __delay32(100000);
	
    I2C1_Write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x0430); // Enable sensor (Gain: 4x, Integration Time: 400ms)
    __delay32(100000);
}

/**
 * @brief Reads raw color data from VEML3328 sensor and computes channel averages.
 *
 * Reads 16-bit raw data for red, green, and blue channels NUM_SAMPLES times,
 * computes the average for each channel, and updates internal average variables.
 * Triggers color detection in simulation mode or turns off LEDs otherwise.
 */
void VEML3328_read()
{
    uint16_t sensor_value_red, sensor_value_green, sensor_value_blue;
    uint32_t value_red = 0, value_green = 0, value_blue = 0;
    char uart_string[100];
    
    for(int i = 0; i < NUM_SAMPLES; i++)
    {
        sensor_value_red = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, R_DATA);       
        value_red += (((sensor_value_red & 0xFF)<<8) | (sensor_value_red >> 8));
        sensor_value_green = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, G_DATA);
        value_green += (((sensor_value_green & 0xFF)<<8) | (sensor_value_green >> 8));
        sensor_value_blue = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, B_DATA);
        value_blue += (((sensor_value_blue & 0xFF)<<8) | (sensor_value_blue >> 8));
        
        __delay32(10000);
    }
    
    avg_value_red = value_red / NUM_SAMPLES;       
    avg_value_green = value_green / NUM_SAMPLES;   
    avg_value_blue = value_blue / NUM_SAMPLES; 
    
    if(simulationEnabled)
    {
        VEML3328_color_detection(avg_value_red, avg_value_green, avg_value_blue);
        __delay32(8000000);
    }
    else 
    {
        LATBbits.LATB14 = 0;
        __delay32(8000);
        WS2812_SetColor(0, 0, 0);
        __delay32(8000);   
    }
}

/**
 * @brief Detects the current color based on averaged sensor values and acts accordingly.
 *
 * @param avg_value_red   Averaged red channel value.
 * @param avg_value_green Averaged green channel value.
 * @param avg_value_blue  Averaged blue channel value.
 *
 * Compares averaged values against calibrated thresholds for known colors and
 * performs movement and communication commands. If no known color is detected,
 * centers the actuator.
 */
void VEML3328_color_detection(uint16_t avg_value_red, uint16_t avg_value_green, uint16_t avg_value_blue) //with calibration
{
    if (avg_value_red >= 50000 && avg_value_green > 21000 && avg_value_green < 26000 && avg_value_blue > 21000 && avg_value_blue < 26000) 
    {
        if(color_array[0]==1) //RED
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","red"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","red"); moveCenter(); __delay32(4000000);     
        }
    }
    else if (avg_value_red > 9000 && avg_value_red < 13000 && avg_value_green > 31000 && avg_value_green < 36500 && avg_value_blue > 30000 && avg_value_blue < 35000  ) 
    {
        if(color_array[1]==1) // GREEN
        {
            moveLeft(); __delay32(4000000); sendMovementCommand("left","green"); moveCenter(); __delay32(4000000);    
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","green"); moveCenter(); __delay32(4000000);
        }
    }
    else if (avg_value_red > 10500 && avg_value_red < 15000 && avg_value_green > 46000 && avg_value_green < 52500 && avg_value_blue > 63000)
    {
        if(color_array[2]==1) // BLUE
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","blue"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","blue"); moveCenter(); __delay32(4000000);
        }
    }
    else if ((avg_value_red > 7000 && avg_value_red < 11000 && avg_value_green > 12000 && avg_value_green < 16500 && avg_value_blue > 17500 && avg_value_blue < 22000)
            || (avg_value_red > 5500 && avg_value_red < 8500 && avg_value_green > 9000 && avg_value_green < 12000 && avg_value_blue > 11000 && avg_value_blue < 15000))
    {
        if(color_array[3]==1) //BLACK
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","black"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","black"); moveCenter(); __delay32(4000000);
        }
    }
    else if ((avg_value_red > 62000  && avg_value_green > 64500 && avg_value_blue > 64500)||
            (avg_value_red > 25000 && avg_value_red < 33000 && avg_value_green > 33000 && avg_value_green < 44000 && avg_value_blue > 50000))
    {
        if(color_array[4]==1) // WHITE
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","white"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","white"); moveCenter(); __delay32(4000000);
        }
    }
    else if (avg_value_red > 57000 && avg_value_green > 22500 && avg_value_green < 27500 && avg_value_blue > 16000 && avg_value_blue < 23500) 
    {
        if(color_array[5]==1) // ORANGE
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","orange"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","orange"); moveCenter(); __delay32(4000000);
        }
    }
    else if (avg_value_red > 65000 && avg_value_green > 65000 && avg_value_blue > 35000 && avg_value_blue < 39000) 
    {
        if(color_array[6]==1) // YELLOW
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","yellow"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","yellow"); moveCenter(); __delay32(4000000);
        }
    }
    else if (avg_value_red > 48000 && avg_value_green > 21000 && avg_value_green < 27500 && avg_value_blue > 31000 && avg_value_blue < 37500) 
    {
        if(color_array[7]==1) // PURPLE
        {   
            moveLeft(); __delay32(4000000); sendMovementCommand("left","purple"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","purple"); moveCenter(); __delay32(4000000);
        }
    }
    else if (avg_value_red > 12000 && avg_value_red < 17000 && avg_value_green > 14000 && avg_value_green < 18500 && avg_value_blue > 17000 && avg_value_blue < 23500)
    {
        if(color_array[8]==1) //BROWN
        {  
            moveLeft(); __delay32(4000000); sendMovementCommand("left","brown"); moveCenter(); __delay32(4000000);
        }
        else
        {
            moveRight(); __delay32(4000000); sendMovementCommand("right","brown"); moveCenter(); __delay32(4000000);
        }
    }
    else // TILE ABSENT
    {
            __delay32(4000000); moveCenter(); __delay32(4000000);
    }   
}