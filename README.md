# Viz Tape — cassette visualizer for the PSP Music player

![Viz Tape: the cassette slides in, winds and flips from Side A to Side B](docs/viz_tape.gif)

I wanted the PSP's Music player to look like a tape deck, so this plugin
turns its last visualizer into an animated cassette: the reels turn, the tape
winds from one reel to the other, and after 15 minutes it flips to Side B.

Working beautifully on my PSP-1000 with ARK-4. Use at your own risk of course.

This was written by Astra and Opus5.5 if you do not want code written by AI on your PSP
this project is not for you.

## What it does

- **A cassette that plays along.** Red Side A and blue Side B, 15 minutes of
  playback per side. The tape winds from the left reel to the right, visible
  through the window, and the packs grow and shrink as it goes.
- **Flips.** At the end of a side the cassette lifts, turns over in
  perspective and settles on the other side, repeating A → B → A.
- **Fast-forward and rewind.** Seeking in a song winds the tape 12× faster
  with the reels spinning fast; rewind runs it backwards, flips included.
- **Pause-aware.** Pausing, hiding the visualizer or putting the PSP to sleep
  stops the tape. Opening the visualizer slides the cassette in and latches it.
- **Cassette icon** in the visualizer selector, made automatically on the
  PSP the first time it starts.
- **Light on the CPU.** Only the reel strip is redrawn while playing, and
  frames where nothing moved are reused: about 1.9 ms per frame at ~60 fps on
  a PSP-1000.
- Sony keeps drawing the track title, time and controls on top.

## Install

