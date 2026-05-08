#!/bin/sh
# Create a QEMU-compatible BSP by rebuilding without hardware-specific drivers.
# Usage: make-qemu-bsp.sh <build-dir>
#
# This builds a BSP without genet.device, pci-bcm2711.hidd, and pcixhci.device
# which crash on QEMU due to accessing non-emulated hardware registers.

BUILD=${1:-$(dirname $0)/../../../build-fresh}
BSP_FULL="$BUILD/bin/raspi-aarch64-smp/AROS/aros-aarch64-bsp.rom"
BSP_QEMU="$BUILD/bin/raspi-aarch64-smp/AROS/aros-aarch64-bsp-qemu.rom"

cd "$BUILD"
# Build BSP without the problematic targets
make kernel-package-raspi-aarch64 \
  PKG_DEVS="input gameport keyboard console sdcard USBHardware/usb2otg" \
  PKG_HIDDS="gfx inputclass mouse keyboard hiddclass vc4gfx" \
  AARCH64_BSP=aros-aarch64-bsp-qemu.rom

echo "QEMU BSP: $BSP_QEMU"
