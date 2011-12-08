#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "rfm12_config.h"

#include "rfm12.h"
#include "uart.h"

#define INIT_LED			DDRD |= ((1 << PD5) | (1 << PD6));			
#define LED1_ON				PORTD |= (1 << PD6);
#define LED1_OFF			PORTD &= ~(1 << PD6);
#define LED2_ON				PORTD |= (1 << PD5);
#define LED2_OFF			PORTD &= ~(1 << PD5);

#define INIT_TASTER			DDRB &= ~(1 << PB1);
#define TASTER1				(PINB & (1 << PINB1))				


int main ( void )
{
	uint8_t *bufcontents;
	uint8_t i;
	uint8_t tv[] = "HS Mhm";
	uint8_t tele1[11] = "HELLO";
	uint8_t tele2[11] = "OLLEH";
	uint8_t rfm_status;

	uint16_t ticker = 0;
	uint16_t led1_an = 0;
	uint16_t led2_an = 0;
	
	uint8_t iNodeId = 1;

	INIT_LED;
	INIT_TASTER;

	//PD7 <> FSK/DATA/NFFS auf 1 setzen
	DDR_SS |= (1 << PD7);	
	PORTD  |= (1 << PD7);
	
	uart_init();
	rfm12_init();
	sei();

	uart_putstr ("\r\nHS Mhm\r\n");
	
	if ( TASTER1 > 0 )
	{
		iNodeId = 2;
		uart_putstr ("\r\nNode 2\r\n");
	}
	else
	{
		uart_putstr ("\r\nNode 1\r\n");
	}	
	
	while (42)
	{
		// Zykluszaehler 
		ticker++;
		if ( ticker == 0) 
			uart_putstr (".");
			
		// ggf LED's nach kurzer Leuchtzeit wieder aus schalten
		if ( led1_an > 0 )
			led1_an--;
		if ( led1_an == 0 )
			LED1_OFF;
		if ( led2_an > 0 )
			led2_an--;
		if ( led2_an == 0 )
			LED2_OFF;

		// wenn etwas emfangen dieses auf RS232 ausgeben
		if ( rfm12_rx_status() == STATUS_COMPLETE )
		{
			LED1_ON;
			led1_an = 10000;
			uart_putstr ("rx packet: ");

			bufcontents = rfm12_rx_buffer();

			// dump buffer contents to uart			
			for (i=0; i < rfm12_rx_len(); i++ )
			{
				uart_putc ( bufcontents[i] );
			}
			uart_putstr ("\r\n");
			
			// tell the implementation that the buffer
			// can be reused for the next data.
			rfm12_rx_clear();
		}

		// zyklisch etwas senden
		if (ticker == 0)
		{
			LED2_ON;
			led2_an = 10000;
			//uart_putstr ("tx packet ..");
			
			if ( iNodeId == 1 )
				rfm12_tx (sizeof(tele1), 1, tele1);
			else
				rfm12_tx (sizeof(tele2), 1, tele2);
		}

		// Telegrammverarbeitung 
		rfm12_tick();
	}
}
