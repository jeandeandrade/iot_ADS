from SCons.Script import Import

Import("env")

import os
from pathlib import Path


def generate_qemu_flash(source, target, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    bootloader = build_dir / "bootloader.bin"
    partition_table = build_dir / "partitions.bin"
    firmware = build_dir / "firmware.bin"
    output = build_dir / "qemu_flash.bin"

    FLASH_SIZE = 4 * 1024 * 1024
    image = bytearray(b"\xff" * FLASH_SIZE)

    layout = [
        (0x1000, bootloader),
        (0x8000, partition_table),
        (0x10000, firmware),
    ]

    for offset, path in layout:
        if not path.exists():
            print(f"[QEMU] Binário não encontrado: {path}")
            continue
        with open(path, "rb") as f:
            data = f.read()
            image[offset : offset + len(data)] = data
            print(f"[QEMU] Inserido {path.name} em 0x{offset:X}")

    with open(output, "wb") as f:
        f.write(image)
        print(f"[QEMU] Imagem gerada: {output}")


# Executa após a geração do firmware.bin
env.AddPostAction("$BUILD_DIR/firmware.bin", generate_qemu_flash)
