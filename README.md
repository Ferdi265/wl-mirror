# `wl-mirror` - a simple Wayland output mirror client

`wl-mirror` attempts to provide a solution to sway's lack of output mirroring
by mirroring an output onto a client surface.

## Features

- Mirror an output onto a resizable window
- Mirror an output onto another output by fullscreening the window
- Reacts to changes in output scale
- Preserves aspect ratio
- Corrects for flipped or rotated outputs
- Supports custom flips or rotations
- Supports mirroring custom regions of outputs
- Supports receiving additional options on stdin for changing the mirrored
  screen or region on the fly (works best when used with [pipectl](https://github.com/Ferdi265/pipectl))

![demo screenshot](https://user-images.githubusercontent.com/4077106/141605347-37ba690c-f885-422a-93a6-81d5a48bee13.png)

## Usage

```
usage: wl-mirror [options] <output>

options:
  -h,   --help             show this help
  -V,   --version          print version
  -v,   --verbose          enable debug logging
        --no-verbose       disable debug logging (default)
  -c,   --show-cursor      show the cursor on the mirrored screen (default)
        --no-show-cursor   don't show the cursor on the mirrored screen
  -i,   --invert-colors    invert colors in the mirrored screen
        --no-invert-colors don't invert colors in the mirrored screen (default)
  -f,   --freeze           freeze the current image on the screen
        --unfreeze         resume the screen capture after a freeze
        --toggle-freeze    toggle freeze state of screen capture
  -s l, --scaling linear   use linear scaling (default)
  -s n, --scaling nearest  use nearest neighbor scaling
  -s e, --scaling exact    only scale to exact multiples of the output size
  -b B  --backend B        use a specific backend for capturing the screen
  -t T, --transform T      apply custom transform T
  -r R, --region R         capture custom region R
        --no-region        capture the entire output (default)
  -S,   --stream           accept a stream of additional options on stdin

backends:
  - auto        automatically try the backends in order and use the first that works (default)
  - dmabuf      use the wlr-export-dmabuf-unstable-v1 protocol to capture outputs
  - screencopy  use the wlr-screencopy-unstable-v1 protocol to capture outputs

transforms:
  transforms are specified as a dash-separated list of flips followed by a rotation
  flips are applied before rotations
  - normal                         no transformation
  - flipX, flipY                   flip the X or Y coordinate
  - 0cw,  90cw,  180cw,  270cw     apply a clockwise rotation
  - 0ccw, 90ccw, 180ccw, 270ccw    apply a counter-clockwise rotation
  the following transformation options are provided for compatibility with sway output transforms
  - flipped                        flip the X coordinate
  - 0,    90,    180,    270       apply a clockwise rotation

regions:
  regions are specified in the format used by the slurp utility
  - '<x>,<y> <width>x<height> [output]'
  on start, the region is translated into output coordinates
  when the output moves, the captured region moves with it
  when a region is specified, the <output> argument is optional

stream mode:
  in stream mode, wl-mirror interprets lines on stdin as additional command line options
  - arguments can be quoted with single or double quotes, but every argument must be fully
    quoted or fully unquoted
  - unquoted arguments are split on whitespace
  - no escape sequences are implemented
```

The [`scripts/`](scripts/) folder contains examples on how `wl-mirror` can be used.

- [`wl-present`](scripts/wl-present) is a small script to demonstrate the use
  of the `-S` option to interactively present on Sway.  
  This script is especially useful when binding the `wl-present` subcommands to
  keyboard shortcuts.
- [`release.sh`](scripts/release.sh) Generates a release tar ball for the
  currently checked out commit if there's a release tag on it.

## Installation

`wl-mirror` is already packaged in many distros and can be installed via the
package manager:

- Arch Linux: `pacman -S wl-mirror`
- Fedora: `dnf install wl-mirror`
- Ubuntu: `apt install wl-mirror` in mantic (23.10), .deb file download for jammy (22.04) in [releases](https://github.com/Ferdi265/wl-mirror/releases/latest/)
- Debian: `apt install wl-mirror` in unstable, .deb file download for bookworm (12) in [releases](https://github.com/Ferdi265/wl-mirror/releases/latest/)
- Alpine Linux: `apk add wl-mirror wl-mirror-doc`
- NixOS: `nix profile install nixpkgs#wl-mirror`
- Void Linux: `xbps-install -S wl-mirror`
- FreeBSD Ports: [x11/wl-mirror](https://www.freshports.org/x11/wl-mirror)
- DragonflyBSD Ports: [x11/wl-mirror](https://github.com/DragonFlyBSD/DPorts/tree/master/x11/wl-mirror)
- Gentoo GURU: [gui-apps/wl-mirror](https://github.com/gentoo-mirror/guru/tree/master/gui-apps/wl-mirror)
- LiGurOS: [gui-apps/wl-mirror](https://gitlab.com/liguros/liguros-repo/-/tree/develop/gui-apps/wl-mirror)

## Supported Wayland Compositors

`wl-mirror` should work on all Wayland compositors based on wlroots, such as
sway or hyprland.

`wl-mirror` currently does not work on KDE and Gnome, due to `wl-mirror` not
supporting the XDG Desktop Portal screen sharing protocol. This is being worked
on (see issues [#16](https://github.com/Ferdi265/wl-mirror/issues/16) and
[#17](https://github.com/Ferdi265/wl-mirror/issues/17)).

## Dependencies

- `CMake`
- `pkg-config`
- `libwayland-client`
- `libwayland-egl`
- `libEGL`
- `libGLESv2`
- `wayland-scanner`
- `scdoc` (for manual pages)

## Script Dependencies

- `pipectl` (`scripts/wl-present`)
- `slurp` (`scripts/wl-present`)
- `rofi` or `dmenu` (`scripts/wl-present`)

## Building

- Install Dependencies
- Clone Submodules (`git submodule update --init`)
- Run `cmake -B build`
- Run `make -C build`

## CMake Options

- `INSTALL_EXAMPLE_SCRIPTS`: also install example scripts (default `OFF`)
- `INSTALL_DOCUMENTATION`: also build and install manual pages (default `OFF`)
- `FORCE_SYSTEM_WL_PROTOCOLS`: always use system-installed wayland-protocols, do not use submodules (default `OFF`)
- `FORCE_SYSTEM_WLR_PROTOCOLS`: always use system-installed wlr-protocols, do not use submodules (default `OFF`)
- `WL_PROTOCOL_DIR`: directory where system-installed wayland-protocols are located (default `/usr/share/wayland-protocols`)
- `WLR_PROTOCOL_DIR`: directory where system-installed wlr-protocols are located (default `/usr/share/wlr-protocols`)

## Files

- `src/main.c`: main entrypoint
- `src/options.c`: CLI and stream option parsing
- `src/wayland.c`: Wayland and `xdg_surface` boilerplate
- `src/egl.c`: EGL boilerplate
- `src/mirror.c`: output mirroring code
- `src/mirror-dmabuf.c`: wlr-export-dmabuf-unstable-v1 backend code
- `src/mirror-screencopy.c`: wlr-screencopy-unstable-v1 backend code
- `src/transform.c`: matrix transformation code
- `src/event.c`: event loop and asynchronous option stream input
