# Genesis Audio Software

## Status

 * Not ready for serious users yet.
 * Load and save any format audio file.
 * Multi-threaded audio pipeline working.
 * MIDI Keyboard support.
 * Recording and playback audio devices. PulseAudio only so far.
 * Basic synthesizer plugin. [YouTube Demo](https://www.youtube.com/watch?v=K5r_o331Eqo)
 * Basic delay plugin.

### Examples

 * `delay.c` - default recording device connected to a delay effect connected
   to default playback device.
 * `synth.c` - default midi keyboard connected to a synth connected to default
   playback device.
 * `list_devices.c` - list available audio and midi devices and watch for when
   devices are added or removed.
 * `list_supported_formats.c` - list available audio import and export formats.
 * `normalize_audio.c` - open an audio file, normalize it, and export it as a
   new file.

## The Vision

 * Safe plugins. Plugins crashing must not crash the studio.
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
   - Ability to edit multiple projects at once.
   - Support for N audio channels instead of hardcoded stereo
 * Cross-platform. I will charge money for nonfree operating systems such as
   OS X and Windows.

## Contributing

libgenesis is programmed in a tiny subset of C++:

 * No STL.
 * No `new` or `delete`.
 * No exceptions or run-time type information.
 * Pass pointers instead of references.
 * No linking against libstdc++.
 * All fields and methods in structs and classes are `public`.

### Building and Running

```
sudo apt-add-repository ppa:andrewrk/rucksack
sudo apt-get update
sudo apt-get install libglfw3-dev libepoxy-dev libglm-dev \
    libfreetype6-dev libpng12-dev librucksack-dev unicode-data libavcodec-dev \
    libavformat-dev libavutil-dev libavfilter-dev libpulse-dev rucksack cmake \
    libasound2-dev
mkdir build
cd build
cmake ..
make
./genesis
```

#### Running the Tests

```
make test
```

#### Generate Test Coverage Report

```
sudo apt-get install lcov
make coverage
# view `coverage/index.html` in a browser
```

#### Enable Optimizations

When you build, you have 2 choices:

 * `Debug`: slower, but contains useful information for reporting and fixing
   bugs. This is the default configuration.
 * `Release`: generates optimized code, but if the software crashes or
   misbehaves, it's really hard to figure out why.

To build in `Release` mode:

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./genesis
```

## Roadmap

 0. modify ui code to use libgenesis
 0. user interface for making music

## Grand Plans

### Coordinates

Positions in the audio project are in *floating point whole notes*. This is to
take into account the fact that the tempo and time signature could change at
any point in the audio project. You can convert from whole notes to frames by
calling a utility function which takes into account tempo and time signature
changes.

### Multiplayer and Peer-to-Peer

When a user opens Genesis, there should be a pane which has a set of rooms that
users can gather in and chat. For example, a lobby. Users can create other
rooms, perhaps private, to join as well. Users can invite other users to join
their open project. When a user joins the project, a peer-to-peer connection is
established so the edits do not go through a third party. Further, if the peers
are connected via LAN, network speeds will be very fast.

The server(s) that provide this chat are also peers. Individuals or businesses
could donate to the server space, similar to being a seeder in a torrent
situation, by running the server software, adding their node to the pool.

### Plugin Registry and Safety

Plugins must be provided as source code and published to the Genesis registry.
The Genesis registry will not be a single server, but once again a peer-to-peer
network. Downloading from the plugin registry will be like downloading a
torrent. By default Genesis will act as a peer on LANs when other instances of
Genesis request plugins over the LAN.

It's not clear how this goal will be accomplished, but we will attempt to build
a system where these constraints are met:

 * Plugins are provided as source code that is guaranteed to build on all
   supported platforms.
 * Plugins either have compile-time protection against malicious code and
   crashes (such as segfaults) or run-time protection.

DRM will never be supported although paid plugins are not out of the question,
as long as the restraint is met that if a user wants another user to join their
project, the other user is able to use the plugin as well.

### Project Network

Users could browse published projects and samples on the peer-to-peer network.
A sample is a rendered project, so if you want to edit the source to the sample
you always can.

Publishing a project requires licensing it generously so that it is always safe
to use projects on the network for any purpose without restriction.

The network would track downloads and usages so that users can get an idea of
popularity and quality. Projects would be categorized and tagged and related to
each other for easy browsing and searchability.

So, one user might publish some drum samples that they made as projects, one
project per sample. Another user might use all of them, edit one of them and
modify the effects, and then create 10 projects which are drum loops using the
samples. A third user might use 2-3 of these drum loops, edit them to modify
the tempo, and produce a song with them and publish the song. Yet another user
might edit the song, produce a remix, and then publish the remix.

## License

Genesis is licensed with the Lesser General Public License. A full copy of the
license text is included in the LICENSE file, but here's a non-normative
summary:

As a user you have access to the source code, and if you are willing to compile
from source yourself, you can get the software for free.

As a company you may freely use the source code in your software; the only
restriction is that if you modify Genesis source code, you must contribute those
modifications back to the Genesis project.
