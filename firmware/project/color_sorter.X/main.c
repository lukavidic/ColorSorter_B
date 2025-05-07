/**
 * @file main.c
 * @brief Project entry point: system and peripheral initialization, main control loop.
 *
 * Performs startup of I2C, UART, SPI, PWM servo, and Wi-Fi;
 * contains a loop that parses UART commands, reads/averages color sensor data, drives the servo/LEDs,
 * and sends movement messages over TCP to the java Application.
 *
 */

/** 
 * @defgroup servo Servo Control
 * @brief API for initializing and driving the PWM servo.
 *
 * This module contains functions to setup Timer2/OC1 for 50 Hz PWM, 
 * move the servo to 0 ,90 ,180 degrees, and arbitrary angles,  
 * plus a toggle helper.
 */
 
 /** 
 * @defgroup wifi Wi-Fi / ESP8266
 * @brief AT-command interface for ESP8266 TCP connectivity.
 *
 * Contains the UART1 init, circular buffer ISR, and  
 * all AT-command sequences (mode, start TCP, send data etc.).
 */
 
 /** 
 * @defgroup color Color Sensor & Detection
 * @brief VEML3328 initialization, sampling, and color-based actions.
 *
 * Implements functions for sending SPI data to WS2812 LEDs, initializing the VEML3328
 * sensor, reading color data, and detecting colors to control actuators.
 */
 
#include "mcc_generated_files/system.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <xc.h>
#define FCY 16000000UL
#include <libpic30.h>

/** 
 * @defgroup color
 * @{
 */
 
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

/** @} */

/** 
 * @defgroup servo
 * @{
 */
 
/** @brief PWM frequency in Hertz used by Timer2/OC1 for driving the servo. */
#define PWM_FREQ 50
/** @brief Timer count for a 1.0 ms pulse */
#define SERVO_RIGHT 1400
/** @brief Timer count for a 2.0 ms pulse */
#define SERVO_LEFT 4800
/** @brief Timer count for a 1.5 ms pulse */
#define SERVO_CENTER 3000

/** @} */

/** 
 * @defgroup wifi Wi-Fi / ESP8266
 * @{
 */
 
/** @brief Circular buffer size for UART RX */
#define BUFFER_SIZE 128

/** @brief UART receive buffer */
volatile char rx_buffer[BUFFER_SIZE];
/** @brief Head index of the circular buffer */
volatile uint16_t rx_head = 0;
/** @brief Tail index of the circular buffer */
volatile uint16_t rx_tail = 0;

/** @brief AT command to test communication */
const char* CMD_AT        = "AT\r\n";
/** @brief AT command to reset the module */
const char* CMD_RST       = "AT+RST\r\n";
/** @brief AT command to get firmware version */
const char* CMD_GMR       = "AT+GMR\r\n";
/** @brief AT command to set mode (1 = Station mode) */
const char* CMD_MODE      = "AT+CWMODE=1\r\n";
/** @brief AT command to connect to WiFi network */
const char* CMD_WIFI_CONN = "AT+CWJAP=\"etfbl.net\",\"\"\r\n";
/** @brief AT command to get IP address */
const char* CMD_CHECK_IP  = "AT+CIFSR\r\n";
/** @brief AT command to configure connection mode (0 = Single connection) */
const char* CMD_CONN_TYPE = "AT+CIPMUX=0\r\n";
/** @brief AT command to start TCP connection */
const char* CMD_START_TCP = "AT+CIPSTART=\"TCP\",\"10.99.131.221\",8084\r\n"; 
/** @brief AT command to send 5 bytes of data */
const char* CMD_SEND      = "AT+CIPSEND=5\r\n";

/** @} */


/* Color configuration values */
volatile char configRed, configGreen, configBlue, configYellow;
volatile char configPurple, configOrange, configBlack, configWhite, configBrown;

/** 
 * @defgroup color
 * @{
 */
 
/** @brief Average red channel value across samples. */
static uint16_t avg_value_red;
/** @brief Average green channel value across samples. */
static uint16_t avg_value_green;
/** @brief Average blue channel value across samples. */
static uint16_t avg_value_blue;

/** @} */

/** @brief Array indicating tiles color servo side L(1)/R(0).*/
char color_array[9] = {0};

