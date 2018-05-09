/*
 * midtermTransmit.c
 *
 * Created: 4/29/2018 1:43:31 PM
 * Author : Oji
 */ 

#define F_CPU 16000000UL
#define FOSC 16000000	// Clock Speed
#define BAUD 9600
#define MYUBRR FOSC/16/BAUD-1

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h> // need for uint8_t
#include <util/delay.h> //delay header
#include <string.h>
#include "nrf24l01.h"
#include <stdio.h>

void setup_timer(void);
nRF24L01 *setup_rf(void);
void ADCinit(void);

void USARTinit(void);
void USARTsend(unsigned char);
static int uart_putchar(char c, FILE *stream)
{
	if (c == '\n') uart_putchar('\r', stream);
	
	loop_until_bit_is_set(UCSR0A, UDRE0);
	UDR0 = c;
	
	return 0;
}
static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

volatile float temp;
char c[10];
volatile uint8_t ADCvalue;
volatile bool rf_interrupt = false;
volatile bool send_message = false;
int main(void) {
	uint8_t to_address[5] = { 0x01, 0x01, 0x01, 0x01, 0x01 };
	USARTinit();
	ADCinit();
	bool on = false;
	sei();
	nRF24L01 *rf = setup_rf();
	setup_timer();
	while (true) {
	if (rf_interrupt) {
		rf_interrupt = false;
		int success = nRF24L01_transmit_success(rf);
		if (success != 0)
			nRF24L01_flush_transmit_message(rf);
		}
		if (send_message) {
			send_message = false;
			on = !on;
			nRF24L01Message msg;
			
			
			if (on){ 
				dtostrf(temp,3,0,c);
				printf("Temperature: %s\n", c);
				memcpy(msg.data, c, 3);
				_delay_ms(1000);
			}
			msg.length = strlen((char *)msg.data) + 1;
			nRF24L01_transmit(rf, to_address, &msg);
		}
	}
	return 0;
}

nRF24L01 *setup_rf(void) 
{
	nRF24L01 *rf = nRF24L01_init();
	rf->ss.port = &PORTB;
	rf->ss.pin = PB2;
	rf->ce.port = &PORTB;
	rf->ce.pin = PB1;
	rf->sck.port = &PORTB;
	rf->sck.pin = PB5;
	rf->mosi.port = &PORTB;
	rf->mosi.pin = PB3;
	rf->miso.port = &PORTB;
	rf->miso.pin = PB4;
	// interrupt on falling edge of INT0 (PD2)
	EICRA |= _BV(ISC01);
	EIMSK |= _BV(INT0);
	nRF24L01_begin(rf);
	return rf;
}

// setup timer to trigger interrupt every second when at 1MHz
void setup_timer(void) 
{
	TCCR1B |= _BV(WGM12);
	TIMSK1 |= _BV(OCIE1A);
	OCR1A = 15624;
	TCCR1B |= _BV(CS10) | _BV(CS11);
}

void USARTinit (void)
{
	/* set baud rate */
	UBRR0H = (MYUBRR>>8); //high value of baud rate
	UBRR0L = MYUBRR; //  low value of baud rate
	
	UCSR0B |= (1 << RXEN0) | (1<<TXEN0);  //enable receiver and transmitter
	UCSR0B |= (1 << RXCIE0);			 // enable receiver input
	UCSR0C = ((1<<UCSZ01)|(1<<UCSZ00)); //asynchronous
	stdout = &mystdout;
}

void USARTsend(unsigned char Data) // function for sending data to the stream
{
	while (!(UCSR0A & (1<<UDRE0))); 
	UDR0=Data;
}

void ADCinit(void)
{
	ADMUX |= (1 << REFS0);	//use AVcc as ref
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 128 Prescale for 16MHz
	ADCSRA |= (1 << ADATE); // Set ADC Auto Trigger Enable
	ADCSRB = 0; // Free running mode
	ADCSRA |= (1 << ADEN); // Enable the ADC
	ADCSRA |= (1 << ADIE); // Enable interrupts	
	ADCSRA |= (1 << ADSC); // start conversion
}

ISR (ADC_vect)
{
	
	
	/*
	convert the read ADCvalue to temperature
	500.0=>(Vref * 100)=>(5V * 100)
	divide by 1024, the max for the ADC values (0-1024)
	*/
	ADCvalue = ADC; 
	ADCvalue = (ADCvalue)*(500.0/1024.0);
	
	/* converts value into ascii */
	temp = ADCvalue;
}

// each one second interrupt
ISR(TIMER1_COMPA_vect)
{
	send_message = true;
}

// nRF24L01 interrupt
ISR(INT0_vect) {
	rf_interrupt = true;
}