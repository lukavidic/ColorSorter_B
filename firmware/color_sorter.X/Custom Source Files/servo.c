#include "servo.h"
#include "mcc_generated_files/system.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <xc.h>
#define FCY 16000000UL
#include <libpic30.h>

//======================================
void servo_init() {
    T2CONbits.TON = 0;   // Disable Timer2
    TRISBbits.TRISB5 = 0;          // Configure RB5 as output
   
   
    T2CONbits.TCKPS = 0b01;        // Prescaler 1:8


    TMR2 = 0;                     // Reset Timer2

    OC1CON1bits.OCTSEL = 0;       // Timer2 as clock source
    OC1CON1bits.OCM = 0b110;      // PWM mode (fault disabled)
    OC1CON2bits.SYNCSEL = 0x1F;   // Self-sync mode
    OC1CON2bits.OCTRIG = 0;       // Internal trigger
   
                                 
    T2CONbits.TON = 1;            // Enable Timer2  
    OC1R = SERVO_CENTER;             // Initial duty cycle
    PR2 = 39999;                  // 20ms period: (39999 + 1) * (8/16MHz) = 20ms
   
    OC1RS = PR2;

 }

//Toggle servo function
uint16_t counter = 0;
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

//Move right
void moveRight()
{
   OC1R = SERVO_RIGHT;
   //LATBbits.LATB14 = 1;
}
//Move left
void moveLeft()
{
   OC1R = SERVO_LEFT;
   //LATBbits.LATB14 = 1;
}
//Move center
void moveCenter()
{
   OC1R = SERVO_CENTER;
   //LATBbits.LATB14 = 1;
}
// This helper function maps an angle (0° to 180°) to the proper timer count value
void servo_set_angle(int angle) {
    if(angle < 0) angle = 0;
    if(angle > 180) angle = 180;
    // Linear mapping: 0° => SERVO_MIN (1200 counts), 180° => SERVO_MAX (4800 counts)
    OC1R = (SERVO_RIGHT + (angle * 20)); //(SERVO_MIN + ((angle * (SERVO_MAX - SERVO_MIN)) / 180));
   
}

