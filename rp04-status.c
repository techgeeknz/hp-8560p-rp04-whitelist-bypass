#include <efi.h>
#include "rp04-efi-common.h"

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *RootBridge;
    UINT32 Value;

    (void)ImageHandle;
    Out(SystemTable, L"RP04 status\r\n");
    Status = FindPch(SystemTable, &RootBridge);
    if (EFI_ERROR(Status)) {
        Out(SystemTable, L"Intel PCH not found.\r\n");
        return Status;
    }
    Status = ReadReg(RootBridge, &Value);
    if (EFI_ERROR(Status)) {
        Out(SystemTable, L"Register read failed.\r\n");
        return Status;
    }
    Out(SystemTable, (Value & RP4D_MASK) ? L"RP04 disabled (RP4D=1).\r\n"
                                         : L"RP04 enabled (RP4D=0).\r\n");
    return EFI_SUCCESS;
}
