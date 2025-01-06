#include "cfe_psp.h"
#include "cfe_psp_config.h"
#include "cfe_psp_memory.h"

uint32_t HLP_u32GetLoResTick(void);

void CFE_PSP_GetTime(OS_time_t *time)
{
    uint32_t ticks;
    int64 microsecs;

    ticks = HLP_u32GetLoResTick();

    microsecs = ( ticks * ((int64)1000*1000) ) / CFE_PSP_GetTimerTicksPerSecond();

    *time = OS_TimeFromTotalMicroseconds(microsecs);
}

void CFE_PSP_Get_Timebase(uint32 *tbu, uint32 *tbl)
{
    /* From CFE_ES_PerfLogEntry
    **  Time is stored as 2 32 bit integers, (TimerLower32, TimerUpper32):
    **      TimerLower32 is the current value of the hardware timer register.
    **      TimerUpper32 is the number of times the timer has rolled over.
    */
    (*tbu) = 0;
    (*tbl) = HLP_u32GetLoResTick();
}
