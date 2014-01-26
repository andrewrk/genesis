# Digital Audio Workstation

## Manifesto

 * Fast but not over-optimized. Waste no CPU cycles, but do not add
   unnecessary complexity for the sake of speed.
 * Take full advantage of multiple cores.
 * Sample-accurate mixing.
 * Never require the user to restart the program
 * Let's get these things right the first time around:
   - Undo/redo
   - Ability to edit multiple projects at once. Mix and match
   - Support for N audio channels instead of hardcoded stereo
 * Tight integration with an online sample/project sharing service
   - Make it almost easier to save it open source than to save it privately
 * Multiplayer support. Each person can be editing a different section.
 * Eventually merge with [LMMS](https://github.com/LMMS/lmms)

## Design

### SoundModule

A "module" is a generic processor. It can have any number of input and output
ports. Here is a list of types of ports:

 * audio - raw audio samples
 * notes - "midi" data
 * parameter - named values that can be read by modules

## Installation

### Dependencies

 * liblilv-dev
 * portaudio19-dev - PortAudio 2.0

### Compilation

```
mkdir build
cd build
qmake ../genesis.pro
make
```
