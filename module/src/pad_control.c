
#include "pad_control.h"

#include "netx_io_areas.h"



void pad_control_apply(const unsigned char *pucIndex, const unsigned char *pucConfiguration, unsigned int sizConfiguration)
{
	HOSTDEF(ptAsicCtrlArea);
	unsigned int sizCnt;
	unsigned long ulConfiguration;
	unsigned long ulOffset;
	volatile unsigned long *pulPadControl;


	pulPadControl = (volatile unsigned long*)HOSTADDR(pad_ctrl);

	for(sizCnt=0; sizCnt<sizConfiguration; ++sizCnt)
	{
		/* Get the value. */
		ulOffset = (unsigned long)(pucIndex[sizCnt]);
		ulConfiguration = (unsigned long)(pucConfiguration[sizCnt]);

		if( ulConfiguration!=PAD_CONTROL_SKIP && ulOffset!=PAD_CONTROL_SKIP )
		{
			/* Write the configuration. */
			ptAsicCtrlArea->ulAsic_ctrl_access_key = ptAsicCtrlArea->ulAsic_ctrl_access_key;  /* @suppress("Assignment to itself") */
			pulPadControl[ulOffset] = ulConfiguration;
		}
	}
}
