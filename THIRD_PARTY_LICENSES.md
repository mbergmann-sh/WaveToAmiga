# Third-party components / Drittanbieter-Komponenten

WaveToAmiga itself is released under the **MIT License** (see `LICENSE`,
© Michael Bergmann 2026). It uses the following third-party software, which is
licensed separately:

## SoX / libsox (subset) – Amiga/AIFF format handlers
* Location: `third_party/sox/` – a subset of the SoX 14.4.2 sources (libsox core + format handlers).
* **Original SoX files, used unmodified:** `8svx.c` (IFF 8SVX), `aiff.c` / `aiff-fmt.c` (AIFF),
  `maud.c` (IFF MAUD), `raw.c`, `g711.c`, `id3.c`, `libsox.c`, `libsox_i.c`, `formats_i.c`, `util.c`,
  `xmalloc.c` and the headers `sox.h`, `sox_i.h`, `util.h`, … Copyright (c) Chris Bagwell, Rob Sykes,
  Guido van Rossum, W V Neisius and other SoX contributors (see `third_party/sox/AUTHORS`).
* **Modified SoX file:** `formats.c` – only one change, marked in the source: on Windows, `fopen()` is mapped to a
  UTF-8 aware wrapper (`_wfopen`) so that file names with non-ASCII characters can be written.
* **Replaced SoX files (WaveToAmiga versions):** `formats.h` (reduced handler list), `soxconfig.h`
  (fixed configuration instead of the output of `./configure`).
* **Added by WaveToAmiga (LGPL v2.1+, same license as libsox):** `svx_ext.c` (IFF 8SVX with Fibonacci-delta
  compression and IFF 16SV, written in the style of SoX' `8svx.c`) and `w2a_stubs.c`.
* License: GNU Lesser General Public License v2.1 or (at your option) later – see `third_party/sox/COPYING`
  and `third_party/sox/LICENSE.LGPL`. `maud.c` and `aiff.c` additionally carry the original permissive notice
  ("may be used for any purpose, this copyright notice must be maintained"), which is kept. `8svx.c` has no
  separate header and is covered by the license of libsox.
* Not used: the GPL-only parts of SoX (`sox.c`, effects, other formats) are **not** part of this project.
* WaveToAmiga links this code **statically**. Because the complete source code of WaveToAmiga is public (MIT),
  the program can be relinked with a modified version of these files (`qmake6 CONFIG+=release`), which satisfies
  the LGPL relinking requirement. If you redistribute a closed-source derivative, provide the object files
  or link libsox dynamically.

## Qt 6 (Widgets, Multimedia, Core, Gui)
* License: LGPL v3 (open-source edition) – https://www.qt.io/licensing
* Qt is used as shared libraries / plugins; the installer ships the unmodified Qt libraries deployed by
  `windeployqt` (Windows) or `scripts/deploy_linux.sh`. You may replace them with your own build of the same
  Qt version.
* Qt Multimedia bundles **FFmpeg** (LGPL v2.1+) for audio playback.

## Qt Installer Framework (build time only)
* Used to create the setup program; not part of the installed application.

## Format specifications
* IFF (Electronic Arts, 1985), 8SVX and 16SV (Commodore-Amiga), AIFF (Apple), MAUD (MacroSystem) – the
  Fibonacci-delta algorithm follows "EA IFF 8SVX Appendix C".
