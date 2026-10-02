#!/bin/bash
# Regenera web/firmware/tamapoke.bin (firmware combinado) para el instalador web.
# Uso: bash tools/build_web.sh
set -e
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"

echo "Compilando..."
arduino-cli compile --fqbn "$FQBN" --export-binaries .

B=build/esp32.esp32.esp32s3
echo "Preparando partes para el instalador web..."
# IMPORTANTE: no usamos un merged binary unico. Un merged binary empezando en
# 0x0 obliga al flasher a borrar sectores hasta el final de la aplicacion,
# incluyendo NVS (0x9000...), aunque el usuario NO marque "Erase device".
# Las partes separadas permiten actualizar bootloader/tabla/app sin tocar NVS.
mkdir -p web/firmware
cp "$B/TamaPoke.ino.bootloader.bin" web/firmware/bootloader.bin
cp "$B/TamaPoke.ino.partitions.bin" web/firmware/partitions.bin
cp "$B/boot_app0.bin" web/firmware/boot_app0.bin
cp "$B/TamaPoke.ino.bin" web/firmware/firmware.bin

echo "OK -> web/firmware/bootloader.bin"
echo "OK -> web/firmware/partitions.bin"
echo "OK -> web/firmware/boot_app0.bin"
echo "OK -> web/firmware/firmware.bin"

echo "Empaquetando sprites..."
python3 tools/pack_bundle.py
