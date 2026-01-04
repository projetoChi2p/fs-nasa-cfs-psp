/************************************************************************
 * Includes
 ************************************************************************/
#include "cfe_psp.h"

#include "iodriver_impl.h"
#include "iodriver_analog_io.h"

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"

/********************************************************************
 * Local Defines
 ********************************************************************/
#define FREERTOS_SYSMON_AGGREGATE_SUBSYS   0
#define FREERTOS_SYSMON_CPULOAD_SUBSYS     1
#define FREERTOS_SYSMON_AGGR_CPULOAD_SUBCH 0
#define FREERTOS_SYSMON_SAMPLE_DELAY       1000
#define FREERTOS_SYSMON_MAX_CPUS           1
#define FREERTOS_SYSMON_STACK_SIZE         4096
#define FREERTOS_SYSMON_TASK_PRIORITY     ( tskIDLE_PRIORITY + 5 )
#define FREERTOS_SYSMON_MAX_SCALE          100
#define FREERTOS_SYSMON_TASK_NAME          "freertos_sysmon"

#ifndef UNUSED_ARGUMENT
#define UNUSED_ARGUMENT(x) (void)(x)
#endif


/********************************************************************
 * Local Type Definitions
 ********************************************************************/
typedef struct freertos_sysmon_cpuload_core
{
    TickType_t last_run_time;
    TickType_t idle_last_uptime;
    CFE_PSP_IODriver_AdcCode_t avg_load;

} freertos_sysmon_cpuload_core_t;

/* This driver was made with use in a FreeRTOS single core. In case of
 * using SMP, it would be required changes to support the multi core
 * monitoring.
*/
typedef struct freertos_sysmon_cpuload_state
{
    volatile bool is_running;
    volatile bool should_run;
    TaskHandle_t  task_id;

    /* Driver only supported for single core. */
    freertos_sysmon_cpuload_core_t core;

} freertos_sysmon_cpuload_state_t;

typedef struct freertos_sysmon_state
{
    uint32_t                        local_module_id;
    freertos_sysmon_cpuload_state_t cpu_load;

} freertos_sysmon_state_t;

/********************************************************************
 * Local Function Prototypes
 ********************************************************************/
static void    freertos_sysmon_Init(uint32_t local_module_id);
static int32_t freertos_sysmon_Start(freertos_sysmon_cpuload_state_t* state);
static int32_t freertos_sysmon_Stop(freertos_sysmon_cpuload_state_t* state);

int32_t freertos_sysmon_aggregate_dispatch(uint32_t CommandCode, uint16_t Subchannel, CFE_PSP_IODriver_Arg_t Arg);
int32_t freertos_sysmon_calc_aggregate_cpu(freertos_sysmon_cpuload_state_t *state, CFE_PSP_IODriver_AdcCode_t *Val);

/* Function that starts up freertos_sysmon driver. */
int32_t freertos_sysmon_DevCmd(uint32_t CommandCode, uint16_t SubsystemId, uint16_t SubchannelId,
                               CFE_PSP_IODriver_Arg_t Arg);

/********************************************************************
 * Global Data
 ********************************************************************/
/* freertos_sysmon device command that is called by iodriver to start up freertos_sysmon */
CFE_PSP_IODriver_API_t freertos_sysmon_DevApi = {.DeviceCommand = freertos_sysmon_DevCmd};

CFE_PSP_MODULE_DECLARE_IODEVICEDRIVER(freertos_sysmon);

freertos_sysmon_state_t freertos_sysmon_global;

static const char *freertos_sysmon_subsystem_names[]  = {"aggregate", "per-cpu", NULL};
static const char *freertos_sysmon_subchannel_names[] = {"cpu-load", NULL};

/***********************************************************************
 * Global Functions
 ********************************************************************/
void freertos_sysmon_Init(uint32_t local_module_id)
{
    memset(&freertos_sysmon_global, 0, sizeof(freertos_sysmon_global));

    freertos_sysmon_global.local_module_id = local_module_id;
}

