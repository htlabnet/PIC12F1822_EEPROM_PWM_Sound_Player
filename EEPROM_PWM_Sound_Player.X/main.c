/*
 * File:   main.c
 * Author: pcjpnet
 *
 * Created on 2017/01/11, 1:10
 */

#include <xc.h>

// CONFIG1
#pragma config FOSC = INTOSC    // Oscillator Selection (INTOSC oscillator: I/O function on CLKIN pin)
#pragma config WDTE = OFF       // Watchdog Timer Enable (WDT disabled)
#pragma config PWRTE = ON       // Power-up Timer Enable (PWRT enabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select (MCLR/VPP pin function is digital input)
#pragma config CP = OFF         // Flash Program Memory Code Protection (Program memory code protection is disabled)
#pragma config CPD = OFF        // Data Memory Code Protection (Data memory code protection is disabled)
#pragma config BOREN = ON       // Brown-out Reset Enable (Brown-out Reset enabled)
#pragma config CLKOUTEN = OFF   // Clock Out Enable (CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin)
#pragma config IESO = OFF       // Internal/External Switchover (Internal/External Switchover mode is disabled)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable (Fail-Safe Clock Monitor is disabled)

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)
#pragma config PLLEN = ON       // PLL Enable (4x PLL enabled)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enable (Stack Overflow or Underflow will cause a Reset)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)

#define _XTAL_FREQ 32000000


// for "SAM940_sound_256k_eeprom_binary.bin" @ 8kHz 8bit
const unsigned int rom_pos[4] = {0x0000, 0x2000, 0x4000, 0x6000};
const unsigned int rom_len[4] = {0x13FC, 0x1FCD, 0x0970, 0x018E};

// Current Position
unsigned int snd_pos = 0;

// Max Position
unsigned int snd_max = 0;

// First Flag
unsigned int flg_fst = 1;


void i2c_enable(void)
{
    SSP1STAT = 0b00000000;		// I2C 400kHz
	SSP1ADD = 19;				// Baud rate: 32MHz/((SSP1ADD + 1)*4) = 400kHz
	SSP1CON1 = 0b00101000;		// Enable I2C Master Mode
}


void i2c_disable(void)
{
	SSP1CON1 = 0b00001000;		// Disable I2C Master Mode
}


void i2c_wait(void)
{
	while ( ( SSP1CON2 & 0x1F ) || ( SSP1STATbits.R_nW ) );
}


void i2c_start(void)
{
	SSP1CON2bits.SEN = 1;	//  Start Condition Enabled bit
	i2c_wait();
}


void i2c_repeat_start(void)
{
	SSP1CON2bits.RSEN = 1;	//  Start Condition Enabled bit
	i2c_wait();
}


void i2c_stop(void)
{
	SSP1CON2bits.PEN = 1;	// Stop Condition Enable bit
	i2c_wait();
}


void i2c_send_byte(unsigned char data)
{
	SSP1BUF = data;
	i2c_wait();
}


unsigned char i2c_read_byte(unsigned int ack)
{
	SSP1CON2bits.RCEN = 1;
	i2c_wait();
	unsigned char data = SSP1BUF;
	i2c_wait();
 
	if(ack) SSP1CON2bits.ACKDT = 0;		// ACK
	else SSP1CON2bits.ACKDT = 1;		// NO_ACK
 
	SSP1CON2bits.ACKEN = 1;
 
	i2c_wait();
	return data;
}



void play_sound(unsigned int sel)
{
    // PLAY SOUND
    
    if (flg_fst == 0){
        snd_pos = snd_max;
        __delay_ms(10);
        CCPR1L = i2c_read_byte(0);
        __delay_ms(10);
        i2c_stop();
        __delay_ms(30);
    } else {
        flg_fst = 0;
    }

    unsigned char addr_h;
    unsigned char addr_l;
    addr_l = rom_pos[sel] & 0x00FF;
    addr_h = rom_pos[sel] >> 8;
    
    i2c_start();
    __delay_ms(1);
    i2c_send_byte(0b10100000); //WRITE ADDRESS
    __delay_ms(1);
    i2c_send_byte(addr_h); //WRITE ADDRESS
    __delay_ms(1);
    i2c_send_byte(addr_l); //WRITE ADDRESS
    __delay_ms(10);

    i2c_repeat_start();
    __delay_ms(1);
    i2c_send_byte(0b10100001); //READ DATA
    __delay_ms(10);

    snd_pos = 0;
    snd_max = rom_len[sel];

    __delay_ms(1000);
}


void interrupt InterTimer(void)
{
    if (TMR0IF == 1) {
        TMR0 = 10;
        TMR0IF = 0;
        if (snd_pos < snd_max) {
            CCPR1L = i2c_read_byte(1);
            snd_pos++;
        } else {
            CCPR1L = 0;
        }
    }
}


void main(void)
{
    // PORT SETTINGS
    OSCCON = 0b01110000;  // Internal Clock: 32MHz
    ANSELA = 0b00000000;  // Set Digital Port
    TRISA  = 0b00011111;  // INPUT: RA4,3,2,1,0
    WPUA   = 0b00011111;  // INPUT: RA4,3,2,1,0
    PORTA  = 0b00000000;  // OUTPUT: LOW

    // PWM SETTINGS
    CCP1SEL = 1;
    CCP1CON = 0b00001100;
    T2CON   = 0b00000000;
    CCPR1L  = 0;
    CCPR1H  = 0;
    TMR2    = 0;
    PR2     = 0b11111111 ;
    TMR2ON  = 1;

    // TIMER SETTINGS
    OPTION_REG = 0b00000001;
    TMR0   = 0;
    TMR0IF = 0;
    TMR0IE = 1;
    GIE    = 1;

    // SOUND SELECT
    snd_max = rom_len[0];
    snd_pos = snd_max;

    // EEPROM
    i2c_enable();

    __delay_ms(500);
    
    while(1) {
        if (RA0 == 0) {
            play_sound(0);
        }
        if (RA3 == 0) {
            play_sound(1);
        }
        if (RA4 == 0) {
            play_sound(2);
        }
        __delay_ms(100);
    }
    
    i2c_disable();
    return;
}

