![Slammin Tuner V1.1.6](https://raw.githubusercontent.com/SlamminMofo/slammin-tuner/main/docs/slammin-tuner-v1.1.6.png)

# Slammin Tuner V1.1.6

Slammin Tuner V1.1.6 is a realtime strobe tuner plugin with a medium scalable GUI, selectable strobe flow modes, custom colors, guitar/bass tuning profiles, and direct numeric entry for A4 and Duration.

## Downloads

- **Windows VST3**: Download `Slammin-Tuner-v1.1.6-Windows-VST3.zip` and place `Slammin Tuner.vst3` in `C:\Program Files\Common Files\VST3`.
- **macOS AU Universal**: Download `Slammin-Tuner-v1.1.6-macOS-AU-universal.zip` and place `Slammin Tuner.component` in `~/Library/Audio/Plug-Ins/Components` or `/Library/Audio/Plug-Ins/Components`.

The macOS AU is built as a universal Audio Unit from the same JUCE source, with `arm64` and `x86_64` slices for Apple Silicon and Intel Macs.

## macOS Validation

The release workflow builds and checks the AU on a macOS runner:

- Universal `arm64;x86_64` build
- `lipo` architecture verification
- `plutil` AU metadata validation
- ad-hoc signing for AU registration
- install into the macOS Audio Unit component folder
- `auval -v aufx ST26 SLMN`

For commercial distribution without Gatekeeper prompts, notarization requires Slammin Captures Apple Developer ID credentials. The CI path is ready for that addition, but the public workflow does not include private signing certificates.

## Main Modes

- **Same Flow 1**: All strobe rows move in the same direction with shaded block styling.
- **Same Flow 2**: All strobe rows move in the same direction with solid flush-row styling.
- **Alt Flow 1**: Alternating rows move in opposite directions with shaded block styling.
- **Alt Flow 2**: Alternating rows move in opposite directions with solid flush-row styling. This is the default.
- **True**: Fully band-resolved strobe mode. Rows represent different harmonic bands of the same note. Brighter rows mean stronger, more reliable harmonic energy in that band.
- **Spectrum Bar**: Keeps the red/blue lower bar style. Disable it for selected-color contrast styling.

## Controls

- **Tuning**: Selects built-in or custom tuning profiles.
- **A4**: Reference pitch, interpreted as Hz when typed manually.
- **Track**: Balanced, Fast, or Precision pitch tracking.
- **Input**: Auto, Left, Right, or Mid input source.
- **Duration**: Strobe response amount, interpreted as `x` when typed manually.
- **Color**: Fifteen guitar-focused colors including neon cyan, neon pink, electric green, amber, violet, red, teal, white, gold, and warm ivory.
- **Mute**: Mutes the input passthrough.
- **Tone**: Plays a quiet reference tone at the current target pitch.
- **Calm**: Reduced-motion mode.
- **Note Name**: Click the large note display to switch flat/sharp spelling.
- **Logo**: Opens the Slammin Captures store, Ko-fi shop, and Tone3000 profile.

## Tunings

Built-in profiles include Chromatic Equal, Guitar Standard, Half-Step Down, Full-Step Down, C# Standard, C Standard, Drop D, Drop Db, Drop C, Drop B, Drop A, DADGAD, Open G, Open D, Open E, Open C, Open A, Open F, Open B, Open D Minor, Open G Minor, All Fourths, Nashville High, Baritone B, Shelf Sweetener, VH Half-Flat, 7 String Guitar, 7 String Drop A, 7 String Half Down, 6 String Bass, Bass Drop D, and 5 String Bass.

## Version Metadata

- Product name: `Slammin Tuner`
- Version: `1.1.6`
- Manufacturer code: `SLMN`
- Plugin code: `ST26`
- Bundle ID: `com.slammincaptures.slammin-tuner.v116`

