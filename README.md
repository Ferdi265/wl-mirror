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
  -v,   --verbose          enable debug logging
        --no-verbose       disable debug logging (default)
  -c,   --show-cursor      show the cursor on the mirrored screen (default)
        --no-show-cursor   don't show the cursor on the mirrored screen
  -i,   --invert-colors    invert colors in the mirrored screen
        --no-invert-colors don't invert colors in the mirrored screen (default)
  -s l, --scaling linear   use linear scaling (default)
  -s n, --scaling nearest  use nearest neighbor scaling
  -s e, --scaling exact    only scale to exact multiples of the output size
  -t T, --transform T      apply custom transform T
  -r R, --region R         capture custom region R
        --no-region        capture the entire output (default)
  -S,   --stream           accept a stream of additional options on stdin

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
```

The [`scripts/`](scripts/) folder contains examples on how `wl-mirror` can be used.

- [`wl-present`](scripts/wl-present) is a small script to demonstrate the use
  of the `-S` option to interactively present on Sway.  
  This script is especially useful when binding the `wl-present` subcommands to
  keyboard shortcuts.

## Dependencies

- `CMake`
- `libwayland-client`
- `libwayland-egl`
- `libEGL`
- `libGLESv2`
- `wayland-scanner`

## Script Dependencies

- `pipectl` (`scripts/wl-present`)
- `slurp` (`scripts/wl-present`)
- `rofi` or `dmenu` (`scripts/wl-present`)

## Building

- Install Dependencies
- Clone Submodules
- Run `cmake -B build`
- Run `make -C build`

## CMake Options

- `INSTALL_EXAMPLE_SCRIPTS`: also install example scripts (default `ON`)
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
- `src/transform.c`: matrix transformation code
- `src/event.c`: event loop and asynchronous option stream input
