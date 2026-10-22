#ifndef RP04_EFI_COMMON_H
#define RP04_EFI_COMMON_H

#include <efi.h>
#include <efipciio.h>

/*
 * Verified for the EliteBook 8560p, board ID 1618, BIOS 68SCF F.67:
 * RP04 is 00:1c.3. In the DSDT, RP4D is bit 3 at RCRB offset 0x341A;
 * that is bit 19 of the dword at offset 0x3418.
 */
#define RP04_RCBA 0xFED1C000ULL
#define RP04_REG (RP04_RCBA + 0x3418)
#define RP4D_MASK 0x00080000U
#define RP04_PCI EFI_PCI_ADDRESS(0, 28, 3)
#define PCH_PCI EFI_PCI_ADDRESS(0, 31, 0)

static EFI_GUID RootBridgeGuid = EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_GUID;

static VOID
Out(EFI_SYSTEM_TABLE *St, CHAR16 *Text)
{
    uefi_call_wrapper(St->ConOut->OutputString, 2, St->ConOut, Text);
}

static EFI_STATUS
PciRead(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb, UINT32 Reg, UINT32 *Value)
{
    return uefi_call_wrapper(
        Rb->Pci.Read, 5, Rb, EfiPciIoWidthUint32, PCH_PCI + Reg, 1, Value
        );
}

static EFI_STATUS
FindPch(EFI_SYSTEM_TABLE *St, EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL **Rb)
{
    EFI_STATUS S;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *P;
    UINT32 Id, Class, Rcba;

    P = NULL;
    /*
     * Fail closed: only use the hard-coded RCRB address after confirming the
     * expected Intel LPC/PCH function and that its RCBA points there.
     */
    S = uefi_call_wrapper(
        St->BootServices->LocateProtocol, 3, &RootBridgeGuid, NULL, (VOID **)&P
        );
    if (EFI_ERROR(S) || P == NULL || P->SegmentNumber != 0)
        return EFI_NOT_FOUND;
    if (EFI_ERROR(PciRead(P, 0, &Id)) || (Id & 0xFFFF) != 0x8086)
        return EFI_NOT_FOUND;
    if (EFI_ERROR(PciRead(P, 8, &Class)) || (Class >> 16) != 0x0601)
        return EFI_NOT_FOUND;
    if (EFI_ERROR(PciRead(P, 0xF0, &Rcba)) ||
        !(Rcba & 1) || (Rcba & 0xFFFFC000U) != RP04_RCBA)
        return EFI_NOT_FOUND;

    *Rb = P;
    return EFI_SUCCESS;
}

static EFI_STATUS
ReadReg(EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb, UINT32 *Value)
{
    return uefi_call_wrapper(
        Rb->Mem.Read, 5, Rb, EfiPciIoWidthUint32, RP04_REG, 1, Value
        );
}

static EFI_STATUS
WriteReg(
    EFI_SYSTEM_TABLE *St,
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb,
    UINT32 Value
    )
{
    EFI_STATUS S;
    UINT32 Readback;

    S = uefi_call_wrapper(
        Rb->Mem.Write, 5, Rb, EfiPciIoWidthUint32, RP04_REG, 1, &Value
        );
    if (EFI_ERROR(S))
        return S;
    S = uefi_call_wrapper(Rb->Flush, 1, Rb);
    if (EFI_ERROR(S))
        return S;
    S = ReadReg(Rb, &Readback);
    if (EFI_ERROR(S))
        return S;
    /* Verify only the control bit; other register bits are intentionally kept. */
    if ((Readback & RP4D_MASK) != (Value & RP4D_MASK))
        return EFI_DEVICE_ERROR;

    Out(St, L"Readback verified.\r\n");
    return EFI_SUCCESS;
}

static EFI_STATUS
Rp04LinkActive(
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb,
    BOOLEAN *Active
    )
{
    EFI_STATUS S;
    UINT32 Value;
    UINT32 Capabilities;
    UINT32 LinkStatus;
    UINT8 Capability;
    UINTN Count;

    *Active = FALSE;
    /*
     * Confirm RP04 is present and advertises a capability list before walking
     * PCI config space. These fixed offsets are the PCI status/capability
     * pointer fields in the standard configuration header.
     */
    S = uefi_call_wrapper(
        Rb->Pci.Read, 5, Rb, EfiPciIoWidthUint32, RP04_PCI + 0x04, 1, &Value
        );
    if (EFI_ERROR(S))
        return S;
    if ((Value & 0xFFFF) == 0xFFFF || (Value & 0xFFFF) == 0)
        return EFI_NOT_FOUND;
    if (!(Value & 0x00100000U))
        return EFI_UNSUPPORTED;

    S = uefi_call_wrapper(
        Rb->Pci.Read, 5, Rb, EfiPciIoWidthUint32, RP04_PCI + 0x34, 1, &Value
        );
    if (EFI_ERROR(S))
        return S;
    Capability = (UINT8)(Value & 0xFCU);

    /* Capability pointers are aligned; the count also bounds malformed loops. */
    for (Count = 0; Capability >= 0x40 && Count < 48; Count++) {
        S = uefi_call_wrapper(
            Rb->Pci.Read, 5, Rb, EfiPciIoWidthUint32,
            RP04_PCI + Capability, 1, &Capabilities
            );
        if (EFI_ERROR(S))
            return S;
        if ((Capabilities & 0xFFU) == 0x10U) {
            /*
             * PCI Express capability Link Status is at capability + 0x10.
             * DLLLA is bit 13 of its 16-bit value (bit 29 in this dword read).
             */
            S = uefi_call_wrapper(
                Rb->Pci.Read, 5, Rb, EfiPciIoWidthUint32,
                RP04_PCI + Capability + 0x10, 1, &LinkStatus
                );
            if (EFI_ERROR(S))
                return S;
            *Active = (BOOLEAN)((LinkStatus & 0x20000000U) != 0);
            return EFI_SUCCESS;
        }
        Capability = (UINT8)((Capabilities >> 8) & 0xFCU);
    }

    return EFI_UNSUPPORTED;
}

