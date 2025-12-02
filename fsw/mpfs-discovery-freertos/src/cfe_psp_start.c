/************************************************************************
 * NASA Docket No. GSC-18,719-1, and identified as “core Flight System: Bootes”
 *
 *  Copyright (c) 2024 Universidade Federal do Rio Grande do Sul (UFRGS)
 *
 *
 *  Copyright (c) 2021 Patrick Paul
 *  SPDX-License-Identifier: MIT-0
 *
 *
 * Copyright (c) 2020 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
** File:  cfe_psp_start.c
**
** \file     cfe_psp_start.c
** \brief    Purpose: cFE PSP main entry point.
** \ingroup  freertos
** \author   Patrick Paul (https://github.com/pztrick)
** \author   Fabio Benevenuti (UFRGS)
** \author   Luis Franca      (UFRGS)
**/


#include "cfe_psp.h"
#include "cfe_psp_config.h"
#include "cfe_psp_memory.h"
#include "cfe_psp_module.h"

// target_config.h provides GLOBAL_CONFIGDATA object for CFE runtime settings
#include <target_config.h>

#include <os-shared-globaldefs.h>

#include "mpfs_hal/mss_hal.h"

// PSP needs to call the CFE entry point
#define CFE_PSP_MAIN_FUNCTION (*GLOBAL_CONFIGDATA.CfeConfig->SystemMain)

// CFE needs a startup script file
#define CFE_PSP_NONVOL_STARTUP_FILE (GLOBAL_CONFIGDATA.CfeConfig->NonvolStartupFile)


int CFE_PSP_Setup(void)
{
    OS_printf("CFE PSP Setup called. No action.\n");
    return CFE_PSP_SUCCESS;
}

void CFE_PSP_Panic(int32 ErrorCode)
{
    OS_printf("CFE PSP Panic. ErrorCode:%ld .\n", (long int)ErrorCode);
    OS_ApplicationExit(ErrorCode);
}

// OSAL:main() invokes PSP:OS_Application_Startup() inside a FreeRTOS task
void OS_Application_Startup(void)
{
    int32     Status;
    uint32    reset_type;
    uint32    reset_subtype;
    uint32    reset_register;
    osal_id_t fs_id;

    /*
    ** Initialize the OS API data structures
    */
    Status = OS_API_Init();
    if (Status != OS_SUCCESS)
    {
        /* Irrecoverable error if OS_API_Init() fails. */
        /* note: OS_printf may not work */
        OS_printf("CFE_PSP: OS_API_Init() failure\n");
        CFE_PSP_Panic(Status);
    }

    /*
    ** Setup the pointer to the reserved area.
    ** This must be done before any of the reset variables are used.
    */
    CFE_PSP_SetupReservedMemoryMap();

    Status = OS_FileSysAddFixedMap(&fs_id, "0:/cf", "/cf");
    if (Status != OS_SUCCESS)
    {
        /* Print for informational purposes --
        * startup can continue, but load the tables may fail later,
        * depending on config. */
        OS_printf("CFE_PSP: OS_FileSysAddFixedMap() failure: %d\n", (int)Status);
    }

    /*
    ** Initialize the statically linked modules (if any)
    */
    CFE_PSP_ModuleInit();

    if (CFE_PSP_Setup() != CFE_PSP_SUCCESS)
    {
        CFE_PSP_Panic(CFE_PSP_ERROR);
    }

    /* Read the Reset Status Register.
     * After a reset occurs, the register should be read and then zero written
     * to allow the next reset event to be correctly captured.
     */
    reset_register = SYSREG->RESET_SR;
    SYSREG->RESET_SR = 0;

    if (reset_register == 0x1ff)
    {
        OS_printf("CFE_PSP: POWERON RESET: Power Up\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_POWER_CYCLE;
    }
    else if (reset_register & RESET_SR_CPU_SOFTCB_BUS_RESET_MASK)
    {
        OS_printf("CFE_PSP: PROCESSOR Reset: CPU Warm Reset\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_RESET_COMMAND;
    }
    else if (reset_register & RESET_SR_DEBUGER_RESET_MASK)
    {
        OS_printf("CFE_PSP: POWERON Reset: Debugger Reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_HWDEBUG_RESET;
    }
    else if (reset_register & RESET_SR_WDOG_RESET_MASK)
    {
        OS_printf("CFE_PSP: PROCESSOR Reset: Watchdog Reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_HW_WATCHDOG;
    }
    else if (reset_register & RESET_SR_FABRIC_RESET_MASK)
    {
        OS_printf("CFE_PSP: PROCESSOR Reset: Push Button Reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_PROCESSOR;
        reset_subtype = CFE_PSP_RST_SUBTYPE_PUSH_BUTTON;
    }
    else
    {
        OS_printf("CFE_PSP: UNDEFINED Reset. Reset Register: %x\n", reset_register);
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_UNDEFINED_RESET;
    }

    /*
    ** Initialize the reserved memory
    */
    CFE_PSP_InitProcessorReservedMemory(reset_type);

    /*
    ** Call cFE entry point. This will return when cFE startup
    ** is complete.
    */
    CFE_PSP_MAIN_FUNCTION(reset_type, reset_subtype, 1, CFE_PSP_NONVOL_STARTUP_FILE);
}
