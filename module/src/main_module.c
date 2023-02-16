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


/*-------------------------------------------------------------------------*/

#define IOLWM_MAX_PACKET_SIZE 255


/* NOTE: Use "pragma pack" instead of "attribute packed" as the latter does not work on MinGW.
 *       See here for details: https://sourceforge.net/p/mingw-w64/bugs/588/
 */
#pragma pack(push, 1)

typedef struct IOLWM_EHCI_EVENT_HEADER_STRUCT
{
	unsigned char  ucPacketType;
	unsigned char  ucEventCode;
	unsigned char  ucTotalLength;
	unsigned char  ucEventOpcode;
	unsigned char  ucMsgCounter;
} IOLWM_EHCI_EVENT_HEADER_T;


typedef struct IOLWM_EHCI_COMMAND_HEADER_STRUCT
{
	unsigned char  ucPacketType;
	unsigned short usHandle;
	unsigned short usTotalLength;
	unsigned short usOpcode;
	unsigned char  ucMsgCounter;
	unsigned char  ucReserved;
} IOLWM_EHCI_COMMAND_HEADER_T;


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

typedef struct IOLWM_PARAM_SMI_CM_CONFIG_REQ_STRUCT
{
	unsigned char  ucClientID;
	unsigned char  ucSmiMode;
} IOLWM_PARAM_SMI_CM_CONFIG_REQ_T;

typedef struct IOLWM_PARAM_SMI_VS_RADIO_TEST_DATA_STRUCT
{
	unsigned short usArgBlockID;
	unsigned char  ucCommand;
	unsigned char  ucTrack;
	unsigned char  ucModulation;
	unsigned char  ucTestPattern;
	unsigned char  ucFrequencyIndex;
	unsigned long  ulGeneratorInitValue;
	unsigned char  ucTxPowerValue;
} IOLWM_PARAM_SMI_VS_RADIO_TEST_DATA_T;

typedef struct IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_STRUCT
{
	unsigned char ucClientID;
	unsigned char ucPortNumber;
	unsigned short usArgBlockLength;
	IOLWM_PARAM_SMI_VS_RADIO_TEST_DATA_T tArgBlockData;
} IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_T;

typedef struct IOLWM_PARAM_SMI_VS_WRITE_CNF_STRUCT
{
	unsigned char  ucClientID;
	unsigned char  ucPortNumber;
	unsigned short usResult;
} IOLWM_PARAM_SMI_VS_WRITE_CNF_T;


typedef struct IOLWM_EHCI_COMMAND_PACKET_STRUCT
{
	union
	{
		unsigned char auc[sizeof(IOLWM_EHCI_COMMAND_HEADER_T)];
		IOLWM_EHCI_COMMAND_HEADER_T t;
	} uHeader;
	union
	{
		unsigned char auc[IOLWM_MAX_PACKET_SIZE-sizeof(IOLWM_EHCI_COMMAND_HEADER_T)];
		IOLWM_PARAM_SMI_CM_CONFIG_REQ_T tSmiCmConfigReq;
		IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_T tSmiVsRadioTestReq;
	} uPayload;
} IOLWM_EHCI_COMMAND_PACKET_T;


typedef struct IOLWM_EHCI_EVENT_PACKET_STRUCT
{
	union
	{
		unsigned char auc[sizeof(IOLWM_EHCI_EVENT_HEADER_T)];
		IOLWM_EHCI_EVENT_HEADER_T t;
	} uHeader;
	union
	{
		unsigned char auc[IOLWM_MAX_PACKET_SIZE-sizeof(IOLWM_EHCI_EVENT_HEADER_T)];
		IOLWM_PARAM_SMI_VS_WRITE_CNF_T tSmiVsWriteCnf;
	} uPayload;
} IOLWM_EHCI_EVENT_PACKET_T;

#pragma pack(pop)

/*-------------------------------------------------------------------------*/


#define IOLWM_RADIO_TEST_CMD_CON_STOP       0
#define IOLWM_RADIO_TEST_CMD_CON_TX         1
#define IOLWM_RADIO_TEST_CMD_CON_RX         2
#define IOLWM_RADIO_TEST_CMD_CON_PREPARE    3