void freertos_sysmon_update_state(freertos_sysmon_cpuload_state_t *state)
{
    /*
    ** This method used to monitor the use of CPU works only for a single
    ** core case. In case of using FreeRTOS with SMP, maybe it's better to
    ** use uxTaskGetSystemState() to get the IDLE task info. Additional
    ** changes in the driver maybe also required to support the use of SMP.
    */
    freertos_sysmon_cpuload_core_t* core = &state->core;

    uint32_t uptime_at_last_calc = core->last_run_time;
    uint32_t idle_uptime_at_last_calc = core->idle_last_uptime;

    uint32_t current_uptime;
    uint32_t idle_task_uptime;
    uint32_t idle_uptime_elapsed;
    uint32_t total_elapsed;
    uint32_t current_load;
    uint32_t idle_percent_since_last_report;

    taskENTER_CRITICAL();
    idle_task_uptime = (uint32_t)xTaskGetIdleRunTimeCounter();
    current_uptime   = (uint32_t)portGET_RUN_TIME_COUNTER_VALUE();
    taskEXIT_CRITICAL();

    idle_uptime_elapsed = idle_task_uptime - idle_uptime_at_last_calc;
    total_elapsed       = current_uptime - uptime_at_last_calc;

    if (total_elapsed > 0)
    {
        idle_percent_since_last_report = ((uint32_t)( ((uint64_t)idle_uptime_elapsed * 100) / total_elapsed ));
        current_load = FREERTOS_SYSMON_MAX_SCALE - idle_percent_since_last_report;

        OS_printf("IDLE Percent: %u, Ticks Elapsed: %u, Idle Ticks: %u\n",
                  (unsigned int)idle_percent_since_last_report,
                  (unsigned int)total_elapsed,
                  (unsigned int)idle_uptime_elapsed);

        /*
        ** Mimic ADC so that "analogio" API can be used with out modification. API assumes 24 bits.
        ** First calculate out of 0x1000 and then duplicate it to expand to 24 bits. Doing this prevents
        ** an overflow. avg_load has a "real" resolution of 12 bits.
        */
        core->avg_load = (0x1000 * current_load) / FREERTOS_SYSMON_MAX_SCALE;
        core->avg_load |= (core->avg_load << 12);
    }
    else
    {
        core->avg_load = 0;
    }

    core->idle_last_uptime = idle_task_uptime;
    core->last_run_time    = current_uptime;
}

void freertos_sysmon_Task(void *pvParameters)
{
    UNUSED_ARGUMENT(pvParameters);

    freertos_sysmon_cpuload_state_t* state = &freertos_sysmon_global.cpu_load;

    while (state->should_run)
    {
        OS_TaskDelay(FREERTOS_SYSMON_SAMPLE_DELAY);
        freertos_sysmon_update_state(state);
    }

    vTaskDelete(NULL);
}

int32_t freertos_sysmon_Start(freertos_sysmon_cpuload_state_t* state)
{
    int32_t StatusCode;
    BaseType_t xReturned;

    if (state->is_running)
    {
        /* Already running, nothing to do */
        StatusCode = CFE_PSP_SUCCESS;
    }
    else
    {
        memset(state, 0, sizeof(*state));
        StatusCode = CFE_PSP_ERROR;

        state->should_run = true;

        xReturned = xTaskCreate(
            freertos_sysmon_Task,                             /* Function that implements the task. */
            FREERTOS_SYSMON_TASK_NAME,                        /* Text name for the task. */
            FREERTOS_SYSMON_STACK_SIZE / sizeof(StackType_t), /* Stack size in words, not in bytes*/
            NULL,                                             /* Parameter passed into the task. */
            FREERTOS_SYSMON_TASK_PRIORITY,                    /* Priority at which the task is created*/
            &state->task_id);                                 /* Pass the created task's handle. */

        if (xReturned == pdPASS)
        {
            state->should_run = true;
            state->is_running = true;

            StatusCode = CFE_PSP_SUCCESS;
        }
        else
        {
            OS_printf("CFE_PSP(FREERTOS_SYSMON): Failed to create monitor task.\n");

            state->is_running = false;
            state->should_run = false;

            StatusCode = CFE_PSP_ERROR;
        }
    }

    return StatusCode;
}

int32_t freertos_sysmon_Stop(freertos_sysmon_cpuload_state_t* state)
{
    if (state->is_running)
    {
        OS_printf("CFE_PSP(FREERTOS_SYSMON): Stop Monitoring\n");
        state->should_run = false;
        state->is_running = false;
        vTaskDelete(state->task_id);
    }

    return CFE_PSP_SUCCESS;
}

int32_t freertos_sysmon_calc_aggregate_cpu(freertos_sysmon_cpuload_state_t *state, CFE_PSP_IODriver_AdcCode_t *Val)
{
    /*
    ** Usually this would return the avreage of load for each CPU, but
    ** in FreeRTOS, this port is currently only supported for a single core.
    */
    *Val = state->core.avg_load;

    OS_printf("CFE_PSP(freertos_sysmon): Aggregate CPU load=%08X\n", (unsigned int)*Val);

    return CFE_PSP_SUCCESS;
}