/**
 * @ingroup wifi
 * @brief Configure UART1 for ESP8266 AT-command communication at 115200 baud.
 * @note Uses RB6 as RX and RB7 as TX.
 */
void UART1_Init(void) 
{
    IEC0bits.U1TXIE = 0;
    IEC0bits.U1RXIE = 0;

    /** Unlock (PPS), map U1TX and U1RX */
    __builtin_write_OSCCONL(OSCCON & 0xBF);
    RPOR3bits.RP7R = 3;     // Remap UART1 pin : U1TX on RB7
    RPINR18bits.U1RXR = 6;  // Remap UART1 pin : U1RX on RB6
    // Lock PPS
    __builtin_write_OSCCONL(OSCCON | 0x40);

    U1MODE = 0;   // Clear mode register
    U1STA = 0;    // Clear status register
    
    U1MODEbits.BRGH = 1;  // High-speed mode (4 clocks per bit)
    /* For 115200 baud at FCY = 16MHz, U1BRG = (16e6/(4*115200))-1 ? 34 */
    U1BRG = 34;
    U1MODEbits.UARTEN = 1; // Enable UART module
    U1STAbits.UTXEN  = 1;  // Enable transmitter
    U1STAbits.URXEN  = 1;  // Enable receiver

    /** Clear and enable UART RX interrupt. */
    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;
    IPC2bits.U1RXIP = 5;

    /** Configure error interrupt handling. */
    IFS4bits.U1ERIF = 0;
    IEC4bits.U1ERIE = 1;
    IPC16bits.U1ERIP = 5;
}


/**
 * @ingroup wifi
 * @brief UART1 receive interrupt service routine.
 *
 * @details
 * This ISR clears the UART1 receive-interrupt flag, then reads all bytes from the hardware FIFO
 * into the software circular buffer (rx_buffer), advancing the head index and preventing overflow
 * by checking against the tail index.
 */
void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void)
{
    IFS0bits.U1RXIF = 0;
    while (U1STAbits.URXDA) 
    {
        uint16_t next_head = (rx_head + 1) % BUFFER_SIZE;
        if (next_head != rx_tail) 
        {
           rx_buffer[rx_head] = U1RXREG;
           rx_head = next_head;
        }
    }
}

/**
 * @ingroup wifi
 * @brief UART1 error interrupt service routine.
 * @note Handles overrun, framing, and parity errors.
 */
void __attribute__((interrupt, no_auto_psv)) _U1ErrInterrupt(void) 
{
    volatile uint16_t error_status = U1STA;
    if (U1STAbits.OERR) U1STAbits.OERR = 0;
    if (U1STAbits.FERR) U1STAbits.FERR = 0;
    IFS4bits.U1ERIF = 0;
}

/**
 * @ingroup wifi
 * @brief Sends a null-terminated string over UART1.
 * @param str Pointer to the string to send.
 */
void UART1_SendString(const char* str) 
{
    while(*str) 
    {
        while(U1STAbits.UTXBF);  // Wait until transmit buffer is not full
        U1TXREG = *str++;
    }
}

/**
 * @ingroup wifi
 * @brief Clears the UART receive buffer.
 * @note Resets the head and tail indices.
 */
void cleanBuffer() 
{
    rx_head = 0;
    rx_tail = 0;
	for(int i = 0; i < BUFFER_SIZE; i++)
    {
         rx_buffer[i] = 0;
    }
}

/**
 * @ingroup wifi
 * @brief Reads a string from the circular buffer (rx_buffer).
 * @param dest Destination buffer to store the string.
 * @param max_len Maximum number of characters to read.
 * @return Number of characters read.
 */
uint16_t UART_ReadString(char* dest, unsigned max_len) 
{
    uint16_t count = 0;
    IEC0bits.U1RXIE = 0;  // Disable interrupts
    
    while (count < max_len - 1 && rx_head != rx_tail) 
    {
        char c = rx_buffer[rx_tail];
        rx_tail = (rx_tail + 1) % BUFFER_SIZE;
        
        dest[count++] = c;
        if (c == '\n') break;  // Stop at newline
    }
    IEC0bits.U1RXIE = 1;  // Re-enable interrupts
    dest[count] = '\0';    // Null-terminate
    return count;
}

