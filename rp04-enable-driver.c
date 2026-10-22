#include <efi.h>
#include "rp04-efi-common.h"

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;
    /*
     * This entry point reports driver-load success even if the guarded RP04
     * operation fails; Run's EFI console messages report the operation result.
     */
    (void)Run(SystemTable, FALSE);
    return EFI_SUCCESS;
}
