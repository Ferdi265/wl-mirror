# `wl-mirror` - a simple Wayland output mirror client

`wl-mirror` attempts to provide a solution to sway's lack of output mirroring
by mirroring an output onto a client surface.

This project is still work-in-progress. The basic boilerplate is there, and only
receiving and drawing the dmabufs is missing.

## Dependencies

- `CMake`
- `libwayland-client`
- `libwayland-egl`
- `libEGL`
- `libGLESv2`
- `wayland-scanner`
- `wayland-protocols`

## Files

- `src/main.c`: main entrypoint
- `src/wayland.c`: Wayland and `xdg_surface` boilerplate
- `src/egl.c`: EGL boilerplate
- `src/mirror.c`: output mirroring code