/**
 * @ingroup wifi
 * @brief Checks if data is available in the receive buffer.
 * @return true if data is available, false otherwise.
 */
bool UART_DataReady(void)
{    
    return !(rx_head == rx_tail);
}

/**
 * @brief Parses configuration message in the format "CFG:Key=Value,...".
 *
 * @details
 * Locates the "CFG:" prefix and splits the following text into comma-separated Key=Value tokens.  
 * For each token, it extracts the key and single-character value and updates the corresponding global config flag.  
 * Tokens with unknown keys or missing values are ignored.
 *
 * @param msg Input message string.
 * @note Supports keys for Red, Green, Blue, Yellow, Purple, Orange, Black, White, Brown.
 */
void parseConfigMessage(char* msg) 
{
    char *p = strstr(msg, "CFG:");
    if (p != NULL)
        p += 4;
    
    // Tokenize the string using comma as delimiter.
    char* token = strtok(p, ",");
    while (token != NULL) 
    {
        
	    // Each token is expected to have the format: Key=Value (e.g. "Red=L")

        char* equalSign = strchr(token, '=');
        if (equalSign != NULL && *(equalSign + 1) != '\0') 
        {
            *equalSign = '\0';  // Split the string into two null-terminated strings
            char* key = token;
            char* value = equalSign + 1;
            // Check the key and save the value into a global variable. 
            if (strcmp(key, "Red") == 0)
                configRed = value[0];
            else if (strcmp(key, "Green") == 0)
                configGreen = value[0];
            else if (strcmp(key, "Blue") == 0)
                configBlue = value[0];
            else if (strcmp(key, "Yellow") == 0)
                configYellow = value[0];
            else if (strcmp(key, "Purple") == 0)
                configPurple = value[0];
            else if (strcmp(key, "Orange") == 0)
                configOrange = value[0];
            else if (strcmp(key, "Black") == 0)
                configBlack = value[0];
            else if (strcmp(key, "White") == 0)
                configWhite = value[0];
            else if (strcmp(key, "Brown") == 0)
                configBrown = value[0];
        }
        // Get next token
        token = strtok(NULL, ",");
    }
}

/**
 * @brief Sends information on movement over the ESP8266 TCP link.
 *
 * @details
 * Formats a `MOV#<side>#<color>\r\n` message, computes its length, and issues
 * the corresponding `AT+CIPSEND=<len>` command to the module. After receiving
 * positive prompt, it transmits the movement string and clears the UART buffer
 * to prepare for the next operation.
 *
 * @param side Movement direction (e.g., "left", "right").
 * @param color Color (e.g., "red", "blue").
 */
void sendMovementCommand(const char* side, const char* color) 
{
    char dataMsg[BUFFER_SIZE];
    char atCommand[32]; 
  
    sprintf(dataMsg, "MOV#%s#%s\r\n", side, color); // Build the command message; the format is "MOV#<side>#<color>\r\n"
    
    /** Calculate the length of the data message (not including the null terminator). */
    int len = strlen(dataMsg);  //!< For "MOV#right#orange\r\n", len should be 18.
    
    sprintf(atCommand, "AT+CIPSEND=%d\r\n", len); //!< Build the AT command with the calculated length.
    
    UART1_SendString(atCommand); //!< Send the AT+CIPSEND command so the module is ready to accept the data.
    __delay32(8000000);          //!< Wait for the ">" prompt from the module (adjust delay as needed)
    cleanBuffer();
   
    UART1_SendString(dataMsg); //!< Now send the actual movement command message
    __delay32(8000000);        //!< Give time for the data transmission
    cleanBuffer();
}


/**
 * @ingroup servo
 * @brief Initialize Timer2 and OC1 for PWM control of the servo.
 * 
 * Sets up 20 ms period (50 Hz) channel on RB5 / OC1.
 * Called before any move or toggle.
 */
