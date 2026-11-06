# FusionDrive (AU + VST3 + Standalone, macOS universal)

Behavioural model of a "vintage drive" saturation stage with a second, physically-modelled **Analog** engine.
Not affiliated with Solid State Logic; it is not a circuit-level clone (SSL does not publish the schematic).

## Install (macOS, 3 steps)
1. Open Terminal in this folder.
2. `./build_mac.sh`   (first run downloads JUCE; installs Xcode CLT / CMake via brew if missing)
3. Restart your DAW and rescan. Installer for other machines: `dist/FusionDrive-1.0.0.dmg`

AU and VST3 are built in the same run. Ableton Live 12: Settings > Plug-ins > enable "Use Audio Units" / "Use VST3 Plug-in System Folders", then Rescan.
AU check: `auval -v aufx Fdrv Fdau`.

## Engines
**Vintage**: Drive -> tone-weighted pre-emphasis (Tone picks which band saturates) -> biased generalized soft clipper
(Character = even/odd balance via bias, Hardness = knee exponent) -> inverse de-emphasis -> DC servo.

**Analog** (button): flux pre-emphasis (flux ~ integral of voltage, so lows saturate first) -> Jiles-Atherton hysteresis
input transformer (RK4) -> leakage-L / winding-C low-pass -> class-A asymmetric stage with PSU **Sag** ->
op-amp **Slew** limiter -> second J-A output transformer -> flux de-emphasis -> **Warmth** (inductance bloom, HF loss) ->
DC servo. **Iron** = core drive, **Drift** = per-channel component tolerance (natural L/R differences).

Extras: 2x/4x/8x FIR oversampling with latency compensation, Auto Gain (RMS-matched), Mid/Side, Mix, Input/Output trim,
7 factory presets, live spectrum + meters.

## DSP self-test (no JUCE needed)
`clang++ -O2 -std=c++17 tests/dsp_test.cpp -o dsp_test && ./dsp_test` prints harmonic content per drive level and runs a
non-finite/stability stress test.

## Free cloud build (macOS .pkg/.dmg + Windows Setup.exe)
Push the CONTENTS of this folder to the ROOT of a GitHub repo (free) - `.github` must sit at the repo root. The workflow in `.github/workflows/build.yml` builds on free macOS and Windows
runners; download results from the Actions tab > latest run > Artifacts. Windows ships VST3 only (Audio Units are macOS-only).
