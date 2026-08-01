#!/usr/bin/env python3
"""Generate the README screenshots from a real OS64 QEMU session."""
import pathlib
import socket
import sys
import tempfile
import time

import pexpect

ROOT = pathlib.Path(__file__).resolve().parents[1]
IMAGES = ROOT / "docs" / "screenshots"
PROMPT = r"root.*?# "


def monitor_command(monitor: pathlib.Path, command: str) -> None:
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(5)
        connection.connect(str(monitor))
        connection.sendall((command + "\n").encode("ascii"))
        time.sleep(0.2)


def capture(monitor: pathlib.Path, name: str) -> None:
    destination = (IMAGES / name).resolve()
    if destination.exists():
        destination.unlink()
    monitor_command(monitor, f"screendump {destination} -f png")
    for _ in range(50):
        if destination.exists() and destination.stat().st_size > 8:
            signature = destination.read_bytes()[:8]
            if signature == b"\x89PNG\r\n\x1a\n":
                print(f"  CAPTURE {destination.relative_to(ROOT)}")
                return
        time.sleep(0.1)
    raise RuntimeError(f"QEMU did not create {destination}")


def command(guest: pexpect.spawn, text: str, timeout: int = 15) -> None:
    guest.sendline(text)
    guest.expect(PROMPT, timeout=timeout)


def drain_serial(guest: pexpect.spawn, seconds: float) -> None:
    """Keep QEMU's blocking COM1 mirror flowing while a TUI frame renders."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            guest.read_nonblocking(size=65536, timeout=0.2)
        except pexpect.TIMEOUT:
            pass


def main() -> int:
    iso = ROOT / "build" / "images" / "os64.iso"
    disk = ROOT / "build" / "images" / "os64-disk.img"
    if not iso.exists() or not disk.exists():
        print("generate-screenshots: build the ISO and disk first", file=sys.stderr)
        return 2
    IMAGES.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="os64-screenshots-") as temporary:
        monitor = pathlib.Path(temporary) / "monitor.sock"
        arguments = [
            "-cpu", "max", "-m", "128M",
            "-drive", f"file={disk},format=raw,if=ide",
            "-nic", "user,model=rtl8139",
            "-boot", "order=d", "-cdrom", str(iso),
            "-display", "none", "-serial", "stdio",
            "-monitor", f"unix:{monitor},server=on,wait=off",
        ]
        guest = pexpect.spawn("qemu-system-x86_64", arguments,
                              cwd=str(ROOT), encoding="latin1", timeout=30)
        try:
            guest.expect(PROMPT)
            capture(monitor, "os64-boot.png")

            command(guest, "display mode 640x480")
            command(guest, "clear")
            command(guest, "help")
            capture(monitor, "os64-help.png")

            command(guest, "clear")
            command(guest, "free -h")
            command(guest, "ps -e")
            capture(monitor, "os64-system-info.png")

            command(guest, "display mode 800x600")
            guest.sendline("sysmgr")
            # Serial mirrors every changed TUI cell as ANSI. Drain the PTY so
            # its finite buffer cannot stall QEMU halfway through the frame.
            drain_serial(guest, 5)
            capture(monitor, "os64-sysmgr.png")
        except (pexpect.EOF, pexpect.TIMEOUT, OSError, RuntimeError) as error:
            print(f"generate-screenshots: {error}", file=sys.stderr)
            return 1
        finally:
            try:
                monitor_command(monitor, "quit")
            except OSError:
                pass
            guest.close(force=True)

    expected = ["os64-boot.png", "os64-help.png",
                "os64-system-info.png", "os64-sysmgr.png"]
    missing = [name for name in expected if not (IMAGES / name).exists()]
    if missing:
        print("generate-screenshots: missing " + ", ".join(missing), file=sys.stderr)
        return 1
    print("Generated 4 OS64 screenshots.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
