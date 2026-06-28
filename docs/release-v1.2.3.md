# Slammin Tuner V1.2.3

Slammin Tuner V1.2.3 is the current public release of the Slammin Captures realtime strobe tuner. It includes the verified Windows VST3 build and a GitHub-built universal macOS Audio Unit for Apple Silicon and Intel Macs.

![Slammin Tuner V1.2.3 interface](Tuner_Main.jpg)

## Downloads

- **Windows VST3**: Download `Slammin-Tuner-v1.2.3-Windows-VST3.zip` and place `Slammin Tuner.vst3` in `C:\Program Files\Common Files\VST3`.
- **macOS AU Universal**: Download `Slammin-Tuner-v1.2.3-macOS-AU-universal.zip` and place `Slammin Tuner.component` in `~/Library/Audio/Plug-Ins/Components` or `/Library/Audio/Plug-Ins/Components`.

The macOS AU is built natively from the same JUCE source on a macOS GitHub runner. It is not a wrapped or converted Windows binary.

## What's New

- Adds a three-state strobe width button:
  - **Arrow Up**: widest strobe rectangles.
  - **Arrow Right**: standard dyadic strobe rectangles.
  - **Arrow Down**: compact dyadic strobe rectangles at 68% of the standard width.
- Keeps the V1.2.2 lower spectrum/contrast bar alignment fix.
- Updates the plugin identity to V1.2.3 with the unique `ST33` VST3/AU subtype.

## macOS Validation

The release workflow builds and checks the AU on a macOS runner:

- Universal `arm64;x86_64` build for Apple Silicon and Intel Macs
- `lipo` architecture verification
- `plutil` AU metadata validation
- ad-hoc signing for AU registration
- install into the macOS Audio Unit component folder
- `auval -v aufx ST33 SLMN`

For commercial distribution without Gatekeeper prompts, notarization requires Slammin Captures Apple Developer ID credentials. The public workflow is ready for that addition, but does not include private signing certificates.

## Main Modes

- **Same Flow 1**: All strobe rows move in the same direction with shaded block styling.
- **Same Flow 2**: All strobe rows move in the same direction with solid flush-row styling.
- **Alt Flow 1**: Alternating rows move in opposite directions with shaded block styling.
- **Alt Flow 2**: Alternating rows move in opposite directions with solid flush-row styling.
- **Classic**: Moves the visible strobe structure like a single monolithic block while preserving the fixed rectangle relationship.
- **Arrow Up**: Uses the widest rectangle arrangement.
- **Arrow Right**: Uses the standard dyadic stack.
- **Arrow Down**: Uses the compact dyadic stack with shorter strobe rectangles.
- **True**: Fully band-resolved strobe mode. Rows represent different harmonic bands of the same note. Brighter rows mean stronger, more reliable harmonic energy in that band.
- **Spectrum Bar**: Keeps the red/blue lower bar style. Disable it for selected-color contrast styling.

## Controls

- **Tuning**: Selects built-in or custom tuning profiles.
- **A4**: Reference pitch, interpreted as Hz when typed manually.
- **Track**: Balanced, Fast, or Precision pitch tracking.
- **Input**: Auto, Left, Right, or Mid input source.
- **Duration**: Strobe response amount, interpreted as `x` when typed manually.
- **Color**: Fifteen guitar-focused colors including neon cyan, neon pink, electric green, amber, violet, red, teal, white, gold, and warm ivory.
- **Strobe Rows**: Selects 1 to 4 strobe rows.
- **Mute**: Mutes the input passthrough.
- **Tone**: Plays a quiet reference tone at the current target pitch.
- **Calm**: Reduced-motion mode.
- **Note Name**: Click the large note display to switch flat/sharp spelling.
- **Logo**: Opens the Slammin Captures store, Ko-fi shop, and Tone3000 profile.

## Settings Recall

Slammin Tuner recalls user settings between sessions, including tuning, A4, tracking mode, input mode, duration, color, strobe mode, row count, strobe width, True mode, mute, tone, calm, bar style, and flat/sharp note spelling.

## Tunings

Built-in profiles include Chromatic Equal, Guitar Standard, Half-Step Down, Full-Step Down, C# Standard, C Standard, Drop D, Drop Db, Drop C, Drop B, Drop A, DADGAD, Open G, Open D, Open E, Open C, Open A, Open F, Open B, Open D Minor, Open G Minor, All Fourths, Nashville High, Baritone B, Shelf Sweetener, VH Half-Flat, 7 String Guitar, 7 String Drop A, 7 String Half Down, 6 String Bass, Bass Drop D, and 5 String Bass.

## Version Metadata

- Product name: `Slammin Tuner`
- Version: `1.2.3`
- Manufacturer code: `SLMN`
- Plugin code / AU subtype: `ST33`
- Bundle ID: `com.slammincaptures.slammin-tuner.v123`