int32_t freertos_sysmon_aggregate_dispatch(uint32_t CommandCode, uint16_t Subchannel, CFE_PSP_IODriver_Arg_t Arg)
{
    int32_t StatusCode;
    freertos_sysmon_cpuload_state_t *state;

    /* There is just one global cpuload object. */
    state = &freertos_sysmon_global.cpu_load;

    StatusCode = CFE_PSP_ERROR_NOT_IMPLEMENTED;

    switch (CommandCode)
    {
        case CFE_PSP_IODriver_NOOP:
        case CFE_PSP_IODriver_ANALOG_IO_NOOP:
            OS_printf("CFE_PSP(freertos_sysmon): Noop\n");
            StatusCode = CFE_PSP_SUCCESS;
            break;
        case CFE_PSP_IODriver_SET_RUNNING:
        {
            if (Arg.U32)
            {
                StatusCode = freertos_sysmon_Start(state);
            }
            else
            {
                StatusCode = freertos_sysmon_Stop(state);
            }
            break;
        }
        case CFE_PSP_IODriver_GET_RUNNING:
        {
            StatusCode = state->is_running;
            break;
        }
        case CFE_PSP_IODriver_SET_CONFIGURATION:
        case CFE_PSP_IODriver_GET_CONFIGURATION:
        {
            /* Not implemented for now */
            break;
        }
        case CFE_PSP_IODriver_LOOKUP_SUBSYSTEM: /**< const char * argument, looks up name and returns positive
                                                    value for subsystem number, negative value for error */
        {
            uint16_t i;

            for (i = 0; freertos_sysmon_subsystem_names[i] != NULL; ++i)
            {
                if (strcmp(Arg.ConstStr, freertos_sysmon_subsystem_names[i]) == 0)
                {
                    StatusCode = i;
                    break;
                }
            }

            break;
        }
        case CFE_PSP_IODriver_LOOKUP_SUBCHANNEL: /**< const char * argument, looks up name and returns positive
                                                    value for channel number, negative value for error */
        {
            uint16_t i;

            for (i = 0; freertos_sysmon_subchannel_names[i] != NULL; ++i)
            {
                if (strcmp(Arg.ConstStr, freertos_sysmon_subchannel_names[i]) == 0)
                {
                    StatusCode = i;
                    break;
                }
            }

            break;
        }
        case CFE_PSP_IODriver_QUERY_DIRECTION:  /**< CFE_PSP_IODriver_Direction_t argument */
        {
            CFE_PSP_IODriver_Direction_t *DirPtr = (CFE_PSP_IODriver_Direction_t *)Arg.Vptr;

            if (DirPtr != NULL)
            {
                *DirPtr = CFE_PSP_IODriver_Direction_INPUT_ONLY;
                StatusCode = CFE_PSP_SUCCESS;
            }

            break;
        }
        case CFE_PSP_IODriver_ANALOG_IO_READ_CHANNELS:
        {
            CFE_PSP_IODriver_AnalogRdWr_t *RdWr = Arg.Vptr;

            if (RdWr->NumChannels == 1 && Subchannel == FREERTOS_SYSMON_AGGR_CPULOAD_SUBCH)
            {
                StatusCode = freertos_sysmon_calc_aggregate_cpu(state, RdWr->Samples);
            }

            break;
        }
        default:
            break;
    }

    return StatusCode;
}

int32_t freertos_sysmon_cpu_load_dispatch(uint32_t CommandCode, uint16_t Subchannel, CFE_PSP_IODriver_Arg_t Arg)
{
    int32_t                          StatusCode;
    freertos_sysmon_cpuload_state_t* state;

    /* There is just one global cpuload object */
    state = &freertos_sysmon_global.cpu_load;
    StatusCode = CFE_PSP_ERROR_NOT_IMPLEMENTED;

    switch(CommandCode)
    {
        case CFE_PSP_IODriver_NOOP:
        case CFE_PSP_IODriver_ANALOG_IO_NOOP:
        {
            StatusCode = CFE_PSP_SUCCESS;
            break;
        }
        case CFE_PSP_IODriver_ANALOG_IO_READ_CHANNELS:
        {
            CFE_PSP_IODriver_AnalogRdWr_t *RdWr = Arg.Vptr;
            uint32_t                       ch;

            if (Subchannel < FREERTOS_SYSMON_MAX_CPUS && (Subchannel + RdWr->NumChannels) <= FREERTOS_SYSMON_MAX_CPUS)
            {
                for (ch = Subchannel; ch < (Subchannel + RdWr->NumChannels); ++ch)
                {
                    RdWr->Samples[ch] = state->core.avg_load;
                }

                StatusCode = CFE_PSP_SUCCESS;
            }
            else
            {
                StatusCode = CFE_PSP_ERROR;
            }
        }
        default:
            break;
    }

    return StatusCode;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/*    freertos_sysmon_DevCmd()                               */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/**
 * \brief Main entry point for API.
 *
 * This function is called through iodriver to invoke the rtems_sysmon module.
 *
 * \par Assumptions, External Events, and Notes:
 *          None
 *
 * \param[in] CommandCode  The CFE_PSP_IODriver_xxx command.
 * \param[in] SubsystemId  The monitor subsystem identifier
 * \param[in] SubchannelId The monitor subchannel identifier
 * \param[in] Arg          The arguments for the corresponding command.
 *
 * \returns Status code
 * \retval #CFE_PSP_SUCCESS if successful
 */
int32_t freertos_sysmon_DevCmd(uint32_t CommandCode, uint16_t SubsystemId, uint16_t SubchannelId,
    CFE_PSP_IODriver_Arg_t Arg)
{
    int32_t StatusCode;

    StatusCode = CFE_PSP_ERROR_NOT_IMPLEMENTED;
    switch (SubsystemId) {
        case FREERTOS_SYSMON_AGGREGATE_SUBSYS:
            StatusCode = freertos_sysmon_aggregate_dispatch(CommandCode, SubchannelId, Arg);
            break;
        case FREERTOS_SYSMON_CPULOAD_SUBSYS:
            StatusCode = freertos_sysmon_cpu_load_dispatch(CommandCode, SubchannelId, Arg);
            break;
        default:
            /* not implemented */
            break;
    }

    return StatusCode;
}
