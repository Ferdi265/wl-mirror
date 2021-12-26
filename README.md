# `wl-mirror` - a simple Wayland output mirror client

`wl-mirror` attempts to provide a solution to sway's lack of output mirroring
by mirroring an output onto a client surface.

This project is currently a working prototype, but work is still ongoing.

## Features

- Mirror an output onto a resizable window
- Mirror an output onto another output by fullscreening the window
- Reacts to changes in output scale
- Preserves aspect ratio
- Corrects for flipped or rotated outputs
- Supports custom flips or rotations
- Supports mirroring custom regions of outputs

![demo screenshot](https://user-images.githubusercontent.com/4077106/141605347-37ba690c-f885-422a-93a6-81d5a48bee13.png)

## Usage

```
usage: wl-mirror [options] <output>

options:
  -h,   --help             show this help
  -v,   --verbose          enable debug logging
  -c,   --show-cursor      show the cursor on the mirrored screen (default)
  -i,   --invert-colors    invert colors in the mirrored screen
  -n,   --no-show-cursor   don't show the cursor on the mirrored screen
  -s l, --scaling linear   use linear scaling (default)
  -s n, --scaling nearest  use nearest neighbor scaling
  -s e, --scaling exact    only scale to exact multiples of the output size
  -t T, --transform T      apply custom transform T
  -r R, --region R         capture custom region R

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
- `src/transform.c`: matrix transformation code
