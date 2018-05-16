#include <stdint.h>
#include "inc/tm4c123gh6pm.h"
#include "Waveforms.h" //Contains the lookup table for different waveforms

void DisableInterrupts(void);
void EnableInterrupts(void);
void WaitForInterrupt(void);
void GPIOPort_Init(void);
void GPIOPortF_Handler(void);
void delay_32us();
int flag=1,k=0,x;
int f=10;                           /* The frequency right now is 10 Hz; so, we must send 1 sample in 100/120 ms */
extern int wt[4][120];              /* The waveform table is initialized as an external variable */

void LTC1661_Write(int data)
{
    GPIO_PORTB_DATA_R &= ~0x04;     /* Assert Slave Select low, as it is active low */
    data |= 0xA000;                 /* Append A000 as the control word for LTC1661*/

    while((SSI1_SR_R & 2) == 0);    /* Wait until transmit FIFO not full */
    SSI1_DR_R = data >> 8;          /* Transmit higher byte */

    while((SSI1_SR_R & 2) == 0);    /* Wait until FIFO not full */
    SSI1_DR_R = data & 0xFF;        /* Transmit low byte */

    while(SSI1_SR_R & 0x10);        /* Wait until transmit complete */
    GPIO_PORTB_DATA_R |= 0x04;      /* Keep Slave Select idle high */
}


void GPIOPort_Init(void)
{
    volatile unsigned long del;

    SYSCTL_RCGC2_R|=0x0000002F;     /* For assigning to the variable del*/
    SYSCTL_RCGCSSI_R |= 2;          /* Enable clock to SSI1 */
    SYSCTL_RCGCGPIO_R |= 0xFF;      /* Enable clock to all GPIOs-we'll be using Ports B,D,F */
    del = SYSCTL_RCGC2_R;           /* Allow time for clock to start */

    /* Configure PORTD 1 and 3 for SSI1 clock and Tx respectively */
    GPIO_PORTD_AMSEL_R &= ~0x09;    /* Disable analog for these pins */
    GPIO_PORTD_DEN_R |= 0x09;       /* And make them digital */
    GPIO_PORTD_AFSEL_R |= 0x09;     /* Enable alternate function */
    GPIO_PORTD_PCTL_R &= ~0x0000F00F; /* Assign pins to SSI1 */
    GPIO_PORTD_PCTL_R |= 0x00002002;  /* Assign pins to SSI1 */

    GPIO_PORTF_LOCK_R = 0x4C4F434B; /* Unlock GPIO PortF */
    GPIO_PORTF_CR_R = 0x1F;         /* Allow changes to PF4-0 */
    GPIO_PORTF_AMSEL_R = 0x00;      /* Disable analog on PF */
    GPIO_PORTF_PCTL_R = 0x00000000; /* PCTL GPIO on PF4-0 */
    GPIO_PORTF_DIR_R = 0x0E;        /* PF4,PF0 in, PF3-1 out-we'll be using switches for changing waveform type and frequency */
    GPIO_PORTF_AFSEL_R = 0x00;      /* Disable alt funct on PF */
    GPIO_PORTF_PUR_R = 0x11;        /* Enable pull-up on PF0 and PF4 */
    GPIO_PORTF_DEN_R = 0x1F;        /* Enable digital I/O on PF4-0 */

    GPIO_PORTF_IS_R = 0x00;         /*  PF0-4 is edge-sensitive */
    GPIO_PORTF_IBE_R = 0x00;        /*  PF0-4 is not both edges */
    GPIO_PORTF_IEV_R= 0x11;         /*  PF0-4 rising edge event */
    GPIO_PORTF_ICR_R = 0xFF;        /*  Clear interrupt flags */
    GPIO_PORTF_IM_R |= 0x11;        /*  Arm interrupt on PF4 and PF0 */
    NVIC_PRI7_R = (NVIC_PRI7_R & 0xFF1FFFFF) | 0x00200000; /*  Priority 1 */
    NVIC_EN0_R = 0x40000000;        /*  Enable interrupt 30 in NVIC */
    GPIO_PORTF_DATA_R |= 0x02;       /* Initially, red light on-we'll get a sine wave output */

    /*PB2 is being used as the slave select line for the ADC*/
    GPIO_PORTB_LOCK_R = 0x4C4F434B; /* Unlock GPIO PortB */
    GPIO_PORTB_CR_R = 0xFF;         /* Allow changes */
    GPIO_PORTB_DIR_R = 0xFF;        /* PB out */
    GPIO_PORTB_DEN_R = 0xFF;        /* Enable digital I/O on PB */

    /* SPI Master, POL = 0, PHA = 0, clock = 4 MHz, 16 bit data */
    SSI1_CR1_R = 0;                 /* Disable SSI and make it master */
    SSI1_CC_R = 0;                  /* Use system clock */
    SSI1_CPSR_R = 2;                /* Prescaler divided by 2 */
    SSI1_CR0_R = 0x0007;            /* 8 MHz SSI clock, SPI mode, 8 bit data */
    SSI1_CR1_R |= 2;                /* Enable SSI1 */
}