1. Download `viz_tape-v1.1.zip` from the
   [latest release](https://github.com/beckerd/psp_viz_tape/releases/latest)
   and unzip it.
2. Connect your PSP to your computer in USB mode.
3. Copy the `seplugins` folder from the ZIP onto the root of the memory card.
   If the card already has a `seplugins` (or `SEPLUGINS`) folder, let your
   computer merge them; nothing else in it is replaced.
4. Open `seplugins/PLUGINS.TXT` on the card in a plain-text editor (create it
   if it doesn't exist) and add these two lines:

   ```
   vsh, ms0:/seplugins/viz_tape/viz_tape_render.prx, on
   vsh, ms0:/seplugins/viz_tape/viz_tape.prx, on
   ```

5. Eject the card, exit USB mode and restart your PSP.
6. Play a song in the Music player and switch to the last visualizer.

That's it. On the first start the plugin creates the cassette icon during
startup, ready for the first Music session. [INSTALL.txt](INSTALL.txt)
in the ZIP has the same steps, plus how to update and uninstall.

**Updating from 1.0:** turn both lines `off`, copy the new `seplugins` folder
over the old one, then turn them back `on`. The old `enabled.txt` is no
longer used and can be deleted.

## Tested on

- PSP-1000 (32 MB), ARK-4, 6.61, 128 GB memory card, alongside xmb_gamecache.

Every feature above ran on that PSP with zero renderer errors. That is the
honest extent of hardware testing. The plugin checks the exact firmware code
it patches and goes inert on anything else, but other models, firmware
versions and PSP Go internal storage are **unverified**. Reports (good and
bad) are very welcome — please open an issue.

## If something goes wrong

- **Boot guard.** For the first 15 seconds after each XMB start, the plugin
  keeps `boot-guard.txt` on the card. If a start crashes in that window, the
  next start skips the plugin once, so a fault can never become a boot loop.
  (Launching a game or entering USB mode within 15 seconds has the same
  harmless one-start effect.)
- To disable it, change both Viz Tape lines in PLUGINS.TXT to `off` over USB,
  or hold **START** while powering on to bypass ARK plugins for one boot, or
  use ARK's **Recovery Menu** (hold R at power-on). See ARK's
  [recovery guide](https://github.com/PSP-Archive/ARK-4/wiki/Recovery).
- Diagnostics are written to `ms0:/seplugins/viz_tape/status.txt`:
  `PATCHED final brown-line slot 7` means the cassette is hooked,
  `ICON created visualizer.rco from the stock resource` means the icon was
  made, `ICON cassette resource selected in RAM` means it is in use, and
  `SEEK fast-forward/rewind state found` means seeking is detected.
- If the cassette doesn't appear, check both PLUGINS.TXT lines and their
  order (renderer first).

## Known limitations

- 6.61 firmware layout only; on anything else the plugin stays inert.
- Only the last visualizer is replaced; the spectrum and others are untouched.
- The tape clock restarts when the XMB restarts, and is not tied to the
  position in the song.
- Don't remove the memory card while the Music player is open: the player
  reads the cassette icon's `visualizer.rco` from the card.
- The icon is only made on the supported 6.61 firmware. On anything else the
  stock icon stays (the cassette visualizer is skipped there anyway).

## Building from source

Requires the [pspdev toolchain](https://github.com/pspdev/pspdev) and Python 3:

```
export PSPDEV=~/pspdev
export PATH=$PSPDEV/bin:$PATH
make
```

`make` builds both PRXs, audits their imports and writes `SHA256SUMS`.
`make release` builds the release ZIP.

`make host-check` runs the host tests (rendering, animation, mesh, frame cache,
address checks, startup and USB lifecycle, boot guard and the on-PSP icon
builder) under AddressSanitizer and UndefinedBehaviorSanitizer.
`make profile-check` and `make icon-check` also verify the firmware signatures,
but need private firmware captures that are not in this repository.

## How it works (short version)

The kernel plugin registers an ARK module-start handler. When the Music
player's visualizer module loads, it verifies the module's exact layout —
sizes, the final effect's 15-entry vtable, code signatures and import stubs —
and only then replaces that effect's one draw pointer in user RAM with the
renderer's. It also locates the visualizer's playback-state word (play,
pause, fast-forward, rewind) for the renderer to read.

The user-mode renderer draws the cassette in software into a fixed 480×272
buffer, run-length-merges it into rectangles and submits them through the
same PAF drawing functions the native effect uses, with no per-frame
allocation. While the tape plays, only the reel strip is redrawn and the rest
comes from a cache.

For the icon, the plugin carries a small patch: the new cassette image and
the resource header that references it. On first start it reads the PSP's
own `flash0:/vsh/resource/visualizer_plugin.rco` (read-only), checks that
applying the patch gives exactly the expected result, and only then writes
`ms0:/seplugins/viz_tape/visualizer.rco`. A second guarded change then points
the Music player at that file (in RAM) after checking its exact size and
checksum again on each Music-module load, including after USB mode. A missing
or invalid file leaves the stock icon selected. Only the last visualizer's
thumbnail differs. Nothing is ever written to flash.



<img width="440" height="496" alt="Screenshot 2026-10-14 at 10 45 55 PM" src="https://github.com/user-attachments/assets/ac097019-8985-4320-aab3-720a413feb3b" />


## Disclaimer

This software is provided **as is, without warranty of any kind**, and the
author accepts no liability for any damage arising from its use — see
sections 15 and 16 of the [LICENSE](LICENSE). Use it at your own risk.

Just in case: an XMB freeze is recovered by holding the power switch off and
disabling the plugin via ARK's Recovery Menu (hold R at power-on), by holding
START at power-on, or by editing PLUGINS.TXT over USB.

## Credits & license

GPL-3.0-or-later. Built with PSPSDK and ARK-4's module-start API. The RCO
work used [RCOMage's format definitions](https://github.com/kakaroto/RCOMage)
and [Jpcsp's LZR decoder](https://github.com/jpcsp/jpcsp) — thanks to both
projects.

The cassette artwork and code were made for this project. The icon patch
contains the new cassette image and the rebuilt 868-byte resource header that
references it; every stock image is copied from your own PSP's file, and
Sony's resource itself is not distributed.
