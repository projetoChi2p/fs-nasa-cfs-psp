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


#include "app_helpers.h"

uint32 CFE_PSP_WatchdogValue     = CFE_PSP_WATCHDOG_MAX;

uint8 CFE_PSP_WatchdogIsEnabled = 0;



/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogInit(void)
{
    HLP_vWatchdogDisable();
    CFE_PSP_WatchdogValue     = CFE_PSP_WATCHDOG_MAX;
    CFE_PSP_WatchdogIsEnabled = 0;
}


/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogEnable(void)
{
    /* This routine is called by HS only once, at startup, after 
     * Set() and a single call to Service();
     */

    uint32_t EffectiveWatchdogValue;
    EffectiveWatchdogValue = HLP_vWatchdogEnable(CFE_PSP_WatchdogValue);
    if (EffectiveWatchdogValue == 0)
    {
        CFE_PSP_WatchdogIsEnabled = 0;
    }
    else
    {
        CFE_PSP_WatchdogIsEnabled = 1;
    }
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogDisable(void)
{
    HLP_vWatchdogDisable();
    CFE_PSP_WatchdogIsEnabled = 0;
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogService(void)
{
    /* In HS semantics, Service() can also be used to
     * set make a new watchdog value effetive after a 
     * call to Set().
     */
    if (CFE_PSP_WatchdogIsEnabled)
    {
        HLP_vWatchdogFeed();
    }
}

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
    //Zero millis is a legal value, meaning minimum expiration time.
    //However, HS requires a non-zero value
    //Default HS time is 10 seconds

    CFE_PSP_WatchdogValue = WatchdogValue;
    if (CFE_PSP_WatchdogIsEnabled)
    {
        // re-enable to handle range and update hardware state
        CFE_PSP_WatchdogEnable();
    }
}
