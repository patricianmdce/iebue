#include <p30F4011.h>
#include <libpic30.h>
#include <timer.h>
#include <stdio.h>

_FOSC(CSW_FSCM_OFF & XT_PLL16);  //oscilator at 16x cristal frequency
_FWDT(WDT_OFF);                  //watchdog timer off

#define FCY  29491              //number of instructions per milisecond Fclock=7372800Hz and PLL=16
#define LED1   _LATF0           //define port for LED1
#define LED2   _LATF1           //define port for LED1

int i, flag;
int setpoint = 6500;      		               	//initial setpoint for the lap period
int status, lap=0;					//global variables to store the status of the input and the lap time

/*This is the Interrupt routine that measures each lap time and calculates the PID correction*/
void __attribute__((interrupt, no_auto_psv)) _CNInterrupt(void)
{
    status = _RC14;         //senses the input pin and saves its status
    LED1 = ~LED1;           //blinks LED2 so that we know it measured the passage of the slit
    _CNIF = 0;          	//clears Change Notification interrupt flag
}

/*This is global var to use with UART2*/
char c;
/* This is UART2 receive ISR */
void __attribute__((__interrupt__, __no_auto_psv__)) _U2RXInterrupt(void)
{
    IFS1bits.U2RXIF = 0;  	//resets RX2 interrupt flag
    c = U2RXREG;          	//reads the character to the c variable
}

/* This is a software delay routine*/
void delay_ms(unsigned int delay)
{
    unsigned int cycles;
    for(; delay; delay--)
        for(cycles=FCY; cycles; cycles--);
}


/*This is the UART2 configuration routine*/
void uart_config(void)
{
    /**********************************
      SerialPort configuration
    **********************************/
    U2MODEbits.UARTEN = 0;  // Bit15 TX, RX DISABLED, ENABLE at end of func
    U2MODEbits.USIDL = 0;   // Bit13 Continue in Idle
    U2MODEbits.WAKE = 0;    // Bit7 No Wake up (since we don't sleep here)
    U2MODEbits.LPBACK = 0;  // Bit6 No Loop Back
    U2MODEbits.ABAUD = 0;   // Bit5 No Autobaud (would require sending '55')
    U2MODEbits.PDSEL = 0;   // Bits1,2 8bit, No Parity
    U2MODEbits.STSEL = 0;   // Bit0 One Stop Bit

    // Load a value into Baud Rate Generator.  Example is for 11500.
    //  U2BRG = (Fcy/(16*BaudRate))-1
    //  U2BRG = (29491200/(16*115200))-1
    //  U2BRG = 15
    U2BRG = 15;

    IFS1bits.U2RXIF = 0;    // Clear the Receive Interrupt Flag
    IEC1bits.U2RXIE = 1;    // Enable Receive Interrupts

    U2MODEbits.UARTEN = 1;  // And turn the peripheral on

    __C30_UART = 2;

    /**********************************
      End of serialPort configuration
    **********************************/
}


/* This is TMR2 ISR */
void __attribute__((interrupt, auto_psv)) _T2Interrupt(void)
{
    IFS0bits.T2IF = 0;    /* Clear Timer interrupt flag */
    LED1 = ~LED1;
    LED2 = ~LED2;
    i++;
    if (i==10)
        {
            T2CONbits.TON = 0;
            flag = 1;
            i=0;
        }

}

/* This is TMR3 ISR */
void __attribute__((interrupt, auto_psv)) _T3Interrupt(void)
{
    IFS0bits.T3IF = 0;    /* Clear Timer interrupt flag */
}



int main () {

    /* Beginning of execution */

    uart_config();          //configs the UART2

    printf("Serial port says: Hello\n\r");  //checks if UART2 is working fine

    _TRISF0 = 0;        	//Configure led port as output
    _TRISF1 = 0;            //Configure led port as output

    /*TIMER 3*/
    T3CONbits.TON = 0;      //Timer_3 is OFF
    TMR3 = 0;               //resets Timer_3
    PR3 = 0Xffff;           //sets the maximum count for Timer_3
    T3CONbits.TCS = 0;      //choose FCY as clock source for Timer_3
    T3CONbits.TCKPS = 0x02; //sets the Timer_3 pre-scaler to 1
    IFS0bits.T3IF = 0;      //clears Timer_3 interrupt flag
    _T3IE = 0;     	       	//disable Timer_3 Interrupts
    T3CONbits.TON = 1;      //turns Timer_3 OFF



    /*INPUT CAPTURE*/

    IC2CONbits.ICM = 2;     //Input capture mode - every falling edge
    IC2CONbits.ICTMR = 0;   //TMR3 contents are captured on capture event
    IFS0bits.IC2IF = 0;     //Interrupt flag off
    IEC0bits.IC2IE = 1;     //Interrupt enable on



    /*PWM freq timer setup*/
    T2CONbits.TON = 0;      //Timer_2 is OFF
    TMR2 = 0;               //resets Timer_2
    PR2 = 737;              //sets the maximum count for Timer_2
    T2CONbits.TCS = 0;      //choose FCY as clock source for Timer_2
    T2CONbits.TCKPS = 0x00; //sets the Timer_2 pre-scaler to 1
    IFS0bits.T2IF = 0;      //clears Timer_2 interrupt flag
    _T2IE = 1;     	       	//disable Timer_2 Interrupts
    T2CONbits.TON = 0;      //turns Timer_2 OFF


    /*PWM config for OC1*/
    OC1RS = PR2/2;          //sets the initial duty_cycle;
    OC1CONbits.OCM =6;      //set OC1 mode to PWM
    OC1R = 0;               //Initial Delay (only for the first cycle)
    OC1CONbits.OCTSEL = 0;  //selects Timer_2 as the OC1 clock source
    T2CONbits.TON = 1;      //turns Timer_2 ON (starts PWM generation at OC1)

    /*PWM config for OC3*/
    OC3R = PR2/2;           //sets the initial duty_cycle;
    OC3CONbits.OCM =5;      //set OC3 mode to PWM
    OC3RS = 0;              //Initial Delay (only for the first cycle)
    OC3CONbits.OCTSEL = 0;  //selects Timer_2 as the OC3 clock source





    LED1 = 1;		//Blinking LEDs
    LED2 = 0;

    /*main cycle*/
    while(1)    //do forever
        {
            if (flag==1)
                {
                    delay_ms(50);
                    flag=0;
                    TMR3 = 0;
                    T2CONbits.TON = 1;      //turns Timer_2 ON
                }

            

        }//end while forever
}//end main



