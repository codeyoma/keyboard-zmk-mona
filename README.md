# moNa2 ZMK firmware

Firmware for a moNa2 (two Seeed XIAO nRF52840, PAW3222 trackball on the right half), with
the same layers as my [Sweep](https://github.com/codeyoma/keyboard-zmk-sweep). The extra
moNa2 keys are transparent for now.

- ZMK v0.3; the matrix, pins and trackball wiring come from the official
  [sayu-hub/zmk-config-moNa2-v2](https://github.com/sayu-hub/zmk-config-moNa2-v2).
- No dongle: the right half (trackball) is the central and connects to the computer.
- Mouse layer: Raise + T. N steps the mouse key speed, M held turns the pointer keys and the
  trackball into scrolling, / held slows both down (Snipe).
- Flash `mona2_right` and `mona2_left` from the GitHub Actions artifacts.

![Keymap](./mona2_keymap.svg)
