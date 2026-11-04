# Pinned QMK color implementation

The four C/header files in this directory are unmodified copies from `quantum/` in SonixQMK commit `62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60`.

They retain their upstream GPL-2.0-or-later notices. The native benchmark compiles this actual HSV → RGB implementation with `USE_CIE1931_CURVE`, matching the firmware build. The small `tests/stubs` headers replace host-incompatible platform attributes only. These files are used by host tests/benchmarks, not as a second firmware implementation.
