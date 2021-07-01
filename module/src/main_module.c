/***************************************************************************
 *   Copyright (C) 2020 by Christoph Thelen                                *
 *   doc_bacardi@users.sourceforge.net                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "main_module.h"

#include <string.h>

#include "pad_control.h"
#include "rdy_run.h"
#include "systime.h"
#include "uart.h"

/*-------------------------------------------------------------------------*/

typedef union PTR_UNION
{
	unsigned long ul;
	unsigned char *puc;
	unsigned short *pus;
	unsigned long *pul;
} PTR_T;


typedef struct IOLWM_EHCI_EVENT_HEADER_STRUCT
{
	unsigned char  ucPacketType;
	unsigned char  ucEventCode;
	unsigned char  ucTotalLength;
	unsigned char  ucEventOpcode;
	unsigned char  ucReserved;
} IOLWM_EHCI_EVENT_HEADER_T;


typedef struct IOLWM_PARAM_VS_STACKREADY_IND_STRUCT
{
    unsigned char  ucNumTracks;
    unsigned char  ucIsMcuPresent;
    unsigned short usStackRevisionBuild;
    unsigned char  ucStackRevisionMinor;
    unsigned char  ucStackRevisionMajor;
    unsigned short usRfRevisionBuild;
    unsigned char  ucRfRevisionMinor;
    unsigned char  ucRfRevisionMajor;
} IOLWM_PARAM_VS_STACKREADY_IND_T;


#define IOLWM_RADIO_TEST_CMD_CON_STOP       0
#define IOLWM_RADIO_TEST_CMD_CON_TX         1
#define IOLWM_RADIO_TEST_CMD_CON_RX         2
#define IOLWM_RADIO_TEST_CMD_CON_PREPARE    3

#define IOLW_EHCI_API_EVENT_CODE_VALUE      0xFF                                                                                                                                              
#define IOLW_EHCI_SMI_EVENT_CODE_VALUE      0xFE                                                                                                                                              

#define IOLWM_VS_STACK_READY_IND_OPCODE     0x80                                                                                                                                              
#define IOLWM_VS_STACK_REVISION_CNF_OPCODE  0x81                                                                                                                                              
#define IOLWM_VS_RF_REVISION_CNF_OPCODE     0x82                                                                                                                                              


/*-------------------------------------------------------------------------*/

static const unsigned char aucPadCtrlUartAppIndex[2] =
{
	PAD_AREG2OFFSET(mmio, 1),    /* MMIO1 */
	PAD_AREG2OFFSET(mmio, 2)     /* MMIO2 */
};



static const unsigned char aucPadCtrlUartAppConfig[2] =
{
	PAD_CONFIGURATION(PAD_DRIVING_STRENGTH_Low, PAD_PULL_Enable, PAD_INPUT_Enable),
	PAD_CONFIGURATION(PAD_DRIVING_STRENGTH_Low, PAD_PULL_Enable, PAD_INPUT_Enable)
};



static void setup_padctrl(void)
{
	pad_control_apply(aucPadCtrlUartAppIndex, aucPadCtrlUartAppConfig, sizeof(aucPadCtrlUartAppIndex));
}


/*-------------------------------------------------------------------------*/

#define UART_BAUDRATE_921600 9216

static void uart_initialize(void)
{
	unsigned long ulValue;
	HOSTDEF(ptUartAppArea);
	HOSTDEF(ptAsicCtrlArea);
	HOSTDEF(ptMmioCtrlArea);


	/* Disable the UART. */
	ptUartAppArea->ulUartcr = 0;

	/* Use baud rate mode 2. */
	ptUartAppArea->ulUartcr_2 = HOSTMSK(uartcr_2_Baud_Rate_Mode);

	/* Set the baud rate. */
//	ulValue = UART_BAUDRATE_DIV(UART_BAUDRATE_921600);
	ptUartAppArea->ulUartlcr_l = 0xc0;
	ptUartAppArea->ulUartlcr_m = 0x25;

	/* Set the UART to 8N1, FIFO enabled. */
	ulValue  = HOSTMSK(uartlcr_h_WLEN);
	ulValue |= HOSTMSK(uartlcr_h_FEN);
	ptUartAppArea->ulUartlcr_h = ulValue;

	/* Disable all drivers. */
	ptUartAppArea->ulUartdrvout = 0;

	/* Disable RTS/CTS mode. */
	ptUartAppArea->ulUartrts = 0;

	/* Enable the UART. */
	ptUartAppArea->ulUartcr = HOSTMSK(uartcr_uartEN);

	/* Setup MMIO2 as UART_APP_RXD. */
	ptAsicCtrlArea->ulAsic_ctrl_access_key = ptAsicCtrlArea->ulAsic_ctrl_access_key;
	ptMmioCtrlArea->aulMmio_cfg[2] = NX90_MMIO_CFG_UART_APP_RXD;

	/* Enable the drivers. */
	ulValue = HOSTMSK(uartdrvout_DRVTX);
	ptUartAppArea->ulUartdrvout = ulValue;

	/* Setup MMIO1 as UART_APP_TXD. */
	ptAsicCtrlArea->ulAsic_ctrl_access_key = ptAsicCtrlArea->ulAsic_ctrl_access_key;
	ptMmioCtrlArea->aulMmio_cfg[1] = NX90_MMIO_CFG_UART_APP_TXD;
}



/* Remove all elements from the receive fifo. */
static void uart_clean_receive_fifo(void)
{
	HOSTDEF(ptUartAppArea);
	unsigned long ulValue;


	do
	{
		/* Check for data in the FIFO. */
		ulValue  = ptUartAppArea->ulUartfr;
		ulValue &= HOSTMSK(uartfr_RXFE);
		if( ulValue==0 )
		{
			/* The FIFO is not empty, get the received byte. */
			ptUartAppArea->ulUartdr;
		}
	} while( ulValue==0 );
}



