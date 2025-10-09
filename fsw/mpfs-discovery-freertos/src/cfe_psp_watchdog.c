/************************************************************************************************
** File:  cfe_psp_watchdog.c
**
** Purpose:
**   This file contains glue routines between the cFE and the OS Board Support Package ( BSP ).
**   The functions here allow the cFE to interface functions that are board and OS specific
**   and usually don't fit well in the OS abstraction layer.
**
** History:
**   2025/05/20  Franca. Luís    | luis.franca@inf.ufrgs.br
**
*************************************************************************************************/

#include "cfe_psp.h"
#include "mpfs_hal/mss_hal.h"
#include "drivers/mss/mss_watchdog/mss_watchdog.h"

uint32 CFE_PSP_WatchdogValue;

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
    mss_watchdog_config_t WatchdogConfig;

    /* Disable Watchdog interruption before initialization. */
    __disable_local_irq((int8_t)WDOG0_TOUT_E51_INT);

    /* Reading the default config */
    MSS_WD_get_config(MSS_WDOG0_LO, &WatchdogConfig);


    WatchdogConfig.forbidden_en = MSS_WDOG_DISABLE;
    WatchdogConfig.timeout_val = 0;
    WatchdogConfig.time_val = MSS_WDOG_TIMER_MAX;

    MSS_WD_configure(MSS_WDOG0_LO, &WatchdogConfig);
}


/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogEnable(void)
{
    MSS_WD_enable_mvrp_irq(MSS_WDOG0_LO);
    __enable_local_irq((int8_t)WDOG0_TOUT_E51_INT);
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogDisable(void)
{
    MSS_WD_disable_mvrp_irq(MSS_WDOG0_LO);
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
    MSS_WD_reload(MSS_WDOG0_LO);
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
uint32 CFE_PSP_WatchdogGet(void)
{
    uint64_t WatchdogValueTicks;
    uint64_t WatchdogValueMilliSecs;

    WatchdogValueTicks = MSS_WD_current_value(MSS_WDOG0_LO);
    WatchdogValueMilliSecs = WatchdogValueTicks;

    return WatchdogValueMilliSecs;
}

/*----------------------------------------------------------------
 *
 * Implemented per public API
 * See description in header file for argument/return detail
 *
 *-----------------------------------------------------------------*/
void CFE_PSP_WatchdogSet(uint32 WatchdogValue)
{
    uint64_t WatchdogValueTicks;
    mss_watchdog_config_t WatchdogConfig;

    WatchdogValueTicks = (WatchdogValue * (LIBERO_SETTING_MSS_SYSREG_CLKS_VERSION)) / 100;

    MSS_WD_get_config(MSS_WDOG0_LO, &WatchdogConfig);

    WatchdogConfig.time_val = WatchdogValueTicks;

    MSS_WD_reload(MSS_WDOG0_LO);
}
