#include "xc.h"
#include <stdint.h>
#define FCY 4000000UL          // 8MHz oscillator gives 4MHz instruction cycle
#include <libpic30.h>

#define PWM_FREQ 50           // 50 Hz PWM (20 ms period)
#define _XTAL_FREQ 8000000UL
#define SERVO_MIN 300         // Timer counts corresponding to 1.0ms pulse (0°)
#define SERVO_MAX 1200        // Timer counts corresponding to 2.0ms pulse (180°)

// Initialize Timer2 and Output Compare 1 (OC1) for PWM generation
void servo_init() {
    T2CONbits.TON = 0; 			   // Disable Timer2
    TRISBbits.TRISB5 = 0;          // Configure RB5 as output
    
    __builtin_write_OSCCONL(OSCCON & 0xBF);  // Clear the IOLOCK bit (unlock PPS)
    RPOR2bits.RP5R = 0x000D;                 // Map OC1 function to RP5 (RB5)
    __builtin_write_OSCCONL(OSCCON | 0x40);  // Lock PPS registers
    

    T2CONbits.TCKPS = 0b01;        // Prescaler 1:8

	
    TMR2 = 0;                     // Reset Timer2
	
    OC1CON1bits.OCTSEL = 0;       // Timer2 as clock source
    OC1CON1bits.OCM = 0b110;      // PWM mode (fault disabled)
    OC1CON2bits.SYNCSEL = 0x1F;   // Self-sync mode
    OC1CON2bits.OCTRIG = 0;       // Internal trigger
    
                                 
    T2CONbits.TON = 1;            // Enable Timer2                           
    OC1R  = SERVO_MIN;            // Initial duty cycle
    PR2   = 9999;                 // PWM period = (PR2+1) * (Tcy * prescaler)
    OC1RS = PR2;

   
   
   
}

// This helper function maps an angle (0° to 180°) to the proper timer count value
void servo_set_angle(int angle) {
    if(angle < 0) angle = 0;
    if(angle > 180) angle = 180;
    // Linear mapping: 0° => SERVO_MIN (300 counts), 180° => SERVO_MAX (1200 counts)
    OC1R = (SERVO_MIN + (angle * 5)); //(SERVO_MIN + ((angle * (SERVO_MAX - SERVO_MIN)) / 180));
    
}

int main(void)
{
    
    
    ANSB = 0;
    servo_init();
    LATBbits.LATB5 = 0;    // Initialize output low
    TRISBbits.TRISB14 = 0; // Set RB14 as output (LED diode)
    LATBbits.LATB14 = 1;  // Initialize output high
  

    while(1)
    {
        for (int angle = 0; angle <= 180; angle += 30) 
		{
           servo_set_angle(angle);
           LATBbits.LATB14 = ~LATBbits.LATB14; // Change diode light
           __delay_ms(1000);                   // Wait 1 second between each step
    
        }
       
       
    }
    
    return 0;
}