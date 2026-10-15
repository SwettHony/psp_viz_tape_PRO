# Changelog

## 1.1 — the icon makes itself; simpler install

- **No more icon step.** The kernel plugin now creates the cassette icon on
  the PSP: on first start it reads the stock `visualizer_plugin.rco` from
  flash0 (read-only), dry-runs the built-in patch to confirm the exact result,
  and only then writes `seplugins/viz_tape/visualizer.rco`. A different or
  damaged stock file never touches the card; partial files are removed.
- **Copy-and-go release.** The ZIP now holds a `seplugins/viz_tape` folder
  (just the two plugins) to copy onto the memory card, with plain-language
  instructions.
- **No `enabled.txt`.** Like any ARK plugin, Viz Tape is now switched on and
  off only by its PLUGINS.TXT lines. The development-era one-boot test ticket
  is gone too; the boot guard now protects every start.
- **Shorter `status.txt`.** It is now a brief startup summary — one line each
  for the start, the icon, fast-forward detection and the cassette hook (or why
  something was skipped). The periodic performance lines are gone; it is
  rewritten at each start and stays under 1 KB.

## 1.0 — first public release

Everything below was developed and tested incrementally on real hardware
(PSP-1000, ARK-4, 6.61, 128 GB card); this is the first version published.

- **Animated cassette** replacing the Music player's last visualizer: red
  Side A and blue Side B, 15 minutes of playback per side, tape winding
  between reels behind the window, and a rounded perspective flip between
  sides. The cassette slides in and latches when the visualizer opens.
- **Fast-forward and rewind** read the visualizer's own playback state: the
  tape winds 12× faster with the reels at 3× (kept below the speed where six
  teeth would appear to spin backwards), and rewind runs it backwards.
- **Cassette selector icon**, shipped as a patch applied to the user's own
  stock resource; the kernel checks the result's size and checksum before
  using it.
- **Efficient rendering**: only the reel strip is redrawn while playing,
  unchanged frames are reused, and the rest comes from a cache — about
  1.9 ms per frame at ~60 fps on a PSP-1000 (down from ~16.6 ms at ~30 fps).
- **Smoothed square reel teeth**, evenly spaced, with smoothed ring edges.
- **Persistent activation** via `enabled.txt`, plus a **boot guard**: a fault
  within 15 seconds of an XMB start skips the plugin on the next start only,
  so it can never cause a boot loop.
- **Fails inert**: every RAM change first verifies the exact firmware code it
  touches; a mismatch is logged to `status.txt` and skipped. No flash writes.
- Host test suite under AddressSanitizer/UBSan, a binary import audit and
  reproducible builds.
