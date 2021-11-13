# `wl-mirror` - a simple Wayland output mirror client

`wl-mirror` attempts to provide a solution to sway's lack of output mirroring
by mirroring an output onto a client surface.

This project is currently a working prototype, but work is still ongoing.

## Features

- Mirror an output onto a resizable window
- Mirror an output onto another output by fullscreening the window
- Reacts to changes in output scale
- Preserves aspect ratio

## Usage

```
usage: wl-mirror [options] <output>

options:
  -h,   --help             show this help
  -v,   --verbose          enable debug logging
  -c,   --show-cursor      show the cursor on the mirrored screen (default)
  -n,   --no-show-cursor   don't show the cursor on the mirrored screen
  -s l, --scaling linear   use linear scaling (default)
  -s n, --scaling nearest  use nearest neighbor scaling
  -s e, --scaling exact    only scale to exact multiples of the output size
```

## Dependencies

- `CMake`
- `libwayland-client`
- `libwayland-egl`
- `libEGL`
- `libGLESv2`
- `wayland-scanner`

## Building

- Install Dependencies
- Clone Submodules
- Run `cmake -B build`
- Run `make -C build`

## Files

- `src/main.c`: main entrypoint
- `src/wayland.c`: Wayland and `xdg_surface` boilerplate
- `src/egl.c`: EGL boilerplate
- `src/mirror.c`: output mirroring code
