# How the RP04 finding was established

This note records the evidence chain behind the RP04 workaround and a
repeatable, cautious workflow for investigating similar firmware behavior.
The result here is specific to one HP EliteBook 8560p (system/board ID 1618,
BIOS `68SCF F.67`, Intel QM67 chipset). It is not evidence that another BIOS
revision or related laptop uses the same register or policy.

## Evidence chain

The conclusion that the firmware disables RP04 was built by correlating
independent static and runtime evidence:

1. **Identify the exact firmware.** Match the machine's reported product,
   board/system ID, and BIOS version to the corresponding image in the
   vendor's update package. A package can contain multiple ROM variants;
   analyzing the wrong one can produce convincing but irrelevant results.
2. **Inspect the ACPI DSDT.** Disassemble the supplied AML table with ACPICA's
   `iasl -d`. The DSDT describes the chipset's Root Complex Register Block
   (RCRB) at `0xFED1C000`, the RP04 PCIe root port at `00:1c.3`, and the
   root-port disable fields. The RP4D field is bit 3 of the byte at RCRB
   offset `0x341A`.
3. **Translate the ACPI field to the firmware's dword access.** The byte at
   `0xFED1C000 + 0x341A` is two bytes into the dword beginning at
   `0xFED1C000 + 0x3418`. Its bit 3 is therefore bit 19 of that dword:
   address `0xFED1F418`, mask `0x00080000`.
4. **Find the firmware write.** Extract PE/COFF modules from the matching
   firmware image, inspect strings for wireless failure messages and error
   identifiers, then trace references and code paths in Ghidra. The WLAN
   failure path writes the RP4D mask to the register above. Strings alone are
   only leads; the address and bit need to be confirmed from code references
   and data flow.
5. **Check actual hardware behavior.** On this 8560p, the disable state
   corresponds to RP04 (`00:1c.3`) disappearing from PCI enumeration. Clearing
   RP4D allowed the installed WLAN endpoint to enumerate. Re-enabling the
   port with the guarded EFI utility retained it with an active PCIe link,
   while the utility disabled it again with the WLAN slot empty. A
   non-approved Broadcom card and a Ralink RT3290 were also observed
   enumerating after re-enabling; the RT3290 Wi-Fi connection worked.

The DSDT-to-register mapping, firmware write, and runtime observations agree.
That is substantially stronger evidence than finding a plausible register
address or a `Wireless Module not supported` string by itself. It establishes
the RP04 disable behavior, not whether the bit removes slot power or reference
clock, and not whether the BIOS warning can be removed.

## Reproducing the analysis

Use a firmware image and ACPI tables that you are authorized to analyze.
Keep original images unchanged and record hashes, versions, and extraction
provenance. Tool versions and output can vary, so treat output as evidence to
review, not as an automatic conclusion.

1. **Inventory before analysis.** Record the system model, board ID, BIOS
   revision, chipset, and the exact update-package image selected for that
   board ID. Compare the image version/date with the machine's reported
   firmware.
2. **Disassemble ACPI tables.** Run ACPICA `iasl -d DSDT.aml`, then search the
   generated ASL for the relevant PCI root-port device, `OperationRegion`
   declarations, and `Field` definitions. Keep ACPI bit offsets distinct
   from absolute MMIO addresses and from the dword layout used by firmware.
3. **Extract firmware modules.** Use a UEFI firmware-volume parser such as
   `uefi_firmware` to inventory/extract FFS files and PE32/TE sections from a
   copy of the ROM. Preserve the module GUID and section path alongside each
   extracted executable so later analysis can be tied back to its source.
4. **Triage, then trace.** Use `file`, `strings`, and PE/COFF inspection tools
   to identify likely modules and useful diagnostics. Import promising
   executable sections into Ghidra, identify entry points and protocol
   callbacks, and follow the failure path to the actual hardware write.
   Search all extracted modules for additional accesses to the same register
   before assigning meaning to it.
5. **Correlate, do not infer from names.** Check the register, bit position,
   PCI address, and ACPI declaration against one another. Determine whether a
   write is conditional, what code path reaches it, whether other bits are
   preserved, and whether the write is read back.
6. **Validate read-only first.** Observe PCI topology and link state before
   considering a write. If controlled hardware testing is appropriate, change
   only a verified bit, preserve unrelated register bits, read back the value,
   and have a recovery path. Compare occupied and empty-slot states. Do not
   use an endpoint's lack of enumeration as proof that its card is defective
   or that a given bus is physically absent.
7. **Report confidence and scope.** Separate direct observations from
   inference. State the exact machine/firmware tested, what the evidence
   demonstrates, and what remains unknown. Do not generalize a chipset
   register mapping to another model without independently confirming it.

Useful analysis tools for this case were `iasl`, `uefi_firmware`, Ghidra,
`objdump`, `strings`, and Linux `lspci`. This list describes the tools used;
the workflow does not require running firmware images or modifying the BIOS.

## What this investigation did not establish

- The whitelist's complete allowed-device table was not the basis for the
  RP04 register identification.
- The exact physical power/clock effect of RP4D was not measured.
- The WWAN socket's USB port mapping and its failure-control mechanism remain
  separate, unresolved questions; the RP04 result must not be applied to it.
- No BIOS image was patched, no signature enforcement was bypassed, and no
  pre-POST code injection was attempted.

No HP firmware image or extracted proprietary firmware module is included in
this repository.
