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
 * Eventually merge with [LMMS](https://github.com/LMMS/lmms)

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

## Design

### Project

A Project has the following data:

 * sample rate, aka frames per second
 * channel count. mono, stereo, surround sound, etc.

A Project has a root Module node. This is the "master mixer track".
From here you can trace child nodes and discover every Module.

A Project has an ordered list of Events. They are indexed by frame index.
Another way of saying this is that an Event occurs at a specific frame.


### Module

A "module" is a generic processor. It can have any number of input and output
ports. Here is a list of types of ports:

 * audio - raw audio samples
 * notes - "midi" data
 * parameter - named values that can be read by modules

An LV2 plugin acts as a Module.


### Event

An Event can be one of these types:

 * Automation - this describes a parameter to modulate
 * Notes - "midi" data
 * Sample
