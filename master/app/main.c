#include "intrinsics.h"
#include <msp430fr2310.h>
#include <stdbool.h>

// ------- Global Variables ------------
volatile int i2c_state = 0;
volatile unsigned int curr_opcode = 0;
volatile unsigned int curr_operand = 0;
volatile int i2c_busy = 0;

int rot1A = 0;
int rot2A = 0;
int tx_switch = 0;

int callsign[] = {0, 0, 0, 0, 0, 0, 0, 0};
int active = 0;
int cursor_loc = 0;

const char valid_chars[] = {' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
                            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',
                            'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
                            'W', 'X', 'Y', 'Z'}; // Size 37
// -------------------------------------


void lcd_raw_send(int send_data, int num) {
    int send_data_temp;
    int nibble;
    int i = 0;
    int busy = 1;
    send_data_temp = send_data;

    while (i < num) {

        // Get top nibble
        if (num == 2) {
            nibble = send_data_temp & 0b11110000;
            send_data_temp = send_data_temp << 4;
        } else {
            nibble = send_data_temp & 0b00001111;
            send_data_temp = send_data_temp >> 4;
            nibble = nibble << 4;
        }
   
        // Output nibble
        P1OUT &= ~(BIT4 | BIT5 | BIT6 | BIT7);
        P1OUT |= nibble;

        // Set enable high
        P1OUT |= BIT1;
        _delay_cycles(1000);

        // Wait and drop enable
        _delay_cycles(1000);
        P1OUT &= ~BIT1;
        _delay_cycles(1000);

        i++;
    }
    
    P1DIR &= ~(BIT4 | BIT5 | BIT6 | BIT7); // Set input
    P1OUT |= BIT0;
    P2OUT &= ~BIT6;

    // Check busy flag
    while (busy != 0) {
        _delay_cycles(1000);
        P1OUT |= BIT1;       // Enable high
        _delay_cycles(1000);

        busy = P1IN & BIT7;  // Read busy

        _delay_cycles(1000);
        P1OUT &= ~BIT1;     // Enable low

        _delay_cycles(1000);
        P1OUT |= BIT1;     // Enable high
        _delay_cycles(2000);
        P1OUT &= ~BIT1;    // Enable low
    }
    _delay_cycles(1000);

    P1DIR |= (BIT4 | BIT5 | BIT6 | BIT7);  // Set output
    P1OUT &= ~BIT0;
}


void lcd_string_write(char* string) {
    int i = 0;

    while (string[i] != '\0') {
        P2OUT |= BIT6;
        lcd_raw_send((int)string[i], 2);
        i++;
    }
}

void update_top(char* string) {
    lcd_raw_send(0b00000010, 2);

    lcd_string_write(string);
}

void update_bottom(char* string) {
    lcd_raw_send((0x40 + 0b10000000), 2); // Start of the second row

    lcd_string_write(string);
}

void update_char(int pos, int char) {
    char string[] = {'\0', '\0'};
    string[0] = char;

    lcd_raw_send((0x40 + 0b10000000 + pos), 2);

    lcd_string_write(string);
}

int get_direction(int newA, int oldA, int b) {
    if (newA == oldA) {
        return 0;
    }

    if (newA > oldA) {
        if (b == 0) {
            return 1;
        } else {
            return -1;
        }
    } else {
        if (b == 1) {
            return 1;
        } else {
            return 0;
        }
    }
}

void i2c_update(unsigned int opcode, unsigned int oprerand) {
    while(i2c_busy);
    i2c_busy = 1;
    i2c_state = 0;

    curr_opcode = opcode;
    curr_operand = oprerand;

    UCB0CTLW0 |= UCTXSTT;
}

int main(void)
{

    int rot1Dir = 0;
    int rot2Dir = 0;

    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    P2OUT &= ~BIT0;
    P2DIR |= BIT0;

    P1OUT &= ~BIT0; // P1.0 is R/W
    P1DIR |= (BIT0 | BIT1 | BIT4 | BIT5 | BIT6 | BIT7);

    P2OUT &= ~BIT6; // RS PIN
    P2DIR |= BIT6;

    // Rotary encoder 1
    P6DIR &= ~(BIT0 | BIT1);
    
    // Rotary encoder 2
    P6DIR &= ~(BIT2 | BIT3);

    // Switch
    P5DIR &= ~BIT0;

    // Init I2C
    UCB0CTLW0 |= UCSWRST;  // Software reset
    UCB0CTLW0 |= UCSSEL__SMCLK;  // Input clock
    UCB0BRW = 10;
    UCB0CTLW0 |= UCMODE_3;  // I2C mode
    UCB0CTLW0 |= UCMST;
    UCB0CTLW0 |= UCTR;
    UCB0I2CSA = 0x0002;

    UCB0CTLW1 |= UCASTP_2;
    UCB0BCNT = 0x02;

    P1SEL0 |= BIT2;  // Confiure I2C pins  P1.3 SCL, P1.2 SDA
    P1SEL0 |= BIT3;
    P1SEL1 &= ~BIT2;
    P1SEL1 &= ~BIT3;

    // Disable low-power mode / GPIO high-impedance
    PM5CTL0 &= ~LOCKLPM5;

    UCB0CTLW0 &= ~UCSWRST;  // Disable software reset
    UCB0IE |= UCTXIE0; // Enable interrupts
    //UCB0CTLW0 |= UCTXACK;



    // Init LCD
    lcd_raw_send(0b110000100010, 3); // Turn on LCD in 2-line mode
    lcd_raw_send(0b00001100, 2); // Display on, cursor off, blink off
    lcd_raw_send(0b00000001, 2); // Clear display
    lcd_raw_send(0b00000110, 2); // Increment mode, entire shift off

    __enable_interrupt();

    update_top("Callsign      TX");

    //int i = 0;
    //while (true) {
    //    char c[] = {'\0', '\0'};
    //    c[0] = (char)i;
    //    lcd_string_write(&c);
    //    i++;
    //    i = i % 256;
    //}

    rot1A = P6IN & BIT0;
    rot2A = (P6IN & BIT2) >> 2;

    while (true)
    {
        // Check rot 1
        rot1Dir = get_direction(P6IN & BIT0, rot1A, (P6IN & BIT1) >> 1);
        rot1A = P6IN & BIT0;
        // Check rot 2
        rot1Dir = get_direction((P6IN & BIT2) >> 2, rot2A, (P6IN & BIT3) >> 3);
        rot2A = (P6IN & BIT2) >> 2;

        if (rot1Dir != 0) {
            cursor_loc += rot1Dir;
            cursor_loc = cursor_loc % 8;
        }

        if (rot2Dir != 0) {
            callsign[cursor_loc] += rot2Dir;
            callsign[cursor_loc] %= 37;

            i2c_update(cursor_loc, callsign[cursor_loc]);

            update_char(cursor_loc, valid_chars[callsign[cursor_loc]]);
        }

        if ((P5IN & BIT0) != tx_switch) {
            tx_switch = P5IN & BIT0;
            i2c_update(8, tx_switch);
        }
    }
}

#pragma vector=EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_I2C_ISR(void){
    int current = UCB0IV;

    switch(current) {
    case 0x18: // Tx interrupt
        if (i2c_state == 0) {
            UCB0TXBUF = curr_opcode;
            i2c_state++;
        }
        if (i2c_state == 1) {
            UCB0TXBUF = curr_operand;
            i2c_state++;
            i2c_busy = 0;
        }
        break;
    }
}