#define IOLWM_RADIO_TEST_CMD_MODULATION_CW    0
#define IOLWM_RADIO_TEST_CMD_MODULATION_GFSK  1

#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_PRBS9       0
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_PRBS15      1
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_ZOZO        2
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_All1        3
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_All0        4
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_F0F0        5
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_FF00        6
#define IOLWM_RADIO_TEST_CMD_TESTPATTERN_UserDefine  7

#define IOLWM_EHCI_PACKET_TYPE_COMMAND      2
#define IOLWM_EHCI_PACKET_TYPE_EVENT        4

#define IOLW_EHCI_API_EVENT_CODE_VALUE      0xFF
#define IOLW_EHCI_SMI_EVENT_CODE_VALUE      0xFE

#define IOLWM_VS_STACK_READY_IND_OPCODE     0x80
#define IOLWM_VS_STACK_REVISION_CNF_OPCODE  0x81
#define IOLWM_VS_RF_REVISION_CNF_OPCODE     0x82

#define IOLWM_SMI_CM_CONFIG_REQ_OPCODE                      0xFE10
#define IOLWM_SMI_CM_MASTER_IDENT_REQ_OPCODE                0xFE01
#define IOLWM_SMI_CM_MASTER_CONFIG_REQ_OPCODE               0xFE02
#define IOLWM_SMI_CM_RDB_MASTER_CONFIG_REQ_OPCODE           0xFE03
#define IOLWM_SMI_CM_TRACK_CONFIG_REQ_OPCODE                0xFE04
#define IOLWM_SMI_CM_RDB_TRACK_CONFIG_REQ_OPCODE            0xFE05
#define IOLWM_SMI_CM_TRACK_STATUS_REQ_OPCODE                0xFE06
#define IOLWM_SMI_CM_PORT_CONFIG_REQ_OPCODE                 0xFE07
#define IOLWM_SMI_CM_RDB_PORT_CONFIG_REQ_OPCODE             0xFE08
#define IOLWM_SMI_CM_PORT_STATUS_REQ_OPCODE                 0xFE09
#define IOLWM_SMI_CM_SCAN_REQ_OPCODE                        0xFE0A
#define IOLWM_SMI_CM_SCAN_STATUS_REQ_OPCODE                 0xFE0B
#define IOLWM_SMI_DS_READ_REQ_OPCODE                        0xFE20
#define IOLWM_SMI_DS_WRITE_REQ_OPCODE                       0xFE21
#define IOLWM_SMI_ODE_DEVICE_WRITE_REQ_OPCODE               0xFE30
#define IOLWM_SMI_ODE_DEVICE_READ_REQ_OPCODE                0xFE31
#define IOLWM_SMI_PDE_PDIN_REQ_OPCODE                       0xFE40
#define IOLWM_SMI_PDE_PDOUT_REQ_OPCODE                      0xFE41
#define IOLWM_SMI_PDE_CONFIG_PD_REQ_OPCODE                  0xFE42
#define IOLWM_SMI_PDE_RDB_CONFIG_PD_REQ_OPCODE              0xFE43
#define IOLWM_SMI_PORT_CMD_REQ_OPCODE                       0xFE60
#define IOLWM_SMI_VS_WRITE_REQ_OPCODE                       0xFE81
#define IOLWM_SMI_VS_READ_REQ_OPCODE                        0xFE82