void servo_init() 
{
    T2CONbits.TON = 0;          // Disable Timer2
    TRISBbits.TRISB5 = 0;       // Configure RB5 as output
    T2CONbits.TCKPS = 0b01;     // Prescaler 1:8
    TMR2 = 0;                   // Reset Timer2
    OC1CON1bits.OCTSEL = 0;     // Timer2 as clock source
    OC1CON1bits.OCM = 0b110;    // PWM mode (fault disabled)
    OC1CON2bits.SYNCSEL = 0x1F; // Self-sync mode
    OC1CON2bits.OCTRIG = 0;     // Internal trigger                          
    T2CONbits.TON = 1;          // Enable Timer2  
    OC1R = SERVO_CENTER;        // Initial duty cycle
    PR2 = 39999;                // 20ms period: (39999 + 1) * (8/16MHz) = 20ms
    OC1RS = PR2;
 }

uint16_t counter = 0;

/**
 * @ingroup servo
 * @brief Cycle through CENTER -> RIGHT -> CENTER -> LEFT -> ... 
 * 
 * Each call advances the servo to the next of four positions.
 */
void toggle_servo() 
{
        switch(counter)
        {
            case 0: OC1R = SERVO_CENTER;  break;
            case 1: OC1R = SERVO_RIGHT;   break;
            case 2: OC1R = SERVO_CENTER;  break;
            case 3: OC1R = SERVO_LEFT;  break;
        }
        counter = (counter + 1) % 4;
}
/**
 * @ingroup servo
 * @brief Move the servo fully to the right (0 degrees).
 */
void moveRight() 
{
   OC1R = SERVO_RIGHT;
}
/**
 * @ingroup servo
 * @brief Move the servo fully to the left (180 degrees).
 */
void moveLeft() 
{
   OC1R = SERVO_LEFT;
}

/**
 * @ingroup servo
 * @brief Move the servo to the center (90 degrees).
 */
void moveCenter() 
{
   OC1R = SERVO_CENTER;
}

/**
 * @ingroup servo
 * @brief Set servo to an arbitrary angle between 0 and 180 degrees.
 * @param angle  Desired angle in degrees; clipped to [0,180].
 * 
 * Uses linear mapping into the timer-count range.
 */
void servo_set_angle(int angle) 
{
    if(angle < 0) angle = 0;
    if(angle > 180) angle = 180;
    // Linear mapping: 0° => SERVO_MIN (1200 counts), 180° => SERVO_MAX (4800 counts).
    OC1R = (SERVO_RIGHT + (angle * 20)); // (SERVO_MIN + ((angle * (SERVO_MAX - SERVO_MIN)) / 180));  
}

/** @brief Global flag to control simulation state (true = running, false = paused). */
volatile bool simulationEnabled = false; 

/**
 * @brief INT1 Interrupt Service Routine for RB15 push-button.
 *
 * This handler fires on the falling edge of RB15 (active-low button)
 * and toggles the global `simulationEnabled` flag. After servicing,
 * it clears the INT1 interrupt flag to allow subsequent events.
 *
 */
void __attribute__((interrupt, no_auto_psv)) _INT1Interrupt(void)
{
    if (PORTBbits.RB15 == 0) 
        simulationEnabled = !simulationEnabled;  
    IFS1bits.INT1IF = 0;
}

/**
 * @ingroup color
 * @brief Initializes the VEML3328 sensor with configured gain and integration time.
 *
 * Configures the sensor registers to shut down for safe configuration,
 * sets gain to 4x and integration time to 400ms, then enables the sensor.
 */
void VEML3328_init()
{    
    i2c_write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x8011); // Shut down the sensor so configuration can be safely changed
    __delay32(100000);
    i2c_write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x0430); // Enable sensor (Gain: 4x, Integration Time: 400ms)
    __delay32(100000);
}
/**
 * @ingroup color
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
 * @ingroup color
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
void WS2812_SetColor(uint8_t red, uint8_t green, uint8_t blue) // (0,0,0) OFF, (128,128,128) WHITE LIGHT
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
 * @ingroup color
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
 * @ingroup color
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
void VEML3328_color_detection(uint16_t avg_value_red, uint16_t avg_value_green, uint16_t avg_value_blue) //!< with calibration
{
    if (avg_value_red >= 50000 && avg_value_green > 21000 && avg_value_green < 26000 && avg_value_blue > 21000 && avg_value_blue < 26000) 
    {
        if(color_array[0]==1) // RED
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
        if(color_array[3]==1) // BLACK
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
    else if (avg_value_red > 63000 && avg_value_green > 64000 && avg_value_blue > 33000 && avg_value_blue < 39000) 
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
        if(color_array[8]==1) // BROWN
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

/**
 * @brief Populates the color_array based on configuration flags.
 * 
 * This function sets each element of the `color_array` to 1 if the corresponding
 * configuration variable is set to `'L'` (indicating that the disk color is tied to the left side),
 * or to 0 otherwise (right side).
 * 
 */
