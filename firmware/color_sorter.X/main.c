#include "mcc_generated_files/system.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <xc.h>
#define FCY 16000000UL
#include <libpic30.h>

#define VEML3328_SLAVE_ADD 0x10 // 7-bit I2C address of the VEML3328 device
#define CONF 0x00               // VEML3328 Configuration register
#define R_DATA 0x05             // VEML3328 Red channel data register
#define G_DATA 0x06             // VEML3328 Green channel data register
#define B_DATA 0x07             // VEML3328 Blue channel data register
#define NUM_SAMPLES 5           // Take 5 consecutive readings to smooth out noise

#define BUFFER_SIZE 128

// Servo motor
#define PWM_FREQ 50
#define SERVO_RIGHT 1400    // Timer counts for 1.0ms  2000/ pulse (0°) 0.6ms 1200  
#define SERVO_LEFT 4800     // Timer counts for 2.0ms 4000 pulse (180° ) 2.4ms 4800  
#define SERVO_CENTER 3000

// AT Command Strings
const char* CMD_AT        = "AT\r\n";
const char* CMD_RST       = "AT+RST\r\n";
const char* CMD_GMR       = "AT+GMR\r\n";
const char* CMD_MODE      = "AT+CWMODE=1\r\n";
const char* CMD_WIFI_CONN = "AT+CWJAP=\"etfbl.net\",\"\"\r\n";
const char* CMD_CHECK_IP  = "AT+CIFSR\r\n";
const char* CMD_CONN_TYPE = "AT+CIPMUX=0\r\n";
const char* CMD_START_TCP = "AT+CIPSTART=\"TCP\",\"10.99.131.221\",8084\r\n"; 
const char* CMD_SEND      = "AT+CIPSEND=5\r\n";

// Global variables for UART circular buffer
volatile char rx_buffer[BUFFER_SIZE];
volatile uint16_t rx_head = 0, rx_tail = 0;
volatile char configRed, configGreen, configBlue, configYellow;
volatile char configPurple, configOrange, configBlack, configWhite, configBrown;

char color_array[9] = {0};
uint16_t avg_value_red, avg_value_green, avg_value_blue;

void UART1_Init(void) 
{
    IEC0bits.U1TXIE = 0;
    IEC0bits.U1RXIE = 0;
    
    // Unlock Peripheral Pin Select (PPS)
    __builtin_write_OSCCONL(OSCCON & 0xBF);
    RPOR3bits.RP7R = 3;     // Remap UART1 pin : U1TX on RB7
    RPINR18bits.U1RXR = 6;  // Remap UART1 pin : U1RX on RB6
    // Lock PPS
    __builtin_write_OSCCONL(OSCCON | 0x40);

    U1MODE = 0;   // Clear mode register
    U1STA = 0;    // Clear status register
    
    U1MODEbits.BRGH = 1;  // High-speed mode (4 clocks per bit)
    // For 115200 baud at FCY = 16MHz, U1BRG = (16e6/(4*115200))-1 ? 34
    U1BRG = 34;
    U1MODEbits.UARTEN = 1; // Enable UART module
    U1STAbits.UTXEN  = 1;  // Enable transmitter
    U1STAbits.URXEN  = 1;  // Enable receiver

    // Clear and enable UART RX interrupt
    IFS0bits.U1RXIF = 0;
    IEC0bits.U1RXIE = 1;
    IPC2bits.U1RXIP = 5;

    // Configure error interrupt handling
    IFS4bits.U1ERIF = 0;
    IEC4bits.U1ERIE = 1;
    IPC16bits.U1ERIP = 5;
}

void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void) // UART1 Receive Interrupt Handler
{
    IFS0bits.U1RXIF = 0;  // Clear receive interrupt flag
    while (U1STAbits.URXDA) 
    {  // Process data from FIFO
        uint16_t next_head = (rx_head + 1) % BUFFER_SIZE;
        if (next_head != rx_tail) 
        {  // Only store if buffer not full
            rx_buffer[rx_head] = U1RXREG;
            rx_head = next_head;
        }
    }
}

