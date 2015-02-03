# Digital Audio Workstation

## Manifesto

 * Fast but not over-optimized. Waste no CPU cycles, but do not add
   unnecessary complexity for the sake of speed.
 * Take full advantage of multiple cores.
 * When there is a tradeoff between speed and memory, sacrifice memory.
 * Sample-accurate mixing.
 * Never require the user to restart the program
 * Let's get these things right the first time around:
   - Undo/redo
   - Ability to edit multiple projects at once. Mix and match
   - Support for N audio channels instead of hardcoded stereo
 * Tight integration with an online sample/project sharing service. Make it
   almost easier to save it open source than to save it privately
 * Multiplayer support. Each person can simultaneously edit different sections.
 * Backend decoupled from the UI. Someone should be able to depend only
   on a C library and headlessly synthesize music.
 * Friends with [LMMS](https://github.com/LMMS/lmms)

## Installation

### System Dependencies

 * libgroove
 * FreeType

### Building and Running

```
cargo run
```

## Roadmap

 0. Open an audio file with libav.
 0. Display the waveform of the audio file in the display.
