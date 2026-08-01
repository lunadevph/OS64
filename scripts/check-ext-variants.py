#!/usr/bin/env python3
"""Create every OS64 native filesystem variant and validate it with e2fsprogs."""
import pathlib
import subprocess
import sys
import tempfile

import pexpect

ROOT = pathlib.Path(__file__).resolve().parents[1]
PARTITION_START = 131072
PARTITION_SECTORS = 131072


def create_variant(kind: str, directory: pathlib.Path) -> pathlib.Path:
    disk = directory / f"disk-{kind}.img"
    with disk.open("wb") as stream:
        stream.truncate(128 * 1024 * 1024)
    command = (
        "qemu-system-x86_64 -cpu max -m 128M "
        f"-drive file={disk},format=raw,if=ide -nic none "
        f"-boot order=d -cdrom {ROOT / 'build/images/os64.iso'} "
        "-nographic -monitor none"
    )
    guest = pexpect.spawn("/bin/sh", ["-c", command], encoding="latin1", timeout=40)
    try:
        guest.sendline("")
        guest.expect(r"root.*?# ")
        guest.sendline(f"format /dev/sda {kind}")
        guest.expect(rf"/dev/sda2 {kind} on /var")
        guest.expect(r"root.*?# ", timeout=45)
        guest.sendline("sync")
        guest.expect(r"root.*?# ")
    finally:
        guest.close(force=True)
    image = directory / f"{kind}.img"
    with disk.open("rb") as source, image.open("wb") as target:
        source.seek(PARTITION_START * 512)
        target.write(source.read(PARTITION_SECTORS * 512))
    return image


def validate(kind: str, image: pathlib.Path) -> bool:
    detected = subprocess.run(
        ["blkid", "-p", "-s", "TYPE", "-o", "value", str(image)],
        text=True, capture_output=True, check=False
    ).stdout.strip()
    checked = subprocess.run(
        ["e2fsck", "-fn", str(image)], text=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False
    )
    ok = detected == kind and checked.returncode == 0
    print(f"{'PASS' if ok else 'FAIL'} {kind}: blkid={detected or 'unknown'} e2fsck={checked.returncode}")
    if not ok:
        print(checked.stdout)
    return ok


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="os64-ext-") as temporary:
        directory = pathlib.Path(temporary)
        return 0 if all(validate(kind, create_variant(kind, directory))
                        for kind in ("ext2", "ext3", "ext4")) else 1


if __name__ == "__main__":
    sys.exit(main())
