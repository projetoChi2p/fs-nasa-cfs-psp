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
** \author   Luís França      (UFRGS)
**/


#include "cfe_psp.h"
#include "cfe_psp_config.h"
#include "cfe_psp_memory.h"
#include "cfe_psp_module.h"
#include "fatfs/ff.h"

// target_config.h provides GLOBAL_CONFIGDATA object for CFE runtime settings
#include <target_config.h>

#include <os-shared-globaldefs.h>

// #include "mpfs_hal/mss_hal.h"
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
    int32 Status;
    uint32 reset_type;
    uint32 reset_subtype;
    uint32 hlp_reset_type;
    osal_id_t    fs_id;

    FRESULT result;

    /*
    ** Initialize the OS API data structures
    */
    Status = OS_API_Init();
    if (Status != OS_SUCCESS)
    {
        /* irrecoverable error if OS_API_Init() fails. */
        /* note: use printf here, as OS_printf may not work */
        OS_printf("CFE_PSP: OS_API_Init() failure\n");
        CFE_PSP_Panic(Status);
    }

    /*
    ** Setup the pointer to the reserved area in vxWorks.
    ** This must be done before any of the reset variables are used.
    */
    CFE_PSP_SetupReservedMemoryMap();

    /* At current implementatiom this platform does not support fixed file systems.
     *
     * Our previous implementation used FAT filesystem for RAMDISK, planning in a
     * commom driver layer for both SDcard and RAMDISK.
     *
     * Aiming in implementation of fixed file systems in on-chip Flash, we also
     * integrated Xilinx MFS, which can use both ROM and RAM.
     *
     */
    // @TODO USe OS_FileSysAddFixedMap() to initialize ROM, Flash or SDcard filesystem supporting CFE_PSP_NONVOL_STARTUP_FILE.
    //
    // FIXED filesystem:
    // - OS_FileSysAddFixedMap()
    //   calls OS_FileSysStartVolume_Impl(), no format, then
    //   calls OS_FileSysMountVolume_Impl()
    // RAM/VIRTUAL filesystem:
    // - OS_initfs()/OS_mkfs()/OS_FileSys_Initialize()
    //   calls OS_FileSysStartVolume_Impl() and OS_FileSysFormatVolume_Impl() only
    // - Requires explicit call to OS_mount()
    //
    // Volatile filesystem is initialized/formatted from within cFE ES using device name /ramdev0. It is done using
    // OS_initfs() or OS_mkfs(), and then OS_mount("/ramdev0", CFE_PLATFORM_ES_RAM_DISK_MOUNT_STRING).
    // OS_mount() does not operate on filesystems marked as FIXED
    // OS_initfs() and OS_mkfs() operates on filesystems not marked as FIXED
    // OS_initfs() or OS_mkfs() are precondition to OS_mount().
    // If successful, OS_mount() marks the filesys as mounted and VIRTUAL.
    // Linkage from device to filesystem is done by OS_FileSysMountVolume_Impl()
    // Both OS_initfs() and OS_mkfs() rely on OS_FileSys_Initialize(), the difference
    // being only that init should not format if init failed, while OS_mkfs() should format on failure to init.
    // OS_FileSys_Initialize() relies on OS_FileSysStartVolume_Impl() and, on fail to start and should format,
    // calls OS_FileSysFormatVolume_Impl()

    /*
    ** Set up a small RAM filesystem with cFE start-up script content initialized
    ** from embedded file. See also <cpuname>_EMBED_FILELIST in targets.cmake.
    */

    // TODO replace this with OS_FileSysAddFixedMap()
    /*
    osal_id_t FileStartupScript;
    extern const unsigned char STARTUP_SCR_DATA[];          // Embedded by <cpuname>_EMBED_FILELIST in targets.cmake
    extern const unsigned long STARTUP_SCR_SIZE;
    #define FREERTOS_RAMDISK_SECTOR_SIZE 128                // must be 512 for FreeRTOS+FAT RAMDISK
    #define ES_STARTUP_BLOCKS            26                 // minium 128 for FreeRTOS+FAT RAMDISK
    #define ES_STARTUP_VOL_LABEL         "RAM1"             // Must start with /RAM if RAMDISK, see also OS_FILESYS_RAMDISK_VOLNAME_PREFIX
    #define ES_STARTUP_DEVICE            "/ramdev1"
    #define ES_STARTUP_MOUNT             "/cf"              // Same prefix as CFE_PLATFORM_ES_NONVOL_STARTUP_FILE
    */

    /* Make the file system */
    /*
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
    */

   /*
   Status = OS_mount(ES_STARTUP_DEVICE, ES_STARTUP_MOUNT);
   if (Status != OS_SUCCESS)
   {
    OS_printf("CFE_PSP: OS_mount() failed.\n");
    CFE_PSP_Panic(Status);
    }
    */

   /*
   ** Initialize the statically linked modules (if any)
   */

    Status = OS_FileSysAddFixedMap(&fs_id, "0:/cf", "/cf");
    if (Status != OS_SUCCESS)
    {
        /* Print for informational purposes --
        * startup can continue, but load the tables may fail later,
        * depending on config. */
        OS_printf("CFE_PSP: OS_FileSysAddFixedMap() failure: %d\n", (int)Status);
    }

    CFE_PSP_ModuleInit();

    if (CFE_PSP_Setup() != CFE_PSP_SUCCESS)
    {
        CFE_PSP_Panic(CFE_PSP_ERROR);
    }

    reset_type = HLP_uGetResetType();

    if (hlp_reset_type == RESET_TYPE_POWERON)
    {
        OS_printf("CFE_PSP: POWERON Reset: Power Cycle.\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_POWER_CYCLE;
    }
    else if (hlp_reset_type == RESET_TYPE_EXTERNAL)
    {
        OS_printf("CFE_PSP POWERON Reset: Fabric (Push button) reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_PUSH_BUTTON;
    }
    else if (hlp_reset_type == RESET_TYPE_WATCHDOG)
    {
        OS_printf("CFE_PSP PROCESSOR Reset: Watchdog reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_PROCESSOR;
        reset_subtype = CFE_PSP_RST_SUBTYPE_HW_WATCHDOG;
    }
    else if (hlp_reset_type == RESET_TYPE_SOFTWARE)
    {
        OS_printf("CFE_PSP PROCESSOR Reset: Software Reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_PROCESSOR;
        reset_subtype = CFE_PSP_RST_SUBTYPE_RESET_COMMAND;
    }
    else
    {
        OS_printf("CFE_PSP POWERON Reset: Undefined Reset.\n");
        reset_type    = CFE_PSP_RST_TYPE_POWERON;
        reset_subtype = CFE_PSP_RST_SUBTYPE_UNDEFINED_RESET;
    }

    if (reset_type == CFE_PSP_RST_TYPE_PROCESSOR)
    {
        CFE_PSP_ReservedMemoryMap.BootPtr->bsp_reset_type = CFE_PSP_RST_TYPE_POWERON;
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