void __attribute__((interrupt, no_auto_psv)) _U1ErrInterrupt(void) // UART1 Error Interrupt Handler
{
    volatile uint16_t error_status = U1STA; // Read to clear errors
    if (U1STAbits.OERR) U1STAbits.OERR = 0;   // Clear overrun error
    if (U1STAbits.FERR) U1STAbits.FERR = 0;   // Clear framing error
    IFS4bits.U1ERIF = 0;                      // Clear error interrupt flag
}

void UART1_SendString(const char* str) // UART1 Send String Function
{
    while(*str) 
    {
        while(U1STAbits.UTXBF);  // Wait until transmit buffer is not full
        U1TXREG = *str++;
    }
}

void cleanBuffer() // UART1 Clean Buffer
{
    rx_head = 0;
    rx_tail = 0;
    // No need to zero buffer - new data overwrites old
	for(int i = 0; i < BUFFER_SIZE; i++)
    {
         rx_buffer[i] = 0;
    }
}

uint16_t UART_ReadString(char* dest, unsigned max_len) // UART1 Read String Function
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

bool UART_DataReady(void)
{    
    return !(rx_head == rx_tail);
}

void parseConfigMessage(char* msg) // Parsing the incoming Configuration Message
{
    char *p = strstr(msg, "CFG:");
    if (p != NULL)
        p += 4;
    
    // Tokenize the string using comma as delimiter.
    //char* token = strtok(msg, ",");
    char* token = strtok(p, ",");
    while (token != NULL) 
    {
        // Each token is expected to have format: Key=Value
        // For example: "Red=L"
        char* equalSign = strchr(token, '=');
        if (equalSign != NULL && *(equalSign + 1) != '\0') 
        {
            *equalSign = '\0';  // Split the string into two null-terminated strings
            char* key = token;
            char* value = equalSign + 1;
            // Now check the key and save the value into a global variable.
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

void sendMovementCommand(const char* side, const char* color) // Send the movement
{
    char dataMsg[BUFFER_SIZE];
    char atCommand[32];  // Buffer for the AT+CIPSEND command
  
    sprintf(dataMsg, "MOV#%s#%s\r\n", side, color); // Build the command message; the format is "MOV#<side>#<color>\r\n"
    
    // Calculate the length of the data message (not including the null terminator)
    int len = strlen(dataMsg);  // For "MOV#right#orange\r\n", len should be 18.
    
    sprintf(atCommand, "AT+CIPSEND=%d\r\n", len); // Build the AT command with the calculated length.
    
    UART1_SendString(atCommand); // Send the AT+CIPSEND command so the module is ready to accept the data.
    __delay32(8000000);          // Wait for the ">" prompt from the module (adjust delay as needed)
    cleanBuffer();
   
    UART1_SendString(dataMsg); // Now send the actual movement command message
    __delay32(8000000);        // Give time for the data transmission
    cleanBuffer();
}

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
void toggle_servo() //Toggle servo function
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

void moveRight() //Move right
{
   OC1R = SERVO_RIGHT;
}

void moveLeft() //Move left
{
   OC1R = SERVO_LEFT;
}

void moveCenter() //Move center
{
   OC1R = SERVO_CENTER;
}

void servo_set_angle(int angle) // This helper function maps an angle (0° to 180°) to the proper timer count value
{
    if(angle < 0) angle = 0;
    if(angle > 180) angle = 180;
    // Linear mapping: 0° => SERVO_MIN (1200 counts), 180° => SERVO_MAX (4800 counts)
    OC1R = (SERVO_RIGHT + (angle * 20)); //(SERVO_MIN + ((angle * (SERVO_MAX - SERVO_MIN)) / 180));  
}

void VEML3328_init()
{    
    i2c_write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x8011); // Shut down the sensor so configuration can be safely changed
    __delay32(100000);
    i2c_write2ByteRegister(VEML3328_SLAVE_ADD, CONF, 0x0430); // Enable sensor (Gain: 4x, Integration Time: 400ms)
    __delay32(100000);
}

void WS2812_Send_Byte(uint8_t byte) 
{
    for (int i = 7; i >= 0; i--)  
    {
        uint8_t spi_data = (byte & (1 << i)) ? 0b11110000 : 0b11000000; 
        SPI1_Exchange8bit(spi_data);
    }
}

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

volatile bool simulationEnabled = false; // Global flag to control simulation state (1 = running, 0 = paused)

void __attribute__((interrupt, no_auto_psv)) _INT1Interrupt(void) // Change Notification Interrupt Service Routine for RB15 button.
{
    if (PORTBbits.RB15 == 0) // // When the button is pressed, the simulationEnabled flag is toggled. (active low)
        simulationEnabled = !simulationEnabled;  
    IFS1bits.INT1IF = 0; // Clear the INT1 interrupt flag so that the ISR can be triggered again
}

void VEML3328_read()
{
    uint16_t sensor_value_red, sensor_value_green, sensor_value_blue;
    uint32_t value_red = 0, value_green = 0, value_blue = 0;
    char uart_string[100];

    for(int i = 0; i < NUM_SAMPLES; i++) // Loop to collect NUM_SAMPLES readings for each channel
    {
        sensor_value_red = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, R_DATA);       // Read 16-bit raw data from sensor
        value_red += (((sensor_value_red & 0xFF)<<8) | (sensor_value_red >> 8));    // Swap high/low bytes
        sensor_value_green = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, G_DATA);
        value_green += (((sensor_value_green & 0xFF)<<8) | (sensor_value_green >> 8));
        sensor_value_blue = i2c_read2ByteRegister(VEML3328_SLAVE_ADD, B_DATA);
        value_blue += (((sensor_value_blue & 0xFF)<<8) | (sensor_value_blue >> 8));
        
        __delay32(10000);
    }
    
    avg_value_red = value_red / NUM_SAMPLES;        // Averaged red channel value
    avg_value_green = value_green / NUM_SAMPLES;    // Averaged green channel value
    avg_value_blue = value_blue / NUM_SAMPLES;      // Averaged blue channel value 
    
    //__delay32(8000000);
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
    // RIGHT SET 0
    color_array[0] = 0; //red
    color_array[1] = 0; //green  
    color_array[2] = 0; //blue
    color_array[3] = 0; //black
    color_array[4] = 0;//white
    color_array[5] = 0; //orange
    color_array[6] = 0; //yellow
    color_array[7] = 0; //purple
    color_array[8] = 0; //brown
    
    // Initialization
    SYSTEM_Initialize();
    VEML3328_init();
    __delay32(16000000);     
    servo_init();
    UART1_Init();
    __delay32(8000000);      
    __builtin_enable_interrupts();
    __delay32(32000000);
    
    // WiFi Connection Sequence
    // Set WiFi mode to Station
    UART1_SendString(CMD_MODE);
    __delay32(8000000);
    cleanBuffer();
    // Connect to the WiFi network (SSID "etfbl.net" with no password)
    UART1_SendString(CMD_WIFI_CONN);
    __delay32(80000000); // Allow time for the join process (adjust delay if needed)
    cleanBuffer();
    
    // TCP Connection Sequence
    // Set connection mode to single connection
    UART1_SendString(CMD_CONN_TYPE);
    __delay32(16000000);
    cleanBuffer();
    //Establish a TCP connection with the remote IP and port
    UART1_SendString(CMD_START_TCP);
    __delay32(16000000);  // Allow time for connection to establish
    cleanBuffer();
    //Prepare to send data (3 bytes)
    UART1_SendString(CMD_SEND);
    __delay32(8000000);  // Wait for ">" prompt from the module
    cleanBuffer();
    // Now send the actual data
    UART1_SendString("ON!\r\n");
    __delay32(8000000); // Give time for data transmission
    
    while (1)
    {
        if (UART_DataReady()) 
        {
            __delay32(1000000);
            
           if (strstr(rx_buffer, "CFG:") != NULL) // Check for configuration message (starting with "CFG:")
           {
               rx_buffer[rx_head] = '\0';   // null-terminate the string
               parseConfigMessage(rx_buffer);
               populateColorArrayFromConfig();
               __delay32(4000000);
           }

           if (strstr(rx_buffer, "Start")!= NULL)
               simulationEnabled = true;

           if (strstr(rx_buffer, "Stop")!= NULL)
               simulationEnabled = false;

           cleanBuffer();      
        }
    
        if(simulationEnabled)
        {
            WS2812_SetColor(128, 128, 128);
            __delay32(8000);
            LATBbits.LATB14 = 1;
            VEML3328_read();
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