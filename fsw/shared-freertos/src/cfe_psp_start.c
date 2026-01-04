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

#include "app_helpers.h"

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
    uint32    bsp_reset_type;
    osal_id_t fs_id;

    /*
    ** Initialize the OS API data structures
    */
    Status = OS_API_Init();
    if (Status != OS_SUCCESS)
    {
        /* Unrecoverable error if OS_API_Init() fails. */
        /* note: OS_printf may not work */
        OS_printf("CFE_PSP: OS_API_Init() failure\n");
        CFE_PSP_Panic(Status);
    }

    /*
    ** Setup the pointer to the reserved area.
    ** This must be done before any of the reset variables are used.
    */
    CFE_PSP_SetupReservedMemoryMap();

    #ifdef OS_FILESYSTEM_NON_VOLATILE_IS_FATFS

    Status = OS_FileSysAddFixedMap(&fs_id, "0:/cf", "/cf");
    if (Status != OS_SUCCESS)
    {
        /* Print for informational purposes --
        * startup can continue, but load the tables may fail later,
        * depending on config. */
        OS_printf("CFE_PSP: OS_FileSysAddFixedMap() failure: %d\n", (int)Status);
    }

    #else

    /*
    ** Set up a small RAM filesystem with cFE start-up script content initialized
    ** from embedded file. See also <cpuname>_EMBED_FILELIST in targets.cmake.
    */
    osal_id_t FileStartupScript;
    extern const unsigned char STARTUP_SCR_DATA[];          // Embedded by <cpuname>_EMBED_FILELIST in targets.cmake
    extern const unsigned long STARTUP_SCR_SIZE;
    #define FREERTOS_RAMDISK_SECTOR_SIZE 128                // must be 512 for FreeRTOS+FAT RAMDISK
    #define ES_STARTUP_BLOCKS            26                 // minium 128 for FreeRTOS+FAT RAMDISK
    #define ES_STARTUP_VOL_LABEL         "RAM1"             // Must start with /RAM if RAMDISK, see also OS_FILESYS_RAMDISK_VOLNAME_PREFIX
    #define ES_STARTUP_DEVICE            "/ramdev1"
    #define ES_STARTUP_MOUNT             "/cf"              // Same prefix as CFE_PLATFORM_ES_NONVOL_STARTUP_FILE

    /* Make the file system */
    Status = OS_mkfs( 0,   // NULL as RAMDISK address implies dynamic allocation
        ES_STARTUP_DEVICE,
        ES_STARTUP_VOL_LABEL,
        OSAL_SIZE_C(FREERTOS_RAMDISK_SECTOR_SIZE),
        OSAL_BLOCKCOUNT_C(ES_STARTUP_BLOCKS) );
    if (Status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: OS_mkfs() failed.\n");
        CFE_PSP_Panic(Status);
    }

    Status = OS_mount(ES_STARTUP_DEVICE, ES_STARTUP_MOUNT);
    if (Status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: OS_mount() failed.\n");
        CFE_PSP_Panic(Status);
    }

    Status = OS_OpenCreate(&FileStartupScript, CFE_PSP_NONVOL_STARTUP_FILE, OS_FILE_FLAG_CREATE, OS_WRITE_ONLY);
    if (Status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: OS_OpenCreate() failure\n");
        CFE_PSP_Panic(Status);
    }

    Status = OS_write(FileStartupScript, STARTUP_SCR_DATA, STARTUP_SCR_SIZE);
    if (Status != STARTUP_SCR_SIZE)
    {
        OS_printf("CFE_PSP: OS_write() failure\n");
        CFE_PSP_Panic(Status);
    }

    Status = OS_close(FileStartupScript);
    if (Status != OS_SUCCESS)
    {
        OS_printf("CFE_PSP: OS_close() failure\n");
        CFE_PSP_Panic(Status);
    }

    #endif

    /*
    ** Initialize the statically linked modules (if any)
    */
    CFE_PSP_ModuleInit();

    if (CFE_PSP_Setup() != CFE_PSP_SUCCESS)
    {
        CFE_PSP_Panic(CFE_PSP_ERROR);
    }

    /* Read the Reset Status Register.
     * After a reset occurs, the register should be read and 
     * then zero written to allow the next reset event to be 
     * correctly captured.
     */
    /* TODO We shall distinguish between a warm-reset triggered
     *      by FDIR or scheduled action, and a warm-reset
     *      commanded from ground station, especially if ground
     *      station intent was to force a cold-reset.
     */
    bsp_reset_type = HLP_uGetResetType();
    switch (bsp_reset_type) {
        case HLP_RESET_TYPE_POWERON:
            OS_printf("CFE_PSP: POWERON RESET: Power Up\n");
            reset_type    = CFE_PSP_RST_TYPE_POWERON;
            reset_subtype = CFE_PSP_RST_SUBTYPE_POWER_CYCLE;
            break;
        case HLP_RESET_TYPE_WATCHDOG:
            OS_printf("CFE_PSP: PROCESSOR Reset: Watchdog Reset.\n");
            reset_type    = CFE_PSP_RST_TYPE_PROCESSOR;
            reset_subtype = CFE_PSP_RST_SUBTYPE_HW_WATCHDOG;
            break;
        case HLP_RESET_TYPE_SOFTWARE:
            OS_printf("CFE_PSP: PROCESSOR Reset: CPU Warm Reset\n");
            reset_type    = CFE_PSP_RST_TYPE_PROCESSOR;
            reset_subtype = CFE_PSP_RST_SUBTYPE_RESET_COMMAND;
            break;
        case HLP_RESET_TYPE_EXTERNAL:
            OS_printf("CFE_PSP: PROCESSOR Reset: Push Button Reset.\n");
            reset_type    = CFE_PSP_RST_TYPE_POWERON;
            reset_subtype = CFE_PSP_RST_SUBTYPE_PUSH_BUTTON;
            break;
        case HLP_RESET_TYPE_DEBUG:
            OS_printf("CFE_PSP: POWERON Reset: Debugger Reset.\n");
            reset_type    = CFE_PSP_RST_TYPE_POWERON;
            reset_subtype = CFE_PSP_RST_SUBTYPE_POWER_CYCLE;
            break;
        default:
            OS_printf("CFE_PSP: UNDEFINED Reset. Reset Register: %x\n", bsp_reset_type);
            reset_type    = CFE_PSP_RST_TYPE_POWERON;
            reset_subtype = CFE_PSP_RST_SUBTYPE_UNDEFINED_RESET;
            break;
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
