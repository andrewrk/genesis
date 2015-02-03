# Digital Audio Workstation

## Status

Not cool yet.

## The Vision

 * Take full advantage of multiple cores.
 * Sample-accurate mixing.
 * Never require the user to restart the program
 * Let's get these things right the first time around:
   - Undo/redo
   - Ability to edit multiple projects at once. Mix and match
   - Support for N audio channels instead of hardcoded stereo
 * Tight integration with an online sample/project sharing service. Make it
   almost easier to save it open source than to save it privately.
 * Multiplayer support. Each person can simultaneously edit different sections.
 * Backend decoupled from the UI. Someone should be able to depend only
   on a C library and headlessly synthesize music.

## Contributing

genesis is programmed in a tiny subset of C++. It does not link against
libstdc++. It's basically C except with some templates and data structures
sprinkled here and there.

### Building and Running

```
sudo apt-get install libgroove-dev libglfw3-dev
make
./build/genesis
```

## Roadmap

 0. UI for the user to load a file.
 0. Load all the audio file's channels into memory.
 0. Display all of the audio file's channels in a list.
 0. Render channels graphically.
 0. Ability to make a selection.
 0. Playback to speakers.
 0. Delete selection.
 0. Undo/Redo
 0. Multiplayer
