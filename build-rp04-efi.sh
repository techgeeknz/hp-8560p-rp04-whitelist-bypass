#!/usr/bin/env bash
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")" && pwd)
prefix=${GNU_EFI_PREFIX:-/usr}
include_dir="$prefix/include/efi"
lib_dir="$prefix/lib"

if [[ ! -f "$include_dir/efi.h" || ! -f "$lib_dir/elf_x86_64_efi.lds" ]]; then
  printf 'GNU-EFI development files not found under %s\n' "$prefix" >&2
  exit 1
fi

cd "$root"
for target in \
  rp04-status:10 \
  rp04-disable:10 \
  rp04-enable:10 \
  rp04-enable-driver:11; do
  name=${target%:*}
  subsystem=${target#*:}

  gcc \
    -I"$include_dir" \
    -I"$include_dir/x86_64" \
    -fpic \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fshort-wchar \
    -mno-red-zone \
    -maccumulate-outgoing-args \
    -DEFI_FUNCTION_WRAPPER \
    -c "$name.c" \
    -o "$name.o"

  ld \
    -nostdlib \
    -znocombreloc \
    -T "$lib_dir/elf_x86_64_efi.lds" \
    -shared \
    -Bsymbolic \
    "$lib_dir/crt0-efi-x86_64.o" \
    "$name.o" \
    -L"$lib_dir" \
    -lefi \
    -lgnuefi \
    -o "$name.so"

  objcopy \
    -O pei-x86-64 \
    -j .text \
    -j .rodata \
    -j .sdata \
    -j .data \
    -j .dynamic \
    -j .dynsym \
    -j .rel \
    -j .rela \
    -j .reloc \
    --subsystem="$subsystem" \
    "$name.so" \
    "$name.efi"

  rm -f "$name.o" "$name.so"
done

printf 'Built RP04 status, disable, enable, and enable-driver EFI images\n'
