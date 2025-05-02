/**
  Generated main.c file from MPLAB Code Configurator

  @Company
    Microchip Technology Inc.

  @File Name
    main.c

  @Summary
    This is the generated main.c using PIC24 / dsPIC33 / PIC32MM MCUs.

  @Description
    This source file provides main entry point for system initialization and application code development.
    Generation Information :
        Product Revision  :  PIC24 / dsPIC33 / PIC32MM MCUs - 1.171.5
        Device            :  PIC24FJ64GA702
    The generated drivers are tested against the following:
        Compiler          :  XC16 v2.10
        MPLAB             :  MPLAB X v6.05
*/

/*
    (c) 2020 Microchip Technology Inc. and its subsidiaries. You may use this
    software and any derivatives exclusively with Microchip products.
    ...
    (License text unchanged)
    ...
*/

/**
  Section: Included Files
*/
#include "mcc_generated_files/system.h"
#include <xc.h>
#include <libpic30.h>
#include <stdint.h>
#include <stdio.h>

#define VEML3328_SLAVE_ADD    0x10
#define CONF                  0x00
#define R_DATA                0x05
#define G_DATA                0x06
#define B_DATA                0x07
#define NUM_SAMPLES           5
#define FCY                   16000000UL

/**
  Section: Function Prototypes
*/
void VEML3328_read(void);
const char* VEML3328_color_detection(uint16_t r, uint16_t g, uint16_t b);

/**
  Section: UART Helper Functions
*/
void uart_send_char(uint8_t c)
{
    while (U1STAbits.UTXBF);
    U1TXREG = c;
}

void uart_send_string(const char *str)
{
    while (*str) 
    {
        uart_send_char(*str++);
    }
}

/**
  Section: WS2812 Control via SPI
*/
void WS2812_Send_Byte(uint8_t byte)
{
    for (int i = 7; i >= 0; i--) 
    {
        uint8_t spi_data = (byte & (1 << i)) ? 0b11110000 : 0b11000000;
        SPI1_Exchange8bit(spi_data);
    }
}

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
  Section: Color Sensor Reading and Detection
*/
void VEML3328_read()
{
    uint32_t sum_r = 0, sum_g = 0, sum_b = 0;
    char buffer[100];

    // Collect NUM_SAMPLES readings
    for (int i = 0; i < NUM_SAMPLES; i++)
    {
        uint16_t r = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, R_DATA);
        uint16_t g = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, G_DATA);
        uint16_t b = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, B_DATA);
        // Fix byte order
        sum_r += ((r & 0xFF) << 8) | (r >> 8);
        sum_g += ((g & 0xFF) << 8) | (g >> 8);
        sum_b += ((b & 0xFF) << 8) | (b >> 8);
        __delay32(10000);
    }

    uint16_t avg_r = sum_r / NUM_SAMPLES;
    uint16_t avg_g = sum_g / NUM_SAMPLES;
    uint16_t avg_b = sum_b / NUM_SAMPLES;

    // Detect color
    const char* color = VEML3328_color_detection(avg_r, avg_g, avg_b);

    // Print one line with averages and color
    snprintf(buffer, sizeof(buffer),"AVERAGE VALUES: R=%u, G=%u, B=%u, %s\r\n",avg_r, avg_g, avg_b, color);
    uart_send_string(buffer);
}

const char* VEML3328_color_detection(uint16_t r, uint16_t g, uint16_t b) 
{
    if (r >= 50000 && g > 21000 && g < 26000 && b > 21000 && b < 26000) 
    {
        return "RED";
    }
    else if (r > 9000 && r < 13000 && g > 31000 && g < 36500 && b > 30000 && b < 35000) 
    {
        return "GREEN";
    }
    else if (r > 10500 && r < 15000 && g > 46000 && g < 52500 && b > 63000) 
    {
        return "BLUE";
    }
    else if (r > 7000 && r < 11000 && g > 12000 && g < 16500 && b > 17500 && b < 22000) 
    {
        return "BLACK";
    }
    else if (r > 63000 && g > 64500 && b > 64500) 
    {
        return "WHITE";
    }
    else if (r > 57000 && g > 22500 && g < 27500 && b > 16000 && b < 23500)
    {
        return "ORANGE";
    }
    else if (r > 65000 && g > 65000 && b > 35000 && b < 39000)
    {
        return "YELLOW";
    }
    else if (r > 48000 && g > 21000 && g < 27500 && b > 31000 && b < 37500)
    {
        return "PURPLE";
    }
    else if (r > 12000 && r < 17000 && g > 14000 && g < 18500 && b > 17000 && b < 23500)
    {
        return "BROWN";
    }
    else
    {
        return "Tile absent!";
    }
}

/**
  Section: Main
*/
int main(void)
{
    // Initialize system
    SYSTEM_Initialize();

    // Configure sensor: shutdown then enable
    i2c_write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x8011);
    __delay32(100000);
    i2c_write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x0430);
    __delay32(100000);

    while (1)
    {
        WS2812_SetColor(128, 128, 128);
        VEML3328_read();
        __delay32(16000000);
    }
    
    return 1;
}