static EFI_STATUS
EnableIfOccupied(
    EFI_SYSTEM_TABLE *St,
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb
    )
{
    EFI_STATUS S;
    UINT32 Value;
    BOOLEAN LinkActive;
    UINTN Attempt;

    Out(St, L"Checking RP04 link...\r\n");
    /*
     * Allow up to ten status checks, 100 ms apart (about 900 ms total).
     * If a usable active link cannot be confirmed, restore the disabled state
     * rather than leaving an unverified RP04 enabled.
     */
    for (Attempt = 0; Attempt < 10; Attempt++) {
        S = Rp04LinkActive(Rb, &LinkActive);
        if (S == EFI_SUCCESS && LinkActive) {
            Out(St, L"PCIe device detected; RP04 remains enabled.\r\n");
            return EFI_SUCCESS;
        }
        if (EFI_ERROR(S) && S != EFI_NOT_FOUND)
            break;
        if (Attempt < 9)
            uefi_call_wrapper(St->BootServices->Stall, 1, 100000);
    }

    Out(St, S == EFI_UNSUPPORTED
            ? L"Could not verify RP04 link; disabling RP04.\r\n"
            : L"No PCIe device detected; disabling RP04.\r\n");
    S = ReadReg(Rb, &Value);
    if (EFI_ERROR(S)) {
        Out(St, L"Register read failed; could not disable RP04.\r\n");
        return S;
    }
    if (Value & RP4D_MASK) {
        Out(St, L"RP04 is already disabled.\r\n");
        return EFI_NOT_FOUND;
    }

    S = WriteReg(St, Rb, Value | RP4D_MASK);
    if (EFI_ERROR(S)) {
        Out(St, L"Failed to disable empty RP04.\r\n");
        return S;
    }
    Out(St, L"Empty RP04 disabled.\r\n");
    return EFI_NOT_FOUND;
}

static EFI_STATUS
Run(EFI_SYSTEM_TABLE *St, BOOLEAN Disable)
{
    EFI_STATUS S;
    EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *Rb;
    UINT32 Value;

    /*
     * Disable=TRUE is the simple disable utility. Disable=FALSE is the guarded
     * enable path: clear RP4D if needed, then retain it only with an active link.
     */
    Out(St, L"RP04 test\r\n");
    S = FindPch(St, &Rb);
    if (EFI_ERROR(S)) {
        Out(St, L"Intel PCH not found; no write performed.\r\n");
        return S;
    }
    S = ReadReg(Rb, &Value);
    if (EFI_ERROR(S)) {
        Out(St, L"Register read failed; no write performed.\r\n");
        return S;
    }

    Out(St, (Value & RP4D_MASK) ? L"RP04 disabled.\r\n"
                                : L"RP04 enabled.\r\n");
    if (Disable && (Value & RP4D_MASK)) {
        Out(St, L"No change needed.\r\n");
        return EFI_SUCCESS;
    }

    if (Disable) {
        Out(St, L"Disabling RP04...\r\n");
        S = WriteReg(St, Rb, Value | RP4D_MASK);
        if (EFI_ERROR(S)) {
            Out(St, L"Write/readback failed.\r\n");
            return S;
        }
    } else {
        if (Value & RP4D_MASK) {
            Out(St, L"Enabling RP04...\r\n");
            S = WriteReg(St, Rb, Value & ~RP4D_MASK);
            if (EFI_ERROR(S)) {
                Out(St, L"Write/readback failed.\r\n");
                return S;
            }
        } else {
            Out(St, L"RP04 is already enabled; checking for a device.\r\n");
        }
        return EnableIfOccupied(St, Rb);
    }

    Out(St, L"Link recovery is not guaranteed.\r\n");
    return EFI_SUCCESS;
}

#endif