#define IOLWM_SMI_CM_MASTER_IDENT_CNF_OPCODE                0x01
#define IOLWM_SMI_CM_MASTER_CONFIG_CNF_OPCODE               0x02
#define IOLWM_SMI_CM_RDB_MASTER_CONFIG_CNF_OPCODE           0x03
#define IOLWM_SMI_CM_TRACK_CONFIG_CNF_OPCODE                0x04
#define IOLWM_SMI_CM_RDB_TRACK_CONFIG_CNF_OPCODE            0x05
#define IOLWM_SMI_CM_TRACK_STATUS_CNF_OPCODE                0x06
#define IOLWM_SMI_CM_PORT_CONFIG_CNF_OPCODE                 0x07
#define IOLWM_SMI_CM_RDB_PORT_CONFIG_CNF_OPCODE             0x08
#define IOLWM_SMI_CM_PORT_STATUS_CNF_OPCODE                 0x09
#define IOLWM_SMI_CM_SCAN_CNF_OPCODE                        0x0A
#define IOLWM_SMI_CM_SCAN_STATUS_CNF_OPCODE                 0x0B
#define IOLWM_SMI_CM_PORT_EVENT_IND_OPCODE                  0x0C
#define IOLWM_SMI_CM_CONFIG_CNF_OPCODE                      0x10
#define IOLWM_SMI_DS_READ_CNF_OPCODE                        0x20
#define IOLWM_SMI_DS_WRITE_CNF_OPCODE                       0x21
#define IOLWM_SMI_ODE_DEVICE_WRITE_CNF_OPCODE               0x30
#define IOLWM_SMI_ODE_DEVICE_READ_CNF_OPCODE                0x31
#define IOLWM_SMI_PDE_PDIN_CNF_OPCODE                       0x40
#define IOLWM_SMI_PDE_PDOUT_CNF_OPCODE                      0x41
#define IOLWM_SMI_PDE_CONFIG_PD_CNF_OPCODE                  0x42
#define IOLWM_SMI_PDE_RDB_CONFIG_PD_CNF_OPCODE              0x43
#define IOLWM_SMI_DU_DEVICE_EVENT_IND_OPCODE                0x50
#define IOLWM_SMI_PORT_CMD_CNF_OPCODE                       0x60
#define IOLWM_SMI_VS_WRITE_CNF_OPCODE                       0x81
#define IOLWM_SMI_VS_READ_CNF_OPCODE                        0x82
#define IOLWM_SMI_VS_IND_OPCODE                             0x83
#define IOLWM_SMI_ACK_OPCODE                                0x86

#define ARG_BLK_ID_MASTER_IDENT                             0x0000
#define ARG_BLK_ID_MASTER_CONFIG                            0x0002
#define ARG_BLK_ID_PORT_CONFIG                              0x8002
#define ARG_BLK_ID_TRACK_CONFIG                             0x8003
#define ARG_BLK_ID_SCAN_STATUS                              0x8004
#define ARG_BLK_ID_PORT_STATUS                              0x9002
#define ARG_BLK_ID_TRACK_STATUS                             0x9003
#define ARG_BLK_ID_SCAN                                     0x9004
#define ARG_BLK_ID_PORT_EVENT                               0xA001
#define ARG_BLK_ID_PDIN                                     0x1001
#define ARG_BLK_ID_PDOUT                                    0x1002

#define ARG_BLK_ID_VS_RADIO_CON_TEST                        0xB051

/*-------------------------------------------------------------------------*/

static union {
	unsigned char auc[IOLWM_MAX_PACKET_SIZE];
	IOLWM_EHCI_COMMAND_PACKET_T tCommand;
	IOLWM_EHCI_EVENT_PACKET_T tEvent;
} m_uBuffer;

static unsigned int m_uiMsgCounter;

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



