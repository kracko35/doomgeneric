# NHDT-W58G emulator port (silent title and attract demo)

This port executes Doom's C engine as **big-endian SH-4A guest code** in the
existing custom NHDT QEMU machine. The guest reads a shareware WAD from RAM,
renders 320×200 pixels, and transfers RGB565 frames to the emulated 416×240
display. The host loads the initial executable and WAD; it does not render or
inject game frames. Time comes from the emulated SH TMU at 12.5 MHz.

This target has no sound, player input, saves, filesystem writes, firmware API
calls, or attached HDD. It boots directly into RAM through QEMU's debugger.
It does not establish that the application can start on a physical navigation
unit or predict physical-device performance.

## Build

Requirements: Python 3.8+, GNU GCC 13 cross compiler for SH, its binutils, and
the existing project-specific `qemu-system-sh4eb` with the `nhdt` machine and
read-only boot bundle. A stock QEMU lacks that machine.

The original engine version is upstream commit
`e257a09527062bfb986b6e5e0780839b6cbd5b71`. All game logic remains in the
`doomgeneric/` sources. The one upstream fix corrects a misspelled big-endian
RGB565 swap helper. The port is isolated in `navi/`.

```sh
export NAVI_SH_TOOLROOT=/path/to/motion-toolchain/root
python3 navi/fetch_wad.py
python3 navi/build.py
python3 navi/run_qemu.py \
  --qemu /path/to/qemu-system-sh4eb \
  --boot-dir /path/to/boot \
  --wad navi/assets/doom1.wad \
  --output navi/evidence
```

If the compiler is in the existing development-PC layout, `build.py` finds it
automatically in the neighboring `hello-navi-20261103/motion-toolchain/root`.
Build artifacts, game data, and local evidence are excluded from Git. The WAD
fetcher accepts only the pinned DOOM 1.9 shareware file; it is not included in
this repository.

Add `--vnc 127.0.0.1:7 --keep-running` to leave the verified guest visible through
a local VNC connection. Forward ports when the guest runs on another computer.
The runner starts a separate guest and never connects to an existing guest.

## Verification

`verification.json` records the binary, WAD, and emulator hashes, title capture,
demo captures, game-tic and frame counters, and success/failure. A passing run
requires a title state followed by advancing automatic-demo game tics and
changing screenshots for at least 350 game tics (10 seconds). Use
`--min-demo-tics 1050` for 30 seconds of demo playback.

Memory map: WAD `0x8b000000`, code/globals `0x8c000000`, 32 MiB outer heap
`0x8d000000–0x8effffff`, stack `0x8ffe0000`. Doom's zone allocator reuses its
own memory; the small outer allocator retains allocations until guest reset.

The runtime implements the subset of C I/O needed by this target. Only
`doom1.wad` can be opened, read-only; configuration and save writes fail. SH's
internal 32-bit division helpers use a narrower clobber contract than ordinary
C calls, so `division.S` preserves the surrounding registers. A little-endian
Linux libgcc must not be linked into this guest.
