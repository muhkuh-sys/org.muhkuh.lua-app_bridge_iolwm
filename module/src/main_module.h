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


#ifndef __MAIN_MODULE_H__
#define __MAIN_MODULE_H__


typedef enum IOLWM_RESULT_ENUM
{
	IOLWM_RESULT_Ok                           =  0,
	IOLWM_RESULT_UnknownCommand               =  1,
	IOLWM_RESULT_Timout                       =  2,
	IOLWM_RESULT_InvalidPacketSize            =  3,
	IOLWM_RESULT_EhciError                    =  4,
	IOLWM_RESULT_ParameterError               =  5
} IOLWM_RESULT_T;


typedef enum IOLWM_COMMAND_ENUM
{
	IOLWM_COMMAND_WaitForPowerup              = 0,
	IOLWM_COMMAND_ActivateSmiMode             = 1,
	IOLWM_COMMAND_RadioTestPrepare            = 2,
	IOLWM_COMMAND_RadioTestContTx             = 3,
	IOLWM_COMMAND_RadioTestStop               = 4
} IOLWM_COMMAND_T;


unsigned long module(unsigned long ulParameter0, unsigned long ulParameter1, unsigned long ulParameter2, unsigned long ulParameter3);


#endif  /* __MAIN_MODULE_H__ */

