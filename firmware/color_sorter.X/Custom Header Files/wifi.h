/* Microchip Technology Inc. and its subsidiaries.  You may use this software 
 * and any derivatives exclusively with Microchip products. 
 * 
 * THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER 
 * EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED 
 * WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A 
 * PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP PRODUCTS, COMBINATION 
 * WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION. 
 *
 * IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
 * INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
 * WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS 
 * BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE 
 * FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS 
 * IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF 
 * ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *
 * MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE OF THESE 
 * TERMS. 
 */

/* 
 * File:   
 * Author: 
 * Comments:
 * Revision history: 
 */
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
// This is a guard condition so that contents of this file are not included
// more than once.  
#ifndef XC_HEADER_TEMPLATE_H
#define	XC_HEADER_TEMPLATE_H

#include <xc.h> // include processor files - each processor file is guarded.  

// Circular Buffer Size for UART
#define BUFFER_SIZE 128


// AT Command String Constants 
extern const char* CMD_AT;
extern const char* CMD_RST;
extern const char* CMD_GMR;
extern const char* CMD_MODE;
extern const char* CMD_WIFI_CONN;
extern const char* CMD_CHECK_IP;
extern const char* CMD_CONN_TYPE;
extern const char* CMD_START_TCP;
extern const char* CMD_SEND;

// Global variables for UART circular buffer 
extern volatile char rx_buffer[BUFFER_SIZE];
extern volatile uint16_t rx_head;
extern volatile uint16_t rx_tail;

// Global variables for configuration 
extern volatile char configRed;
extern volatile char configGreen;
extern volatile char configBlue;
extern volatile char configYellow;
extern volatile char configPurple;
extern volatile char configOrange;
extern volatile char configBlack;
extern volatile char configWhite;
extern volatile char configBrown;

void UART1_Init(void);
void __attribute__((interrupt, no_auto_psv)) _U1RXInterrupt(void);
void __attribute__((interrupt, no_auto_psv)) _U1ErrInterrupt(void);
void UART1_SendString(const char* str);
void cleanBuffer();
uint16_t UART_ReadString(char* dest, unsigned max_len);
bool UART_DataReady(void);
void parseConfigMessage(char* msg);
#endif


