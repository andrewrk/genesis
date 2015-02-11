# Digital Audio Workstation

## Status

Not cool yet.

## The Vision

 * Safe plugins. Plugins crashing must not crash the studio.
 * Cross-platform.
 * Projects must work on every computer. It's not possible to have a plugin
   that works on one person's computer and not another.
 * Tight integration with an online sample/project sharing service. Make it
   almost easier to save it open source than to save it privately.
 * Multiplayer support. Each person can simultaneously edit different sections.
 * Backend decoupled from the UI. Someone should be able to depend only
   on a C library and headlessly synthesize music.
 * Take full advantage of multiple cores.
 * Sample-accurate mixing.
 * Never require the user to restart the program
 * Let's get these things right the first time around:
   - Undo/redo
   - Ability to edit multiple projects at once. Mix and match
   - Support for N audio channels instead of hardcoded stereo

## Contributing

genesis is programmed in a tiny subset of C++:

 * No linking against libstdc++.
 * No STL.
 * No `new` or `delete`.
 * No exceptions or run-time type information.

### Building and Running

```
sudo apt-get install libgroove-dev libsdl2-dev libepoxy-dev libglm-dev libfreetype6-dev
make
./build/genesis
```

## Roadmap

 0. Make the text box support all the text editing controls you would expect
 0. Make the text box not auto-size
 0. UI for the user to load a file.
 0. Load all the audio file's channels into memory.
 0. Display all of the audio file's channels in a list.
 0. Render channels graphically.
 0. Ability to make a selection.
 0. Playback to speakers.
 0. Delete selection.
 0. Undo/Redo
 0. Multiplayer
