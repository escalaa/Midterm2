/*
 * Reciever.c
 *
 * Created: 5/8/2018 9:02:20 PM
 * Author : Oji
 */ 

#define F_CPU 8000000UL
#define BAUD 9600

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <string.h>
#include <util/delay.h>
#include "nrf24l01.h"
#include "nrf24l01-mnemonics.h"
nRF24L01 *setup_rf(void);
void process_message(char *message);
inline void prepare_led_pin(void);
inline void  set_led_high(void);
inline void  set_led_low(void);

volatile bool rf_interrupt = false;

void init_USART();
static int put_char(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(put_char, NULL, _FDEV_SETUP_WRITE);

int main(void) {
	stdout = &mystdout;
	
	uint8_t address[5] = { 0x01, 0x01, 0x01, 0x01, 0x01 };
	prepare_led_pin();
	init_USART();
	
	sei();
	nRF24L01 *rf = setup_rf();
	nRF24L01_listen(rf, 0, address);
	uint8_t addr[5];
	nRF24L01_read_register(rf, CONFIG, addr, 1);

	while (true) {
		if (rf_interrupt) {
			rf_interrupt = false;
			while (nRF24L01_data_received(rf)) {
				nRF24L01Message msg;
				nRF24L01_read_received_data(rf, &msg);

				process_message((char *)msg.data);
				printf("Temperature: ");
				printf((char *)msg.data);
				printf("\r\n");
				_delay_ms(1000);
			}

			nRF24L01_listen(rf, 0, address);
		}
	}

	return 0;
}

nRF24L01 *setup_rf(void) {
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

void process_message(char *message) {
	if (strcmp(message, "ON") == 0)
	set_led_high();
	else if (strcmp(message, "OFF") == 0)
	set_led_low();
}

inline void prepare_led_pin(void) {
	DDRB |= _BV(PB0);
	PORTB &= ~_BV(PB0);
}

inline void set_led_high(void) {
	PORTB |= _BV(PB0);
}

inline void set_led_low(void) {
	PORTB &= ~_BV(PB0);
}

// nRF24L01 interrupt
ISR(INT0_vect) {
	rf_interrupt = true;
}

void init_USART(){
	unsigned int BAUD_rate;

	//set BAUD rate: UBRR = [F_CPU/(16*BAUD)]-1
	BAUD_rate = ((F_CPU/16)/BAUD) - 1;
	UBRR0H = (unsigned char) (BAUD_rate >> 8); //shift top 8 bits into UBRR0H
	UBRR0L = (unsigned char) BAUD_rate; //shift rest of 8 bits into UBRR0L
	UCSR0B |= (1 << RXEN0) | (1 << TXEN0); //enable receiver and trasmitter
	// UCSR0B |= (1 << RXCIE0); //enable receiver interrupt
	UCSR0C |= (1 << UCSZ01) | (1 << UCSZ00); //set data frame: 8 bit, 1 stop
}

static int put_char(char c, FILE *stream)
{
	while(!(UCSR0A &(1<<UDRE0))); // wait for UDR to be clear
	UDR0 = c;    //send the character
	return 0;
}
