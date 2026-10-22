#include <efi.h>
#include "rp04-efi-common.h"

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    (void)ImageHandle;
    (void)Run(SystemTable, FALSE);
    return EFI_SUCCESS;
}