void GPIOPortF_Handler(void)
{
    volatile int readback;

    /* Press SW1(PF4) for changing frequency and SW2(PF0) for changing the flag, which decides the type of waveform */
    if (GPIO_PORTF_RIS_R & 0x10){   /* Checking if the interrupt came from SW1 */
        f+=10.00;                   /* Increasing f by 10 increases frequency by almost 6 Hz */
        GPIO_PORTF_ICR_R=0xFF;      /* Clear interrupt */
    }
    else if (GPIO_PORTF_RIS_R & 0x01){ /* Checking if the interrupt came from SW1 */
        k=0;                        /* When the waveform is changed, we start from the 1st value in the LUT for that waveform */
        flag++;                     /* Increment flag to move on to the next waveform */
        if (flag==5)
            flag=1;
        GPIO_PORTF_ICR_R=0xFF;      /* Clear interrupt */
        switch(flag){
        case 1:
            GPIO_PORTF_DATA_R=0x02;
            break;
        case 2:
            GPIO_PORTF_DATA_R=0x04;
            break;
        case 3:
            GPIO_PORTF_DATA_R=0x08;
            break;
        case 4:
            GPIO_PORTF_DATA_R=0x06;
            break;
        }

    }
}

/* The following function produces a constant delay of 32 us */
void delay_32us()
{
    SYSCTL_RCGCTIMER_R |= 2;        /* Enable clock to Timer Block 1 */

    TIMER1_CTL_R = 0;               /* Disable Timer before initialization */
    TIMER1_CFG_R = 0x04;            /* 16-bit option */
    TIMER1_TAMR_R = 0x02;           /* Periodic mode and down-counter */
    TIMER1_TAILR_R = 512;           /* TimerA interval load value reg */
    TIMER1_ICR_R = 0x1;             /* Clear the TimerA timeout flag */
    TIMER1_CTL_R |= 0x01;           /* Enable Timer A after initialization */

    while ((TIMER1_RIS_R & 0x1) == 0);/* wait for TimerA timeout flag */
}

int main(void)
{
    int i;
    GPIOPort_Init();               /* Initialize GPIO Ports */
    EnableInterrupts();            /* Enable global Interrupt flag (I) */

    while(1) {
        switch(flag){
        case 1:
            LTC1661_Write(wt[0][k]);/*0th row of he LUT contains the values for sine waveform */
            x=(int)(100000/(96*f));/* Number of times we call the 32 us delay before sending out a sample */
            for (i=0;i<x;i++)      /* Delay loop */
                delay_32us();
            k=k+1;                 /* Increment k to go the next value in LUT  */
            if(k>=120)
                k=0;
            break;
        case 2:
            LTC1661_Write(wt[1][k]);
            x=(int)(100000/(96*f));
            for (i=0;i<x;i++)
                delay_32us();
            k=k+1;
            if(k>=120)
                k=0;
            break;
        case 3:
            LTC1661_Write(wt[2][k]);
            x=(int)(100000/(96*f));
            for (i=0;i<x;i++)
                delay_32us();
            k=k+1;
            if(k>=120)
                k=0;
            break;
        case 4:
            LTC1661_Write(wt[3][k]);
            x=(int)(100000/(96*f));
            for (i=0;i<x;i++)
                delay_32us();
            k=k+1;
            if(k>=120)
                k=0;
            break;
            }
    }
}

void DisableInterrupts(void)
{
    __asm ("    CPSID  I\n");
}

void EnableInterrupts(void)
{
    __asm  ("    CPSIE  I\n");
}

void WaitForInterrupt(void)
{
    __asm  ("    WFI\n");
}
