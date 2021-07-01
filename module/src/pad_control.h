
#include <stddef.h>

#include "netx_io_areas.h"



#ifndef __PAD_CONTROL_H__
#define __PAD_CONTROL_H__


#define PAD_DRIVING_STRENGTH_Low  0
#define PAD_DRIVING_STRENGTH_High 1

#define PAD_INPUT_Disable 0
#define PAD_INPUT_Enable  1

#define PAD_PULL_Disable 0
#define PAD_PULL_Enable  1


#define DO_CONCAT_PAD_REG(a)   ulPad_ctrl_##a
#define DO_CONCAT_PAD_AREG(a) aulPad_ctrl_##a
#define PAD_REG2OFFSET(name)        ((unsigned char)(offsetof(NX90_PAD_CTRL_AREA_T,DO_CONCAT_PAD_REG(name))/sizeof(unsigned long)))
#define PAD_AREG2OFFSET(name,index) ((unsigned char)((offsetof(NX90_PAD_CTRL_AREA_T,DO_CONCAT_PAD_AREG(name))/sizeof(unsigned long))+index))

#define PAD_CONTROL_SKIP 0xffU

#define PAD_CONFIGURATION(DRIVING_STRENGTH, PULL_ENABLE, INPUT_ENABLE) ((DRIVING_STRENGTH)|((PULL_ENABLE)<<4U)|((INPUT_ENABLE)<<6U))

#define DO_CONCAT_PAD_DEFAULT(a) pad_ctrl_##a
#define PAD_DEFAULT(name) (HOSTDFLT(DO_CONCAT_PAD_DEFAULT(name)))


#define NUMBER_OF_PAD_CTRL (sizeof(NX90_PAD_CTRL_AREA_T)/sizeof(unsigned long))

typedef struct PAD_CTRL_VALUES_STRUCT
{
	unsigned char aucValues[NUMBER_OF_PAD_CTRL];
} PAD_CTRL_VALUES_T;


void pad_control_apply(const unsigned char *pucIndex, const unsigned char *pucConfiguration, unsigned int sizConfiguration);


#endif  /* __PAD_CONTROL_H__ */

