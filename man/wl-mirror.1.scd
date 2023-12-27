wl-mirror(1)

# NAME

wl-mirror - a simple Wayland output mirror client

# SYNOPSIS

*wl-mirror* [-h,-V,-v,-c,-i,-f,-s S,-b B,-t T,-r R,-S] <output>

# OPTIONS

*-h, --help*
	Show help message and exit.

*-V, --version*
	Show version information and exit.

*-v, --verbose*
*    --no-verbose*
	Enable or disable debug logging.

*-c, --show-cursor*
*    --no-show-cursor*
	Show the cursor on the mirrored surface (enabled by default).

*-i, --invert-colors*
*    --no-invert-colors*
	Invert colors on the mirrored surface.

*-f, --freeze*
*    --unfreeze*
*    --toggle-freeze*
	Freeze, unfreeze, or toggle freezing of the current image on the screen.

*-F, --fullscreen*
*    --no-fullscreen*
	Open as a fullscreen window, or make the current window fullscreen in stream
	mode.

*    --fullscreen-output O*
*    --no-fullscreen-output*
	Set a different output to fullscreen on (default is the current output).

*-s l, --scaling linear*
	Use linear scaling (enabled by default).

*-s n, --scaling nearest*
	Use nearest-neighbor scaling.

*-s e, --scaling exact*
	Use exact multiple scaling (and if necessary add letterboxing).
	Uses nearest neighbor scaling for downscaling.

*-b B, --backend B*
	Use a specific screen capture backend, see *BACKENDS*.

*-t T, --transform T*
	Apply custom transform (rotation and flipping), see *TRANSFORMS*.

*-r R, --region R*
	Capture custom screen region R, see *REGIONS*.

*-S, --stream*
	Accept a stream of additional options on stdin, see *STREAM MODE*.

# BACKENDS

*auto*
	Automatically try backends in order and use the first that works (enabled by default).
	The next backend is selected automatically when the current backend fails to capture a frame 10 times in a row.

*dmabuf*
	Use the *wlr-export-dmabuf-unstable-v1* protocol to capture outputs (requires wlroots).
	This backend keeps the image data on the GPU and does not need expensive copies to the CPU and back.

*screencopy*
	Use the *wlr-screencopy-unstable-v1* protocol to capture outputs (requires wlroots)
	This backend passes the image data via shared memory on the CPU, but may have better compatibility with complex GPU driver configurations (e.g., multi GPU).

# TRANSFORMS

Transforms are specified as a dash-separated list of flips followed by a rotation amount. Flips are applied before rotations, both flips and rotations are optional.
Custom transformations are applied after adjusting for the wayland output transform, so that if no custom transformations are applied, the mirrored surface is displayed right-side-up.

*normal*
	No transformation (default transformation).

*flipX, flipY*
	Flip the X or Y coordinate of the image, i.e. *flipX* means the mirrored surface has left and right swapped.

*0cw, 90cw, 180cw, 270cw*
	Apply a clockwise rotation.

*0ccw, 90ccw, 180ccw, 270ccw*
	Apply a counter-clockwise rotation.

The following transformation options are provided for compatibility with *sway-output*(5) transforms:

*flipped*
	Same as *flipX*.

*0, 90, 180, 270*
	Same as *0cw, 90cw, 180cw, 270cw*.

# REGIONS

Regions are specified in the format used by the *slurp*(1) utility:

	'*x*,*y* *width*x*height* [*output*]'

When processing the region option, the region is translated into output coordinates, so when the output moves, the captured region moves with it.
When a region is specified, the *output* positional argument is optional.

# STREAM MODE

In stream mode, *wl-mirror* interprets lines on stdin as additional command line options.

- Arguments can be quoted with single or double quotes, but every argument must be fully quoted.
- Unquoted arguments are split on whitespace.
- No escape sequences are implemented.

Option lines on stdin are processed asynchronously, and can override all options and the captured output.
Stream mode is used by *wl-present*(1) to add interactive controls to *wl-mirror*.

# AUTHORS

Maintained by Ferdinand Bachmann <theferdi265@gmail.com>. More information on *wl-mirror* can be found at <https://github.com/Ferdi265/wl-mirror>.

# SEE ALSO

*wl-present*(1) *slurp*(1) *sway-output*(5)
