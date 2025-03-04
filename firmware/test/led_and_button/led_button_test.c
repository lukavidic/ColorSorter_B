void main() {

  unsigned int old_state = 0;
  
  ANSB.F14 = 0; // RB14 as digital output (LED)
  ANSB.F15 = 0; // RB15 as digital input (switch)
  ANSB.F0 = 0;
  ANSB.F1 = 0;
  TRISB.F0 = 1;
  TRISB.F1 = 1;
  TRISB.F14 = 0;                // Initialize PORTB as output
  TRISB.F15 = 1;                // Initialize PORTB as input
  LATB.F14 = 0;                 // Set PORTB to zero

  while(1) {

    if(PORTB.F15 == 1 && old_state == 0)
        {
         Delay_ms(80);
         if(PORTB.F15 == 0)
                    {
                     LATB.F14 = ~LATB.F14;
                     old_state = 1;
                     }
        }
    else
    {
    if(PORTA.F15 == 1)
     old_state = 0;
    }
  }
}