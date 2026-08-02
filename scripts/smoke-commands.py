#!/usr/bin/env python3
"""Boot OS64 in QEMU and invoke every command trampoline at least once."""
import pathlib, re, shutil, subprocess, sys, tempfile
import pexpect

ROOT = pathlib.Path(__file__).resolve().parents[1]
conf = (ROOT / "user/commands.conf").read_text()
installed = []
for line in conf.splitlines():
    if line.startswith(("bin=", "sbin=")):
        installed += line.split("=", 1)[1].split()

cases = {
 "basename":"basename /tmp/sample", "browser":"browser --help", "cat":"cat /proc/mounts", "cd":"cd /tmp", "clear":"clear",
 "cp":"cp /etc/hostname /tmp/host.copy", "curl":"curl --help", "date":"date", "dd":"dd --help",
 "dirname":"dirname /tmp/sample", "diskinfo":"diskinfo", "display":"display status", "dmesg":"dmesg",
 "echo":"echo command-smoke", "fill":"fill --help", "find":"find /proc", "free":"free -h", "groups":"groups",
 "pm":"pm list",
 "head":"head -n 1 /etc/hostname", "hexdump":"hexdump /etc/hostname", "help":"help", "history":"history",
 "host":"host --help", "id":"id", "ifconfig":"ifconfig", "install-apps":"install-apps --help", "ip":"ip route",
 "login":"login --help", "logout":"logout --help", "ls":"ls /etc", "logd":"logd status", "lspci":"lspci", "mkdir":"mkdir --help",
 "mkuser":"mkuser --help", "mv":"mv /tmp/host.copy /tmp/host.moved", "nano":"nano --help", "nslookup":"nslookup --help",
 "ntp":"ntp status", "ofp":"ofp", "passwd":"passwd --help", "ping":"ping --help", "ps":"ps", "pwd":"pwd",
 "rm":"rm /tmp/host.moved", "route":"route", "sh":"sh", "stat":"stat /proc/version", "status":"status",
 "su":"su --help", "sudo":"sudo --help", "sync":"sync", "tail":"tail -n 1 /etc/hostname", "time":"time",
 "touch":"touch /tmp/touched", "truncate":"truncate -s 8 /tmp/touched", "umask":"umask", "uname":"uname -a",
 "wc":"wc -c /etc/hostname", "who":"who", "whoami":"whoami", "xxd":"xxd /etc/hostname",
 "acpid":"acpid status", "chkfs":"chkfs -a", "chmod":"chmod 0600 /tmp/touched", "chgrp":"chgrp --help",
 "chown":"chown --help", "diskd":"diskd status", "displayd":"displayd status", "format":"format /dev/sda",
 "fsd":"fsd status", "graphicsd":"graphicsd status", "halt":"halt --help", "init":"init", "install":"install --help",
 "memoryd":"memoryd status", "mount":"mount", "netd":"netd status", "nologin":"nologin", "reboot":"reboot --help",
 "shutdown":"shutdown --help", "timed":"timed status", "useradd":"useradd --help", "userdel":"userdel --help",
 "userd":"userd status", "usermod":"usermod --help"
}
missing = sorted(set(installed) - set(cases))
extra = sorted(set(cases) - set(installed))
if missing or extra:
    print("matrix mismatch", "missing=", missing, "extra=", extra, file=sys.stderr); sys.exit(2)

with tempfile.TemporaryDirectory(prefix="os64-smoke-") as td:
    disk = pathlib.Path(td) / "disk.img"
    with disk.open("wb") as f: f.truncate(128 * 1024 * 1024)
    cmd = ("qemu-system-x86_64 -cpu max -m 128M "
           f"-drive file={disk},format=raw,if=ide -nic user,model=rtl8139 "
           f"-boot order=d -cdrom {ROOT/'build/images/os64.iso'} -nographic -monitor none")
    child = pexpect.spawn("/bin/sh", ["-c", cmd], encoding="latin1", timeout=35)
    child.sendline("")
    child.expect(r"root.*?# ")
    # Use an isolated disk and prepare FAT32 plus the default ext4 before the matrix.
    child.sendline("format /dev/sda")
    child.expect(r"root.*?# ", timeout=25)
    failures = []
    results = []
    for name in installed:
        command = cases[name]
        child.sendline(command)
        try:
            child.expect(r"root.*?# ", timeout=20)
            output = child.before
            crashed = "OS64 Crashed" in output or "System halted" in output
            child.sendline("status")
            child.expect(r"Last program exit status: ([0-9]+)", timeout=5)
            status = int(child.match.group(1))
            child.expect(r"root.*?# ", timeout=5)
            failed = crashed or status != 0
            results.append((name, "FAIL" if failed else "PASS", status))
            if failed: failures.append(name)
        except (pexpect.TIMEOUT, pexpect.EOF):
            results.append((name, "FAIL", -1)); failures.append(name); break
    # Exercise indirect ext blocks and the package manager as real lifecycles.
    for label, command in (
        ("ext-indirect-write", "dd if=/dev/zero of=/var/tmp/indirect.bin bs=4096 count=16"),
        ("ext-indirect-copy", "cp /var/tmp/indirect.bin /var/tmp/indirect-copy.bin"),
        ("ext-indirect-stat", "stat /var/tmp/indirect-copy.bin"),
        ("ext-indirect-remove", "rm /var/tmp/indirect.bin"),
        ("ext-indirect-clean", "rm /var/tmp/indirect-copy.bin"),
        ("pm-update", "pm update"),
        ("pm-install", "pm install hello"),
        ("pm-execute", "hello smoke-test"),
        ("pm-status", "pm status"),
        ("pm-remove", "pm remove hello"),
    ):
        child.sendline(command)
        try:
            child.expect(r"root.*?# ", timeout=15)
            output = child.before
            child.sendline("status")
            child.expect(r"Last program exit status: ([0-9]+)", timeout=5)
            status = int(child.match.group(1))
            child.expect(r"root.*?# ", timeout=5)
            failed = status != 0 or "OS64 Crashed" in output
            print(f"{'FAIL' if failed else 'PASS':4} {label:16} status={status}")
            if failed: failures.append(label)
        except (pexpect.TIMEOUT, pexpect.EOF):
            failures.append(label)
            print(f"FAIL {label:16} status=-1")
            break
    child.close(force=True)
    # Validate both on-disk formats with independent host implementations.
    for label, start, count, checker in (
        ("fat32", 2048, 129024, ["fsck.fat", "-n"]),
        ("ext4", 131072, 131072, ["e2fsck", "-fn"]),
    ):
        image = pathlib.Path(td) / f"{label}.img"
        with disk.open("rb") as src, image.open("wb") as dst:
            src.seek(start * 512); dst.write(src.read(count * 512))
        checked = subprocess.run(checker + [str(image)], stdout=subprocess.DEVNULL,
                                 stderr=subprocess.STDOUT).returncode
        if checked != 0: failures.append(label + "-host-check")
        print(f"{'PASS' if checked == 0 else 'FAIL':4} {label:16} host-check")
    for name, result, status in results: print(f"{result:4} {name:16} status={status}")
    print(f"invoked={len(results)} installed={len(installed)} failures={len(failures)}")
    sys.exit(1 if failures or len(results) != len(installed) else 0)
