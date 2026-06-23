# `wl-mirror` - a simple Wayland output mirror client

`wl-mirror` attempts to provide a solution to sway's lack of output mirroring
by mirroring an output onto a client surface.

## Features

- Mirror an output onto a resizable window
- Mirror an output onto another output by fullscreening the window
- Reacts to changes in output scale (including fractional scaling)
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
  -h,   --help                  show this help
  -V,   --version               print version
  -v,   --verbose               enable debug logging
        --no-verbose            disable debug logging (default)
  -c,   --show-cursor           show the cursor on the mirrored screen (default)
        --no-show-cursor        don't show the cursor on the mirrored screen
  -i,   --invert-colors         invert colors in the mirrored screen
        --no-invert-colors      don't invert colors in the mirrored screen (default)
  -f,   --freeze                freeze the current image on the screen
        --unfreeze              resume the screen capture after a freeze
        --toggle-freeze         toggle freeze state of screen capture
  -F,   --fullscreen            display wl-mirror as fullscreen
        --no-fullscreen         display wl-mirror as a window (default)
        --fullscreen-output O   set fullscreen target output to output O, implies --fullscreen
        --no-fullscreen-output  unset fullscreen target output, implies --no-fullscreen (default)
  -s f, --scaling fit           scale to fit (default)
  -s c, --scaling cover         scale to cover, cropping if needed
  -s e, --scaling exact         only scale to exact multiples of the output size
  -s l, --scaling linear        use linear scaling (default)
  -s n, --scaling nearest       use nearest neighbor scaling
  -b B  --backend B             use a specific backend for capturing the screen
  -t T, --transform T           apply custom transform T
  -r R, --region R              capture custom region R
        --no-region             capture the entire output (default)
  -S,   --stream                accept a stream of additional options on stdin
        --title N               specify a custom title N for the mirror window

backends:
  - auto                automatically try the backends in order of efficiency and use the first that works (default)
  - export-dmabuf       use the wlr-export-dmabuf-unstable-v1 protocol to capture outputs
  - screencopy          use the wlr-screencopy-unstable-v1 protocol to capture outputs (auto)
  - screencopy-dmabuf   use the wlr-screencopy-unstable-v1 protocol to capture outputs (via DMA-BUF)
  - screencopy-shm      use the wlr-screencopy-unstable-v1 protocol to capture outputs (via SHM)
  - extcopy             use the ext-image-copy-capture-v1 protocol to capture outputs (auto)
  - extcopy-dmabuf      use the ext-image-copy-capture-v1 protocol to capture outputs (via DMA-BUF)
  - extcopy-shm         use the ext-image-copy-capture-v1 protocol to capture outputs (via SHM)

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

title placeholders:
  the title string supports the following placeholders:
  - {width}, {height}:               size of the mirrored area
  - {x}, {y}:                        offsets on the screen
  - {target_width}, {target_height}
    {target_output}:                 info about the mirrored device
  a few perhaps useful examples:
    --title='Wayland Mirror Output {target_output}'
    --title='{target_output}:{width}x{height}+{x}+{y}'
    --title='resize set {width} {height} move position {x} {y}'
