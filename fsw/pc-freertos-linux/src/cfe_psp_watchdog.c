/************************************************************************************************
** File:  cfe_psp_watchdog.c
**
** Purpose:
**   This file contains glue routines between the cFE and the OS Board Support Package ( BSP ).
**   The functions here allow the cFE to interface functions that are board and OS specific
**   and usually don't fit well in the OS abstraction layer.
**
*************************************************************************************************/

#include "cfe_psp.h"
#include "cfe_psp_config.h"

/*
** The watcdog time in milliseconds.
*/
uint32 CFE_PSP_WatchdogValue = CFE_PSP_WATCHDOG_MAX;

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogInit(void)
{
    /*
    ** Just set it to a value right now
    ** The pc-freertos-linux desktop platform does not actually implement a watchdog
    ** timeout ( but could with a signal )
    */
    CFE_PSP_WatchdogValue = CFE_PSP_WATCHDOG_MAX;
}


/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogEnable(void) { }

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogDisable(void) { }

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogService(void) { }

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
uint32 CFE_PSP_WatchdogGet(void)
{
    return CFE_PSP_WatchdogValue;
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogSet(uint32 WatchdogValue)
{
    CFE_PSP_WatchdogValue = WatchdogValue;
}