static void uart_send(const void *pvData, unsigned int sizData)
{
	HOSTDEF(ptUartAppArea);
	unsigned long ulValue;
	const unsigned char *pucCnt;
	const unsigned char *pucEnd;


	pucCnt = (const unsigned char*)pvData;
	pucEnd = pucCnt + sizData;
	while( pucCnt<pucEnd )
	{
		/* Wait until there is space in the FIFO. */
		do
		{
			ulValue  = ptUartAppArea->ulUartfr;
			ulValue &= HOSTMSK(uartfr_TXFF);
		} while( ulValue!=0 );

		ptUartAppArea->ulUartdr = *(pucCnt++);
	}
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



static unsigned int fletcher(const unsigned char *pucData, unsigned int sizData)
{
	unsigned int uiSum1;
	unsigned int uiSum2;


	uiSum1 = 0;
	uiSum2 = 0;

	while( sizData!=0 )
	{
		uiSum1 = uiSum1 + (*(pucData++));
		if( uiSum1>=255 )
		{
			uiSum1 -= 255;
		}
		uiSum2 = uiSum2 + uiSum1;
		if( uiSum2>=255 )
		{
			uiSum2 -= 255;
		}
		--sizData;
	}

	return (uiSum2<<8U) | uiSum1;
}



static unsigned long event_wait(unsigned long *pulSize, unsigned char ucOpcode, unsigned char ucSubcode, unsigned long ulRetries)
{
	unsigned long ulResult;
	int iReceivedPacket;
	unsigned long ulSize;
	unsigned int uiChecksumMy;
	unsigned int uiChecksumRec;


	ulResult = IOLWM_RESULT_Ok;
	iReceivedPacket = 0;

	do
	{
		/* Receive the header. */
		ulResult = uart_receive(m_uBuffer.auc, sizeof(IOLWM_EHCI_EVENT_HEADER_T), 0, 1000);
		if( ulResult==IOLWM_RESULT_Ok )
		{
			/* Get the size of the payload part.
			 * 2 bytes of the total length were already received in the header data.
			 */
			ulSize = m_uBuffer.tEvent.uHeader.t.ucTotalLength - 2U;

			/* Now read the payload part. */
			ulResult = uart_receive(m_uBuffer.tEvent.uPayload.auc, ulSize, 0, 1000);
			if( ulResult==IOLWM_RESULT_Ok )
			{
				/* The last 2 bytes are the checksum of the packet. */
				uiChecksumRec  = (unsigned int)(m_uBuffer.tEvent.uPayload.auc[ulSize-2U]);
				uiChecksumRec |= (unsigned int)(m_uBuffer.tEvent.uPayload.auc[ulSize-1U] << 8U);

				/* Generate the checksum for the data part. */
				uiChecksumMy = fletcher(m_uBuffer.auc, sizeof(IOLWM_EHCI_EVENT_HEADER_T)+ulSize-2U);
				if( uiChecksumRec==uiChecksumMy )
				{
					/* Is this the requested packet? */
					if(
						ucOpcode==m_uBuffer.tEvent.uHeader.t.ucEventCode &&
						ucSubcode==m_uBuffer.tEvent.uHeader.t.ucEventOpcode
					)
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
		}
	} while( iReceivedPacket==0 && (ulRetries--)>0);

	if( iReceivedPacket==0 )
	{
		ulResult = IOLWM_RESULT_Timout;
	}

	return ulResult;
}


static void send_command_ack(unsigned int uiMsgCounter)
{
	unsigned int uiChecksum;


	/* Create an event ACK packet. */
	m_uBuffer.tCommand.uHeader.t.ucPacketType = IOLWM_EHCI_PACKET_TYPE_COMMAND;
	m_uBuffer.tCommand.uHeader.t.usHandle = 0x0001U;
	m_uBuffer.tCommand.uHeader.t.usTotalLength = 6U;
	m_uBuffer.tCommand.uHeader.t.usOpcode = 0xfe86U;
	m_uBuffer.tCommand.uHeader.t.ucMsgCounter = (unsigned char)(uiMsgCounter & 0xffU);
	m_uBuffer.tCommand.uHeader.t.ucReserved = 0x00U;

	/* Add the checksum. */
	uiChecksum = fletcher(m_uBuffer.tCommand.uHeader.auc, sizeof(IOLWM_EHCI_COMMAND_HEADER_T));
	m_uBuffer.tCommand.uPayload.auc[0] = (unsigned char)( uiChecksum        & 0xffU);
	m_uBuffer.tCommand.uPayload.auc[1] = (unsigned char)((uiChecksum >> 8U) & 0xffU);

	/* Send the packet. */
	uart_send(m_uBuffer.auc, sizeof(IOLWM_EHCI_COMMAND_HEADER_T)+2U);
}

#if 0
static void send_event_ack(unsigned char ucMsgCounter)
{
	unsigned int uiChecksum;


	/* Create an event ACK packet. */
	m_uBuffer.tEvent.uHeader.t.ucPacketType = 0x04;
	m_uBuffer.tEvent.uHeader.t.ucEventCode = 0xFE;
	m_uBuffer.tEvent.uHeader.t.ucTotalLength = 4;
	m_uBuffer.tEvent.uHeader.t.ucEventOpcode = 0x86;
	m_uBuffer.tEvent.uHeader.t.ucMsgCounter = ucMsgCounter;

	/* Add the checksum. */
	uiChecksum = fletcher(m_uBuffer.tEvent.uHeader.auc, sizeof(IOLWM_EHCI_EVENT_HEADER_T));
	m_uBuffer.tEvent.uPayload.auc[0] = (unsigned char)( uiChecksum        & 0xffU);
	m_uBuffer.tEvent.uPayload.auc[1] = (unsigned char)((uiChecksum >> 8U) & 0xffU);

	/* Send the packet. */
	uart_send(m_uBuffer.auc, sizeof(IOLWM_EHCI_EVENT_HEADER_T)+2U);
}
#endif

static unsigned long module_waitforpowerup(unsigned char *pucBuffer)
{
	unsigned long ulResult;
	unsigned long ulSize;
	unsigned int uiMsgCounter;


	ulSize = 0;
	ulResult = event_wait(&ulSize, IOLW_EHCI_API_EVENT_CODE_VALUE, IOLWM_VS_STACK_READY_IND_OPCODE, 10);
	if( ulResult==IOLWM_RESULT_Ok )
	{
		if( ulSize>=sizeof(IOLWM_PARAM_VS_STACKREADY_IND_T) )
		{
			/* Copy the data to the user buffer. */
			memcpy(pucBuffer, m_uBuffer.tEvent.uPayload.auc, sizeof(IOLWM_PARAM_VS_STACKREADY_IND_T));

			/* Acknowledge the event. */
			uiMsgCounter = m_uBuffer.tEvent.uHeader.t.ucMsgCounter;
			send_command_ack(uiMsgCounter);

			/* Synchronize the local message counter. */
			m_uiMsgCounter = uiMsgCounter;

			ulResult = IOLWM_RESULT_Ok;
		}
		else
		{
			ulResult = IOLWM_RESULT_InvalidPacketSize;
		}
	}

	return ulResult;
}



static void command_create(IOLWM_EHCI_COMMAND_PACKET_T *ptPacket, unsigned short usOpcode, unsigned short sizPayload)
{
	ptPacket->uHeader.t.ucPacketType = 0x02;
	ptPacket->uHeader.t.usHandle = 0x0001;
	/* Set the total length of the packet.
	 * This includes all header fields after the "usTotalLength", the payload data and the 2 byte checksum.
	 */
	ptPacket->uHeader.t.usTotalLength = (unsigned short)(4U + sizPayload + 2U);
	ptPacket->uHeader.t.usOpcode = usOpcode;
	ptPacket->uHeader.t.ucMsgCounter = (unsigned char)(m_uiMsgCounter & 0xffU);
	ptPacket->uHeader.t.ucReserved = 0x00;
}



static void generate_command_checksum(unsigned int sizData)
{
	unsigned int uiChecksum;
	unsigned char *pucChecksum;


	/* Build the checksum over header and data. */
	uiChecksum = fletcher(m_uBuffer.auc, sizeof(IOLWM_EHCI_COMMAND_HEADER_T) + sizData);
	/* Append the checksum to the data. */
	pucChecksum = m_uBuffer.tCommand.uPayload.auc + sizData;
	*(pucChecksum++) = (unsigned char)( uiChecksum & 0x00ffU);
	*pucChecksum     = (unsigned char)((uiChecksum & 0xff00U) >> 8U);
}



static unsigned long module_activateSmiMode(void)
{
	unsigned long ulResult;
	unsigned long ulSize;
	unsigned int uiStatus;


	++m_uiMsgCounter;

	command_create(&(m_uBuffer.tCommand), IOLWM_SMI_CM_CONFIG_REQ_OPCODE, sizeof(IOLWM_PARAM_SMI_CM_CONFIG_REQ_T));
	m_uBuffer.tCommand.uPayload.tSmiCmConfigReq.ucClientID = 0x01;  // ClientID 1
	m_uBuffer.tCommand.uPayload.tSmiCmConfigReq.ucSmiMode = 0x01;   // SMI Mode = ON
	generate_command_checksum(sizeof(IOLWM_PARAM_SMI_CM_CONFIG_REQ_T));

	uart_send(m_uBuffer.auc, sizeof(IOLWM_EHCI_COMMAND_HEADER_T)+sizeof(IOLWM_PARAM_SMI_CM_CONFIG_REQ_T)+2U);

	/* Wait for an ACK packet. */
	ulResult = event_wait(&ulSize, IOLW_EHCI_SMI_EVENT_CODE_VALUE, IOLWM_SMI_ACK_OPCODE, 10);
	if( ulResult==IOLWM_RESULT_Ok )
	{
		/* Wait for the response. */
		ulResult = event_wait(&ulSize, IOLW_EHCI_SMI_EVENT_CODE_VALUE, IOLWM_SMI_CM_CONFIG_CNF_OPCODE, 10);
		if( ulResult==IOLWM_RESULT_Ok )
		{
			/* The response must have at least 3 bytes. */
			if( ulSize<3U )
			{
				ulResult = IOLWM_RESULT_InvalidPacketSize;
			}
			else
			{
				/* Get the status from the event. It will be overwritten by the following ACK. */
				uiStatus  = (unsigned int)(m_uBuffer.tEvent.uPayload.auc[1]);
				uiStatus |= (unsigned int)(m_uBuffer.tEvent.uPayload.auc[2]<<8U);

				/* Send an ACK. */
				send_command_ack(m_uBuffer.tEvent.uHeader.t.ucMsgCounter);

				/* Check the result. */
				if( uiStatus!=0 )
				{
					ulResult = IOLWM_RESULT_EhciError;
				}
			}
		}
	}

	return ulResult;
}



static unsigned long smi_vs_radio_test(unsigned char ucTrack, unsigned char ucCommand, unsigned char ucModulation, unsigned char ucTestPattern, unsigned char ucFrequencyIndex, unsigned long ulDataWhitenerIv, unsigned char ucTxPowerValue)
{
	unsigned long ulResult;
	unsigned long ulSize;
	unsigned short usResult;


	++m_uiMsgCounter;

	command_create(&(m_uBuffer.tCommand), IOLWM_SMI_VS_WRITE_REQ_OPCODE, sizeof(IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_T));
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.ucClientID = 0x01;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.ucPortNumber = 0x00;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.usArgBlockLength = sizeof(IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_T);
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.usArgBlockID = ARG_BLK_ID_VS_RADIO_CON_TEST;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ucCommand = ucCommand;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ucTrack = ucTrack;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ucModulation = ucModulation;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ucTestPattern = ucTestPattern;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ucFrequencyIndex = ucFrequencyIndex;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ulGeneratorInitValue = ulDataWhitenerIv;
	m_uBuffer.tCommand.uPayload.tSmiVsRadioTestReq.tArgBlockData.ucTxPowerValue = ucTxPowerValue;
	generate_command_checksum(sizeof(IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_T));

	uart_send(m_uBuffer.auc, sizeof(IOLWM_EHCI_COMMAND_HEADER_T) + sizeof(IOLWM_PARAM_SMI_VS_RADIO_TEST_REQ_T) + 2U);

	/* Wait for an ACK packet. */
	ulResult = event_wait(&ulSize, IOLW_EHCI_SMI_EVENT_CODE_VALUE, IOLWM_SMI_ACK_OPCODE, 10);
	if( ulResult==IOLWM_RESULT_Ok )
	{
		ulResult = event_wait(&ulSize, IOLW_EHCI_SMI_EVENT_CODE_VALUE, IOLWM_SMI_VS_WRITE_CNF_OPCODE, 10);
		if( ulResult==IOLWM_RESULT_Ok )
		{
			if( ulSize<sizeof(IOLWM_PARAM_SMI_VS_WRITE_CNF_T) )
			{
				ulResult = IOLWM_RESULT_InvalidPacketSize;
			}
			else
			{
				/* Save the result. It would be overwritten by the following ACK. */
				usResult = m_uBuffer.tEvent.uPayload.tSmiVsWriteCnf.usResult;

				/* Send an ACK. */
				send_command_ack(m_uBuffer.tEvent.uHeader.t.ucMsgCounter);

				/* Check the result. */
				if( usResult!=0 )
				{
					ulResult = IOLWM_RESULT_EhciError;
				}
			}
		}
	}

	return ulResult;
}



static unsigned long module_prepareRadioTest(void)
{
	return smi_vs_radio_test(
		0,
		IOLWM_RADIO_TEST_CMD_CON_PREPARE,
		IOLWM_RADIO_TEST_CMD_MODULATION_CW,
		IOLWM_RADIO_TEST_CMD_TESTPATTERN_PRBS9,
		1,
		0,
		15
	);
}



static unsigned long module_radioTestContTx(unsigned long ulTrack, unsigned long ulFrequencyIndex)
{
	unsigned long ulResult;
	unsigned char ucTrack;
	unsigned char ucFrequencyIndex;


	if( ulTrack>0x000000ffU )
	{
		ulResult = IOLWM_RESULT_ParameterError;
	}
	else if( ulFrequencyIndex>0x000000ffU )
	{
		ulResult = IOLWM_RESULT_ParameterError;
	}
	else
	{
		ucTrack = (unsigned char)ulTrack;
		ucFrequencyIndex = (unsigned char)ulFrequencyIndex;
		ulResult = smi_vs_radio_test(
			ucTrack,
			IOLWM_RADIO_TEST_CMD_CON_TX,
			IOLWM_RADIO_TEST_CMD_MODULATION_CW,
			IOLWM_RADIO_TEST_CMD_TESTPATTERN_PRBS9,
			ucFrequencyIndex,
			0,
			15
		);
	}

	return ulResult;
}



static unsigned long module_radioTestStop(unsigned long ulTrack)
{
	unsigned long ulResult;
	unsigned char ucTrack;


	if( ulTrack>0x000000ffU )
	{
		ulResult = IOLWM_RESULT_ParameterError;
	}
	else
	{
		ucTrack = (unsigned char)ulTrack;
		return smi_vs_radio_test(
			ucTrack,
			IOLWM_RADIO_TEST_CMD_CON_STOP,
			IOLWM_RADIO_TEST_CMD_MODULATION_CW,
			IOLWM_RADIO_TEST_CMD_TESTPATTERN_PRBS9,
			1,
			0,
			15
		);
	}

	return ulResult;
}



unsigned long module(unsigned long ulParameter0, unsigned long ulParameter1, unsigned long ulParameter2, unsigned long ulParameter3 __attribute__((unused)))
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
		m_uiMsgCounter = 0;
		setup_padctrl();
		uart_initialize();
		uart_clean_receive_fifo();

		uAdr.ul = ulParameter1;
		ulResult = module_waitforpowerup(uAdr.puc);
		break;

	case IOLWM_COMMAND_ActivateSmiMode:
		/* Activate SMI Mode has no parameter.
		 */
		ulResult = module_activateSmiMode();
		break;

	case IOLWM_COMMAND_RadioTestPrepare:
		/* Prepare the radio test. */
		ulResult = module_prepareRadioTest();
		break;

	case IOLWM_COMMAND_RadioTestContTx:
		/* Run the "ContTx" radio test. */
		ulResult = module_radioTestContTx(ulParameter1, ulParameter2);
		break;

	case IOLWM_COMMAND_RadioTestStop:
		/* Stop the radio test on a track. */
		ulResult = module_radioTestStop(ulParameter1);
		break;
	}

	return ulResult;
}

/*-----------------------------------*/