```

The [`scripts/`](scripts/) folder contains examples on how `wl-mirror` can be used.

- [`wl-present`](scripts/wl-present) is a small script to demonstrate the use
  of the `-S` option to interactively present on Sway.  
  This script is especially useful when binding the `wl-present` subcommands to
  keyboard shortcuts. See example below.
- [`release.sh`](scripts/release.sh) Generates a release tar ball for the
  currently checked out commit if there's a release tag on it.

### Sway Keybindings Example

The following keybindings shortcuts can be used in your sway config.

```
set $mode_present Present: (m) start mirror, (o) set-output, (r) set-region, (Shift-r) unset-region, (s) set-scaling, (f) toggle-freeze, (c) custom settings 
mode "$mode_present" {
    # command starts mirroring
    bindsym m mode "default"; exec wl-present mirror
    # these commands modify an already running mirroring window
    bindsym o mode "default"; exec wl-present set-output
    bindsym r mode "default"; exec wl-present set-region
    bindsym Shift+r mode "default"; exec wl-present unset-region
    bindsym s mode "default"; exec wl-present set-scaling
    bindsym f mode "default"; exec wl-present toggle-freeze
    bindsym c mode "default"; exec wl-present custom

    # return to default mode
    bindsym Return mode "default"
    bindsym Escape mode "default"
}
bindsym $mod+p mode "$mode_present"
```

This requires `wl-mirror`, the `wl-present` script, `pipectl` (optional),
slurp, and one of `wofi`, `wmenu`, `rofi`, or `dmenu`. 

Note that wl-present only allows one instance by default, but multiple
instances can be used at the same time using the `--name` option or
`WL_PRESENT_PIPE_NAME` environment variable.

### Kanshi Configuration Example

The following [kanshi](https://git.sr.ht/~emersion/kanshi) profile will launch
wl-mirror in fullscreen on an external output mirroring your internal output
when switched to with `kanshictl switch mirror-hdmi` or when selected
automatically.

```
profile mirror-hdmi {
    output eDP-1 enable mode 1920x1080 position 0,0
    output HDMI-A-1 enable mode 1920x1080 position 1920,0
    exec wl-present mirror eDP-1 --fullscreen-output HDMI-A-1 --fullscreen

    # alternatively, for wl-mirror < 0.16.4
    # exec wl-present mirror eDP-1 & sleep .5; wl-present fullscreen-output HDMI-A-1; wl-present fullscreen
}
```

## Installation

`wl-mirror` is already packaged in many distros and can be installed via the
package manager:

[![Packaging Status](https://repology.org/badge/vertical-allrepos/wl-mirror.svg?columns=3)](https://repology.org/project/wl-mirror/versions)

## Supported Wayland Compositors

`wl-mirror` should work on all Wayland compositors based on wlroots, such as
sway or hyprland, as well as compositors that implement the standard
`ext-image-copy-capture-v1` protocol.

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
- `epoll-shim` (on systems that do not have `epoll`, e.g. FreeBSD)
- `libdecor` (see `WITH_LIBDECOR`)
- `libgbm` (see `WITH_GBM`)
- `libdrm` (see `WITH_GBM`)
- `wayland-scanner`
- `scdoc` (for manual pages, see `INSTALL_DOCUMENTATION`)

## Script Dependencies

- `pipectl` (optional for `scripts/wl-present`)
- `slurp` (`scripts/wl-present`)
- `wofi`, `wmenu`, `rofi` or `dmenu` (`scripts/wl-present`)

## Building

- Install Dependencies
- Clone Submodules (`git submodule update --init`)
- Run `cmake -B build`
- Run `cmake --build build`

## CMake Options

- `INSTALL_EXAMPLE_SCRIPTS`: also install example scripts (default `OFF`)
- `INSTALL_DOCUMENTATION`: also build and install manual pages (default `OFF`)
- `WITH_LIBDECOR`: build with libdecor for window decoration (default `OFF`)
- `WITH_GBM`: build with GBM and libdrm for DMA-BUF allocation (default `OFF`)
- `FORCE_WAYLAND_SCANNER_PATH`: always use the provided path for wayland-scanner, do not use pkg-config (default empty)
- `FORCE_SYSTEM_WL_PROTOCOLS`: always use system-installed wayland-protocols, do not use submodules (default `OFF`)
- `FORCE_SYSTEM_WLR_PROTOCOLS`: always use system-installed wlr-protocols, do not use submodules (default `OFF`)
- `WL_PROTOCOL_DIR`: directory where system-installed wayland-protocols are located (default `/usr/share/wayland-protocols`)
- `WLR_PROTOCOL_DIR`: directory where system-installed wlr-protocols are located (default `/usr/share/wlr-protocols`)

## Files

- `src/main.c`: main entrypoint
- `src/options.c`: CLI and stream option parsing
- `src/wayland.c`: Wayland and `xdg_surface` boilerplate
- `src/wayland/shm.c`: Wayland SHM buffer allocation
- `src/wayland/dmabuf.c`: GBM DMA-BUF buffer allocation
- `src/egl.c`: EGL boilerplate
- `src/egl/shm.c`: EGL SHM buffer import
- `src/egl/dmabuf.c`: EGL DMA-BUF buffer import
- `src/mirror.c`: output mirroring code
- `src/mirror-export-dmabuf.c`: wlr-export-dmabuf-unstable-v1 backend code
- `src/mirror-screencopy.c`: wlr-screencopy-unstable-v1 backend code
- `src/mirror-extcopy.c`: ext-image-copy-capture-v1 backend code
- `src/transform.c`: matrix transformation code
- `src/event.c`: event loop
- `src/stream.c`: asynchronous option stream input

## License

This project is licensed under the GNU GPL version 3.0 or later (SPDX
[GPL-3.0-or-later](https://spdx.org/licenses/GPL-3.0-or-later.html)). The full
license text can also be found in the [LICENSE](/LICENSE) file.
