#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <avr/eeprom.h>

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

#define EEPROM_START_ADDR    0
#define EEPROM_ADDR_NODEID   (0 + EEPROM_START_ADDR)

#define TTL_MAX				5	

#define AF_GATEWAY			0x1000

typedef enum dataType_etag
{
	DT_NONE,
	DT_INT_COUNTER,
	DT_FLOAT_TEMP,
	DT_MAX
} dataType_e;

typedef union data_utag
{
	float fValue;
	uint16_t iValue;		
} data_u;			

typedef struct tele_stag
{
	uint8_t adr_flags_src;			// Flags fuer Broadcast, Gateway-Verbindung (Sender)
	uint8_t adr_src;				// Knotenadresse 1 - 255 (Sender)

	uint8_t adr_flags_dest;			// Flags fuer Broadcast, Gateway-Verbindung (Empfaenger)
	uint8_t adr_dest;				// Knotenadresse 1 - 255 (Empfaenger)
	
	uint8_t ttl;					// Time To Life (Countdounzaehler fuer Weiterleitung)
	
	dataType_e dataType;			// Typ der Nutzdaten 'data'
	data_u data;
} tele_s;

uint8_t generate_data( data_u* pData, dataType_e* pDataType )
{
	static uint8_t data_counter = 0;
	
	data_counter++;

	*pDataType = DT_INT_COUNTER;
	pData->iValue = data_counter;
	
	return 0;
};

uint8_t rx_telegram( tele_s* pTele )
{
	uint8_t bOk = 0;
	char str[100];
	
	switch ( pTele->dataType )
	{
		case DT_INT_COUNTER:
			sprintf( str, "rx: %d (type=iC)\r\n", pTele->data.iValue );
			uart_putstr( str );
			bOk = 1;
			break;
			
		case DT_FLOAT_TEMP:
			sprintf( str, "rx: %.2f (type=fT)\r\n", pTele->data.fValue );
			uart_putstr( str );
			bOk = 1;
			break;
	}		
	
	return bOk;
};

int main ( void )
{
	uint8_t sInfo[255] = "";

	uint16_t ticker = 0;
	uint16_t led1_an = 0;
	uint16_t led2_an = 0;
	uint8_t errcode = 0;
		
	uint8_t iNodeId = 1;
	tele_s tx_tele;
	tele_s* pRx_tele = NULL;
	uint8_t iLen = 0;

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
		iNodeId = 3;
		
		//EEPWriteByte( EEPROM_ADDR_NODEID, iNodeId );		// NodeID persistent in EEPROM speichern (nur einmalig)
		//iNodeId = EEPReadByte( EEPROM_ADDR_NODEID );		// gespeicherte NodeID aus EEPROM lesen
	}
	sprintf( sInfo, "nodeID=%d\r\n", iNodeId );
	uart_putstr( sInfo );
	
	while (42)
	{
		// Zykluszaehler 
		ticker++;
	//	if ( ticker == 0) 
	//		uart_putstr (".");
			
		// ggf LED's nach kurzer Leuchtzeit wieder ausschalten
		if ( led1_an > 0 )
			led1_an--;
		if ( led1_an == 0 )
			LED1_OFF;
		if ( led2_an > 0 )
			led2_an--;
		if ( led2_an == 0 )
			LED2_OFF;

		// wenn etwas empfangen dieses verarbeiten und ggf forwarden
		if ( rfm12_rx_status() == STATUS_COMPLETE )
		{
			LED1_ON;
			led1_an = 10000;
			//uart_putstr ("rx packet: ");

			iLen = rfm12_rx_len();
			pRx_tele = (tele_s*)rfm12_rx_buffer();
			while ( iLen >= sizeof(tele_s) )
			{
				rx_telegram( pRx_tele );
				
				pRx_tele->ttl--;
				if ( pRx_tele->ttl > 0 )
				{
					sprintf( sInfo, "fwd (adrSrc=%d, adrDest=%d,ttl=%d)\r\n", pRx_tele->adr_src, pRx_tele->adr_dest, pRx_tele->ttl );
					uart_putstr( sInfo );
							
					rfm12_tx( sizeof(tele_s), 0, (uint8_t*)pRx_tele );
				}
				
				iLen -= sizeof(tele_s);
			}
			
			// Buffer und ggf unvollstaendige Telegramme freigeben
			rfm12_rx_clear();
		}

		// zyklisch etwas senden
		if (ticker == 0)
		{
			errcode = generate_data( &tx_tele.data, &tx_tele.dataType );
			if (!errcode)
			{
				tx_tele.adr_flags_src = 0;
				tx_tele.adr_src = iNodeId;
				
				tx_tele.adr_flags_dest = AF_GATEWAY;
				tx_tele.adr_dest = 0;
				
				tx_tele.ttl = TTL_MAX;
				
				sprintf( sInfo, "tx (adrSrc=%d, adrDest=%d,ttl=%d)\r\n",tx_tele.adr_src, tx_tele.adr_dest, tx_tele.ttl );
				uart_putstr( sInfo );
				
				rfm12_tx( sizeof(tx_tele), 0, (uint8_t*)&tx_tele );

				LED2_ON;
				led2_an = 10000;
			}
		}

		// Telegrammverarbeitung 
		rfm12_tick();
		
		// KnotenID setzen
	/*	if ( uart_collectline( buf, &iPos ) )
		{
			if ( strstr( buf, "n=" ) != NULL )
			{
				i = atoi( buf + sizeof("node") );
				if ( i > 0 )
				{	
					iNodeId = i;
					sprintf( sInfo, "nID=%d done\r\n", iNodeId );
				}				
			}
			buf[0] = 0;
			iPos = 0;
		}		*/
	}
}

	
