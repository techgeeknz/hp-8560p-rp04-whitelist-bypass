# HP EliteBook 8560p RP04 wireless-port workaround

This repository contains small UEFI utilities for inspecting and changing the
PCIe root-port disable bit used by the HP EliteBook 8560p firmware for RP04.
The enable utility and rEFInd-loadable driver re-enable RP04 only when they
can confirm an active PCIe link; otherwise, they disable RP04 again.

This is a hardware-specific workaround, not a general HP whitelist remover.
It does not remove or dismiss the HP POST wireless warning. The warning must
still be acknowledged before rEFInd starts.

## Tested hardware and results

Confirmed on an HP EliteBook 8560p, system/board ID `1618`, BIOS `68SCF F.67`,
with an Intel QM67 chipset:

- With an Intel Centrino WLAN card installed, the enable utility detected an
  active link and left RP04 enabled.
- A non-approved Broadcom WLAN card enumerated after RP04 was re-enabled.
- A Ralink RT3290 WLAN/Bluetooth card enumerated as PCI functions `04:00.0`
  and `04:00.1`; its Wi-Fi was also confirmed working.
- With the WLAN slot empty, the rEFInd-loaded driver disabled RP04 again.

These are results for one laptop and firmware revision. Other 8560p revisions
and related models have not been tested. Do not assume they use the same
register, address, or firmware behavior.

## Files

- `rp04-status.c` — reports whether the RP04 disable bit is set.
- `rp04-enable.c` — clears the disable bit, verifies an active PCIe link, and
  re-disables RP04 if the link is not confirmed.
- `rp04-disable.c` — sets the RP04 disable bit.
- `rp04-enable-driver.c` — invokes the same guarded enable path as
  `rp04-enable.c`, as an EFI boot-service driver.
- `rp04-efi-common.h` — shared chipset discovery, register access, and link
  detection.
- `build-rp04-efi.sh` — builds the three EFI applications and the driver.
- `*.efi` — prebuilt x86-64 EFI images corresponding to the source.

The status and disable applications only inspect or change the RP04 disable
bit. The guarded enable path additionally checks the PCIe link status.

## Build

On Fedora, install GCC, binutils, and GNU-EFI development files (the
`gnu-efi-devel` package), then run:

```sh
./build-rp04-efi.sh
```

The script expects GNU-EFI under `/usr` by default. Set `GNU_EFI_PREFIX` if
your installation uses a different prefix.

## Use as rEFInd driver

Back up the existing rEFInd configuration and EFI files before changing the
ESP. Copy `rp04-enable-driver.efi` into rEFInd's architecture-specific driver
directory, normally `EFI/refind/drivers_x64/` on x86-64 systems. rEFInd
normally detects drivers in its architecture-specific driver directory
automatically; if driver-directory scanning has been customized, check the
installed version's rEFInd documentation. Verify that the driver is actually
loaded.

The driver runs after the HP firmware POST check and after the warning is
dismissed. It performs a bounded link check of up to about one second. If it
cannot verify an active link, it sets RP04 disabled again. A slow or unusual
endpoint could therefore be treated as absent.

## What it changes

The code verifies the Intel PCH at PCI address `00:1f.0` and checks that its
Root Complex Register Block is enabled at `0xFED1C000`. It then accesses the
RP04 disable register at `0xFED1F418` and changes only bit 19 (`RP4D`, mask
`0x00080000`), preserving the other bits. After a write, it flushes and reads
back the register.

For link detection, it reads PCI configuration space at `00:1c.3`, walks the
capability list to the PCI Express capability, and checks the Data Link Layer
Link Active bit. It checks for a link, not for a particular card model or a
working operating-system driver.

## Safety and recovery

This code writes chipset MMIO and can affect hardware initialization. Use it
only on hardware whose register mapping has been independently verified.
Keep a working EFI shell or other recovery path available, and do not test
with important data at risk. The source is provided as-is; no guarantee is
made for other machines or firmware versions.

This project does not modify firmware, bypass BIOS signature checks, or
perform any pre-POST code execution. It does not inject keyboard input.

## Evidence and references

- The HP EliteBook 8560p Maintenance and Service Guide documents the machine's
  WLAN module options and service procedures:
  <https://h10032.www1.hp.com/ctg/Manual/c03052147.pdf>
- The supplied F.67 DSDT identifies RP04 as PCIe root port `00:1c.3` and
  describes its firmware disable state.
- The F.67 firmware's WLAN failure path sets the corresponding RP4D bit.
- rEFInd EFI driver documentation:
  <https://www.rodsbooks.com/refind/drivers.html>

Only the source and generated RP04 utility binaries are included here; no HP
firmware image or extracted proprietary firmware module is redistributed.