void populateColorArrayFromConfig(void) 
{
    color_array[0] = (configRed == 'L') ? 1 : 0;
    color_array[1] = (configGreen == 'L') ? 1 : 0;
    color_array[2] = (configBlue == 'L') ? 1 : 0;
    color_array[3] = (configBlack == 'L') ? 1 : 0;
    color_array[4] = (configWhite == 'L') ? 1 : 0;
    color_array[5] = (configOrange == 'L') ? 1 : 0;
    color_array[6] = (configYellow == 'L') ? 1 : 0;
    color_array[7] = (configPurple == 'L') ? 1 : 0;
    color_array[8] = (configBrown == 'L') ? 1 : 0;
}

int main(void)
{
    /** RIGHT SET 0 */
    color_array[0] = 0; // red
    color_array[1] = 0; // green  
    color_array[2] = 0; // blue
    color_array[3] = 0; // black
    color_array[4] = 0; // white
    color_array[5] = 0; // orange
    color_array[6] = 0; // yellow
    color_array[7] = 0; // purple
    color_array[8] = 0; // brown
    
    /** Initialization */
    SYSTEM_Initialize();
    VEML3328_init();
    __delay32(16000000);     
    servo_init();
    UART1_Init();
    __delay32(8000000);      
    __builtin_enable_interrupts();
    __delay32(32000000);
    
    /**
    *	WiFi Connection Sequence
    *   Set WiFi mode to Station
	*/
    UART1_SendString(CMD_MODE);
    __delay32(8000000);
    cleanBuffer();
    /** Connect to the WiFi network (SSID "etfbl.net" with no password). */
    UART1_SendString(CMD_WIFI_CONN);
    __delay32(80000000); // Allow time for the join process
    cleanBuffer();
    
    /** 
	* TCP Connection Sequence
    * Set connection mode to single connection. 
	*/
    UART1_SendString(CMD_CONN_TYPE);
    __delay32(16000000);
    cleanBuffer();
    /** Establish a TCP connection with the remote IP and port. */
    UART1_SendString(CMD_START_TCP);
    __delay32(16000000);  // Allow time for connection to establish
    cleanBuffer();
    /** Prepare to send data (3 bytes). */
    UART1_SendString(CMD_SEND);
    __delay32(8000000);  // Wait for ">" prompt from the module
    cleanBuffer();
    /**Send the actual data. */
    UART1_SendString("ON!\r\n");
    __delay32(8000000); // Give time for data transmission
    
	 /** 
      * The loop checks for incoming UART messages and depending on the message recieved it
      * parses and applies configuration settings, and on "Start" or "Stop" toggles the simulationEnabled flag.
      * Besides UART handling, when simulationEnabled is true it updates LEDs, reads sensor data,
	  * and depending on the sensor data moves servo and sends info to the application if required.
      * Otherwise if simulationEnabled is false it clears LEDs and idles with timed delays.
	*/
    while (1)
    {
        if (UART_DataReady()) 
        {
            __delay32(1000000);
            
           if (strstr(rx_buffer, "CFG:") != NULL) // Check for configuration message (starting with "CFG:")
           {
               rx_buffer[rx_head] = '\0';   // Null-terminate the string
               parseConfigMessage(rx_buffer);
               populateColorArrayFromConfig();
               __delay32(4000000);
           }

           if (strstr(rx_buffer, "Start")!= NULL) // Check for start message
               simulationEnabled = true;

           if (strstr(rx_buffer, "Stop")!= NULL) // Check for stop message
               simulationEnabled = false;

           cleanBuffer();      
        }
    
        if(simulationEnabled)
        {
            WS2812_SetColor(128, 128, 128);
            __delay32(8000);
            LATBbits.LATB14 = 1;
            VEML3328_read(); // Reads sensor data
        }
        else
        {
            LATBbits.LATB14 = 0;
            __delay32(8000);
            WS2812_SetColor(0, 0, 0);
            __delay32(8000);
        }
    }
    return 1;
}