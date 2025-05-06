/*
 * File:   wifi.c
 * Author: Danilo
 *
 * Created on April 16, 2025, 1:24 PM
 */


#include "xc.h"
#include "wifi.h"

// AT Command Strings
const char* CMD_AT        = "AT\r\n";
const char* CMD_RST       = "AT+RST\r\n";
const char* CMD_GMR       = "AT+GMR\r\n";
const char* CMD_MODE      = "AT+CWMODE=1\r\n";
const char* CMD_WIFI_CONN = "AT+CWJAP=\"etfbl.net\",\"\"\r\n";
const char* CMD_CHECK_IP  = "AT+CIFSR\r\n";
const char* CMD_CONN_TYPE = "AT+CIPMUX=0\r\n";
const char* CMD_START_TCP = "AT+CIPSTART=\"TCP\",\"10.99.148.186\",8084\r\n"; 
const char* CMD_SEND      = "AT+CIPSEND=5\r\n";

// Global variables for UART circular buffer
volatile char rx_buffer[BUFFER_SIZE];
volatile uint16_t rx_head = 0, rx_tail = 0;
volatile char configRed, configGreen, configBlue, configYellow;
volatile char configPurple, configOrange, configBlack, configWhite, configBrown;

// ========================
// UART1 Initialization
// ========================
void UART1_Init(void) {
    IEC0bits.U1TXIE = 0;
    IEC0bits.U1RXIE = 0;
    
    // Unlock Peripheral Pin Select (PPS)
    __builtin_write_OSCCONL(OSCCON & 0xBF);
    // Remap UART1 pins: TX on RB7 and RX on RB6
    RPOR3bits.RP7R = 3;       // U1TX on RB7
    RPINR18bits.U1RXR = 6;    // U1RX on RB6
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
    IPC2bits.U1RXIP = 5;      // Set UART RX interrupt priority

    // Configure error interrupt handling
    IFS4bits.U1ERIF = 0;
    IEC4bits.U1ERIE = 1;
    IPC16bits.U1ERIP = 5;
    

}
// ========================
// UART1 Receive Interrupt Handler
// ========================

void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void) {
    IFS0bits.U1RXIF = 0;  // Clear receive interrupt flag

    while (U1STAbits.URXDA) {  // Process data from FIFO
        uint16_t next_head = (rx_head + 1) % BUFFER_SIZE;
        if (next_head != rx_tail) {  // Only store if buffer not full
            rx_buffer[rx_head] = U1RXREG;
            rx_head = next_head;
        }
    }
}

// ========================
// UART1 Error Interrupt Handler
// ========================
void __attribute__((interrupt, no_auto_psv)) _U1ErrInterrupt(void) {
    volatile uint16_t error_status = U1STA; // Read to clear errors
    if (U1STAbits.OERR) U1STAbits.OERR = 0;   // Clear overrun error
    if (U1STAbits.FERR) U1STAbits.FERR = 0;   // Clear framing error
    IFS4bits.U1ERIF = 0;                      // Clear error interrupt flag
}

// ========================
// UART1 Send String Function
// ========================
void UART1_SendString(const char* str) {
    while(*str) {
        while(U1STAbits.UTXBF);  // Wait until transmit buffer is not full
        U1TXREG = *str++;
    }
}
// ========================
// UART1 Clean Buffer
// ========================
void cleanBuffer() {
    rx_head = 0;
    rx_tail = 0;
    // No need to zero buffer - new data overwrites old
	for(int i = 0; i < BUFFER_SIZE; i++)
    {
         rx_buffer[i] = 0;
    }
}

// ========================
// UART1 Read String Function
// ========================

uint16_t UART_ReadString(char* dest, unsigned max_len) {
    uint16_t count = 0;
    IEC0bits.U1RXIE = 0;  // Disable interrupts
    
    while (count < max_len - 1 && rx_head != rx_tail) {
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

// ========================
// Parsing the incoming Configuration Message
// ========================

void parseConfigMessage(char* msg) {
    // Remove the "CFG:" prefix if present.
    if (strncmp(msg, "CFG:", 4) == 0) {
        msg += 4;
    }
    
    // Tokenize the string using comma as delimiter.
    char* token = strtok(msg, ",");
    while (token != NULL) {
        // Each token is expected to have format: Key=Value
        // For example: "Red=L"
        char* equalSign = strchr(token, '=');
        if (equalSign != NULL && *(equalSign + 1) != '\0') {
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

// ========================
// Send the movement
// ========================
void sendMovementCommand(const char* side, const char* color) {
    char dataMsg[BUFFER_SIZE];
    char atCommand[32];  // Buffer for the AT+CIPSEND command
    
    // Build the command message; the format is "MOV#<side>#<color>\r\n"
    sprintf(dataMsg, "MOV#%s#%s\r\n", side, color);
    
    // Calculate the length of the data message (not including the null terminator)
    int len = strlen(dataMsg);  // For "MOV#right#orange\r\n", len should be 18.
    
    // Build the AT command with the calculated length.
    sprintf(atCommand, "AT+CIPSEND=%d\r\n", len);
    
    // Send the AT+CIPSEND command so the module is ready to accept the data.
    UART1_SendString(atCommand);
    __delay32(4000000); // Wait for the ">" prompt from the module (adjust delay as needed)
    cleanBuffer();
   
    // Now send the actual movement command message
    UART1_SendString(dataMsg);
    __delay32(4000000);  // Give time for the data transmission
    cleanBuffer();
}