static void uart_send(unsigned char ucData)
{
	HOSTDEF(ptUartAppArea);
	unsigned long ulValue;


	/* Wait until there is space in the FIFO. */
	do
	{
		ulValue  = ptUartAppArea->ulUartfr;
		ulValue &= HOSTMSK(uartfr_TXFF);
	} while( ulValue!=0 );

	ptUartAppArea->ulUartdr = ucData;
}



static unsigned long uart_receive(unsigned char *pucData, unsigned int sizData, unsigned long ulCharTimeoutInMs, unsigned long ulTotalTimeoutInMs)
{
	HOSTDEF(ptUartAppArea);
	unsigned long ulResult;
	unsigned long ulTimerChar;
	unsigned long ulTimerTotal;
	unsigned long ulValue;
	int iCharTimerElapsed;
	int iTotalTimerElapsed;
	unsigned int uiRecCnt;


	ulResult = IOLWM_RESULT_Ok;

	iCharTimerElapsed = 0;
	iTotalTimerElapsed = 0;

	/* Get the current system tick. */
	ulTimerTotal = systime_get_ms();

	uiRecCnt = 0U;
	while( uiRecCnt<sizData )
	{
		ulTimerChar = systime_get_ms();
		do
		{
			/* Is data in the receive FIFO? */
			ulValue  = ptUartAppArea->ulUartfr;
			ulValue &= HOSTMSK(uartfr_RXFE);
			/* Char timer elapsed? */
			if( ulCharTimeoutInMs!=0 )
			{
				iCharTimerElapsed = systime_elapsed(ulTimerChar, ulCharTimeoutInMs);
			}
			/* Total timer elapsed? */
			if( ulTotalTimeoutInMs!=0 )
			{
				iTotalTimerElapsed = systime_elapsed(ulTimerTotal, ulTotalTimeoutInMs);
			}
		} while( ulValue!=0 && iCharTimerElapsed==0 && iTotalTimerElapsed==0 );

		/* Is data in the FIFO? */
		if( ulValue==0 )
		{
			ulValue  = ptUartAppArea->ulUartdr;
			ulValue &= 0xffU;
			if( pucData!=NULL )
			{
				pucData[uiRecCnt] = (unsigned char)ulValue;
			}
			++uiRecCnt;
		}
		else
		{
			ulResult = IOLWM_RESULT_Timout;
			break;
		}
	}

	return ulResult;
}



static unsigned long event_wait(unsigned char *pucBuffer, unsigned long *pulSize, unsigned char ucOpcode, unsigned char ucSubcode, unsigned long ulRetries)
{
	unsigned long ulResult;
	int iReceivedPacket;
	unsigned long ulSize;
	union {
		unsigned char auc[sizeof(IOLWM_EHCI_EVENT_HEADER_T)];
		IOLWM_EHCI_EVENT_HEADER_T t;
	} uHeader;


	ulResult = IOLWM_RESULT_Ok;
	iReceivedPacket = 0;

	do
	{
		/* Receive the header. */
		ulResult = uart_receive(uHeader.auc, sizeof(IOLWM_EHCI_EVENT_HEADER_T), 0, 1000);
		if( ulResult==IOLWM_RESULT_Ok )
		{
			/* Get the size of the data part. */
			ulSize = uHeader.t.ucTotalLength-2U;

			/* Now read the data part. */
			ulResult = uart_receive(pucBuffer, ulSize, 0, 1000);
			if( ulResult==IOLWM_RESULT_Ok )
			{
				/* Is this the requested packet? */
				if( ucOpcode==uHeader.t.ucEventCode && ucSubcode==uHeader.t.ucEventOpcode )
				{
					/* Yes, this is the correct package. */
					if( pulSize!=NULL )
					{
						*pulSize = ulSize;
					}
					iReceivedPacket = 1;
				}
			}
		}
	} while( iReceivedPacket==0 && (ulRetries--)>0);

	if( iReceivedPacket==0 )
	{
		ulResult = IOLWM_RESULT_Timout;
	}

	return ulResult;
}


static unsigned long module_waitforpowerup(unsigned char *pucBuffer)
{
	unsigned long ulResult;
	unsigned long ulSize;


	ulResult = event_wait(pucBuffer, &ulSize, IOLW_EHCI_API_EVENT_CODE_VALUE, IOLWM_VS_STACK_READY_IND_OPCODE, 10);
	if( ulResult==IOLWM_RESULT_Ok )
	{
		if( ulSize>=sizeof(IOLWM_PARAM_VS_STACKREADY_IND_T) )
		{
			ulResult = IOLWM_RESULT_Ok;
		}
		else
		{
			ulResult = IOLWM_RESULT_InvalidPacketSize;
		}
	}

	return ulResult;
}



unsigned long module(unsigned long ulParameter0, unsigned long ulParameter1, unsigned long ulParameter2, unsigned long ulParameter3)
{
	unsigned long ulResult;
	IOLWM_COMMAND_T tCmd;
	PTR_T uAdr;

	ulResult = IOLWM_RESULT_UnknownCommand;

	tCmd = (IOLWM_COMMAND_T)ulParameter0;
	switch(tCmd)
	{
	case IOLWM_COMMAND_WaitForPowerup:
		/* Initialize has one parameter:
		 *   parameter 1: address of the buffer to copy the indication data to.
		 */
		setup_padctrl();
		uart_initialize();
		uart_clean_receive_fifo();

		uAdr.ul = ulParameter1;
		ulResult = module_waitforpowerup(uAdr.puc);
		break;
	}

	return ulResult;
}

/*-----------------------------------*/
