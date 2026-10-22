# Instructions for AI agents

## Scope

This repository documents a tested workaround for one HP EliteBook 8560p:
system/board ID `1618`, BIOS `68SCF F.67`, Intel QM67. The verified RP04
disable register is `0xFED1F418`, with RP4D at bit 19 (`0x00080000`), and
RP04 is PCI `00:1c.3`. Treat those values as specific to this configuration.
Do not assume another 8560p firmware revision or related model is identical.

The project does not modify firmware, bypass signature verification, remove
the POST wireless warning, or run code before firmware POST.

## Evidence standards

- Distinguish direct observations, static-analysis results, and hypotheses.
- Do not infer hardware behavior from strings, model names, or a single
  register write alone. Correlate the code path, register/bit layout, ACPI
  declarations, PCI topology, and runtime behavior where available.
- Preserve firmware provenance: model, board ID, BIOS version, image source,
  image hash, module GUID, and extraction path. Check that a package's selected
  image actually matches the target board.
- Keep original firmware images immutable. Do not add vendor firmware images
  or extracted proprietary modules to this repository.
- State what is unknown. In particular, the RP04 finding does not establish
  the physical effect on slot power/clock, and does not establish WWAN wiring
  or control behavior.

## Analysis workflow

1. Inventory the target system and select the matching firmware image.
2. Disassemble ACPI AML with ACPICA `iasl`; inspect the root-port device,
   operation regions, and bit fields.
3. Extract firmware volumes and PE/COFF sections from a copy of the image.
   Record FFS GUIDs and paths for candidate modules.
4. Use strings and PE metadata only for triage. In Ghidra, follow references
   and control flow from relevant firmware checks through to actual writes or
   protocol calls. Search for other accesses to the same controls.
5. Reconcile ACPI bit offsets with the absolute address and access width used
   by firmware. Document the calculation.
6. Prefer read-only runtime observation. Any hardware write requires a
   verified control meaning, preservation of unrelated bits, readback, and a
   practical recovery path. Do not add experimental register writes or
   firmware-patching instructions without explicit authorization and a
   hardware-specific safety review.
7. Describe exact test conditions and outcomes. A missing PCI enumeration
   does not by itself prove a defective card or absent physical bus signals.

Tools used in this investigation include `iasl`, `uefi_firmware`, Ghidra,
`objdump`, `strings`, and `lspci`. Analyze only firmware and devices the user
is authorized to inspect.

## Changes to this repository

- Keep changes limited to the RP04 workaround, its evidence, and its
  documented use.
- Rebuild EFI binaries from source when changing code; verify the PE subsystem
  is EFI application (`10`) or EFI boot-service driver (`11`) as intended.
- Preserve the guarded enable behavior: enable RP04, check for an active PCIe
  link with bounded retries, and disable RP04 again if a link cannot be
  confirmed. Do not silently change the target hardware, timeout, or failure
  policy.
- Do not claim that a link check proves a card is supported by the OS or that
  the BIOS warning has been removed.
