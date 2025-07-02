/************************************************************************************************
** File:  cfe_psp_watchdog.c
**
** Purpose:
**   This file contains glue routines between the cFE and the OS Board Support Package ( BSP ).
**   The functions here allow the cFE to interface functions that are board and OS specific
**   and usually don't fit well in the OS abstraction layer.
**
** History:
**   2025/05/20  França. Luís    | luis.franca@inf.ufrgs.br
**
*************************************************************************************************/

#include "cfe_psp.h"
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_watchdog/mss_watchdog.h"

mss_watchdog_config_t wd0lo_config;

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogInit(void)
{
    PLIC_SetPriority(WDOG0_TOUT_PLIC, 2);
    __enable_local_irq(WDOG0_MVRP_E51_INT);


}


/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogEnable(void)
{

}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogDisable(void)
{

}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogService(void)
{

}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
uint32 CFE_PSP_WatchdogGet(void)
{

}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogSet(uint32 WatchdogValue)
{
    // CFE_PSP_WatchdogValue = WatchdogValue;
}
