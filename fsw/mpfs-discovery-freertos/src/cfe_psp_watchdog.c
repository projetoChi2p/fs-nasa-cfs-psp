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

#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_watchdog/mss_watchdog.h"

uint32 CFE_PSP_WatchdogValue     = CFE_PSP_WATCHDOG_MAX;

/*
 * The MSS Watchdog on the PolarFire SoC starts in a disabled state after reset.
 * However, invoking MSS_WD_configure() or MSS_WD_reload() automatically enables
 * the Watchdog. Once enabled, it cannot be disabled, and it must be periodically
 * refreshed until a system reset occurs.
 *
 * To stay consistent with the PSP API, where the watchdog should only become
 * active when CFE_PSP_WatchdogEnable() is called, this flag prevents a premature
 * and irreversible watchdog start when CFE_PSP_WatchdogService() is invoked before
 * the watchdog has been formally enabled.
 */
uint8 CFE_PSP_WatchdogIsEnabled = 0;


void wdog0_tout_e51_local_IRQHandler_9(void)
{
    MSS_WD_clear_timeout_irq(MSS_WDOG0_LO);
}


/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogInit(void)
{
    /* Disable Watchdog interruption before initialization. */
    __disable_local_irq((int8_t)WDOG0_TOUT_E51_INT);

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
    uint32_t              WatchdogValueTicks;
    mss_watchdog_config_t WatchdogConfig;
    uint8_t               error;

    WatchdogValueTicks = (CFE_PSP_WatchdogValue * (LIBERO_SETTING_MSS_APB_AHB_CLK / 256)) / 1000;

    /* Get current watchdog configuration */
    MSS_WD_get_config(MSS_WDOG0_LO, &WatchdogConfig);
    
    /* Enable watchdog interrupt */
    __enable_local_irq((int8_t)WDOG0_TOUT_E51_INT);

    /* Update timeout value */
    WatchdogConfig.time_val     = WatchdogValueTicks;
    WatchdogConfig.timeout_val  = 0x3e0u;
    WatchdogConfig.forbidden_en = MSS_WDOG_DISABLE;
    WatchdogConfig.mvrp_val     = 0;

    /* Apply configuration and enable watchdog */
    error = MSS_WD_configure(MSS_WDOG0_LO, &WatchdogConfig);

    if (error)
    {
        OS_printf("CFE_PSP_WatchdogEnable: watchdog configuration failed (error %d).", error);
    }

    CFE_PSP_WatchdogIsEnabled = 1;

    MSS_WD_reload(MSS_WDOG0_LO);
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogDisable(void)
{
    __disable_local_irq((int8_t)WDOG0_TOUT_E51_INT);
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogService(void)
{
    /*
     * Only reload the  watchdog if we previously enabled it.
     * This prevents accidental irreversible enabling of the watchdog via
     * MSS_WD_reload() if the watchdog service is called before the enable.
    */
    if (CFE_PSP_WatchdogIsEnabled)
    {
        MSS_WD_reload(MSS_WDOG0_LO);
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
    CFE_PSP_WatchdogValue = WatchdogValue;
}
