#include "intrinsics.h"
#include <msp430fr2310.h>
#include <stdbool.h>

// ------- Global Variables ------------
volatile int i2c_state = 0;
volatile int opcode = 0;
volatile int status[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

volatile int active = 0;
volatile int curr_char = 0;
volatile int curr_unit = 1; // one-indexed
volatile int curr_sub_unit = 1; // one-indexed

const int slave_addr = 0x0002;
const int char_lengths = {
    3, // ' '
    5, // 0
    5, // 1
    5, // 2
    5, // 3
    5, // 4
    5, // 5
    5, // 6
    5, // 7
    5, // 8
    5, // 9
    2, // A
    4, // B
    4, // C
    3, // D
    1, // E
    4, // F
    3, // G
    4, // H
    2, // I
    4, // J
    3, // K
    4, // L
    2, // M
    2, // N
    3, // O
    4, // P
    4, // Q
    3, // R
    3, // S
    1, // T
    3, // U
    3, // V
    3, // W
    4, // X
    4, // Y
    4 // Z
};
const int char_def = {
    0b000, // ' '
    0b11111, // 0
    0b01111, // 1
    0b00111, // 2
    0b00011, // 3
    0b00001, // 4
    0b00000, // 5
    0b10000, // 6
    0b11000, // 7
    0b11100, // 8
    0b11110, // 9
    0b01, // A
    0b1000, // B
    0b1010, // C
    0b100, // D
    0b0, // E
    0b0010, // F
    0b110, // G
    0b0000, // H
    0b00, // I
    0b0111, // J
    0b101, // K
    0b0100, // L
    0b11, // M
    0b10, // N
    0b111, // O
    0b0110, // P
    0b1101, // Q
    0b010, // R
    0b000, // S
    0b1, // T
    0b001, // U
    0b0001, // V
    0b011, // W
    0b1001, // X
    0b1011, // Y
    0b1100 //Z
};
// -------------------------------------

int main(void)
{
    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;


    // Set oscillating pin
    P1DIR |= BIT1;

    // Set PTT pin
    P1DIR |= BIT0;

    // Setup timer B0
    TB0CTL |= TBCLR;
    TB0CTL |= TBSSEL__SMCLK;
    TB0CTL |= MC__UP;
    TB0CCR0 = 1136; // 1/2 440 Hz

    // Setup timer B1
    TB0CTL |= TBCLR;
    TB0CTL |= TBSSEL__ACLK;
    TB0CTL |= MC__UP;
    TB0CCR0 = 6553; // ~ 5 WPM

    // Init I2C
    UCB0CTLW0 |= UCSWRST;  // Software reset
    UCB0CTLW0 |= UCSSEL__SMCLK;  // Input clock
    UCB0BRW = 10;
    UCB0CTLW0 |= UCMODE_3;  // I2C mode
    UCB0I2COA0 = slave_addr | UCOAEN; // My slave address
    //UCB0CTLW1 |= UCSWACK;

    P1SEL0 |= BIT2;  // Confiure I2C pins
    P1SEL0 |= BIT3;
    P1SEL1 &= ~BIT2;
    P1SEL1 &= ~BIT3;

    // Disable low-power mode / GPIO high-impedance
    PM5CTL0 &= ~LOCKLPM5;

    UCB0CTLW0 &= ~UCSWRST;  // Disable software reset
    UCB0IE |= (UCRXIE0 | UCSTPIE); // Enable interrupts
    //UCB0CTLW0 |= UCTXACK;

    // Timer 0 interrupts
    TB0CCTL0 &= ~CCIFG;
    TB0CCTL0 |= CCIE;

    __enable_interrupt();


    while (true)
    {
    }
}

#pragma vector=EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_I2C_ISR(void){
    int current = UCB0IV;
    int read_data;

    switch(current) {
    case 0x08: // Rx stop condition
        i2c_state = 0;
        break;
    case 0x16: // Rx data
        read_data = UCB0RXBUF;
        switch(i2c_state) {
        case 0: // Rx top level mode
            opcode = read_data;
            break;
        case 1: // Rx temp
            status[opcode] = read_data;
            break;
        }
        i2c_state++;
        break;
    }
}

#pragma vector=TIMER0_B0_VECTOR
__interrupt void TIMER0_ISR(void) {
    if (status[8] && active) {
        P1OUT ^= BIT1;
    } else {
        P1OUT &= ~BIT1;
    }
    TB0CCTL0 &= ~CCIFG;
}

#pragma vector=TIMER1_B0_VECTOR
__interrupt void TIMER1_ISR(void) {
    int definition = 0;
    if (curr_char == 8) {
        active = 0;
        curr_unit++;
        if (curr_unit > 7) {
            curr_unit = 1;
            curr_sub_unit = 1;
            curr_char = 0;
        }
    } else {
        definition = char_def[status[curr_char]];

        if (defintion >> (char_lengths[status[curr_char]] - curr_unit)) {
            // Play dash
            if (curr_sub_unit > 3) {
                active = 0;
            } else {
                active = 1;
            }

            curr_sub_unit++;
            if (curr_sub_unit > 4) {
                curr_unit++;
                curr_sub_unit = 1;
            }
        } else {
            // Play dot
            if (curr_sub_unit > 1) {
                active = 0;
            } else {
                active = 1;
            }

            curr_sub_unit++;
            if (curr_sub_unit > 2) {
                curr_unit++;
                curr_sub_unit = 1;
            }
        }

        if (curr_unit > char_lengths[status[curr_char]]) {
            curr_char++;
            curr_unit = 1;
        }
    }



    int definition = char_def[status[curr_char]]
    status[curr_char]


    TB0CCTL0 &= ~CCIFG;
}