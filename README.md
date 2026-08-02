# Soroka ZMK firmware

This repository builds wired USB firmware for the
[Soroka](https://github.com/kapee1/soroka/) keyboard on a SparkFun Pro Micro
RP2040-compatible controller. The encoder and the 5x5 WS2812 RGB matrix are
enabled. The matrix shows a small heart animation after power-on.
ZMK Studio is available over the same USB connection and does not require an
unlock key.

GitHub Actions is pinned to the stable ZMK `v0.3` release. Download the UF2
artifact from a successful workflow run, enter the RP2040 bootloader, and copy
the UF2 file to the mounted `RPI-RP2` drive.

## RGB picture and animation

The animation is generated at run time by `render_procedural_heart()` in
`config/boards/shields/soroka/matrix_animation.c`. The `heart_mask` array
defines the 5x5 shape and relative pixel intensity. `FRAME_TIME_MS`, the
brightness limits, and the phase controls at the top of the file adjust the
frame rate, breathing cycle, and color cycle.

The default mapping assumes that all rows run left-to-right, matching the old
firmware. If the image has every second row mirrored, set
`MATRIX_SERPENTINE` to `1`.

## Choose physical layout

The selected layout is `split_space_1_225_275_1`. Other supported layouts are
defined under `config/boards/shields/soroka/layouts`.
  
  ------

![Physical Layouts](/kle/soroka_matrix.png)
