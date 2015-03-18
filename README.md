# Genesis Audio Software

## Status

 * Not ready for serious users yet.
 * You can open any format audio file and zoom and scroll around.
 * You can play a selection (supported audio backends: pulseaudio).
 * You can delete sections of audio.
 * You can save as any format audio file.

![](https://s3.amazonaws.com/superjoe/temp/genesis-deb4513.png)

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
   - Ability to edit multiple projects at once.
   - Support for N audio channels instead of hardcoded stereo

## Contributing

genesis is programmed in a tiny subset of C++:

 * No STL.
 * No `new` or `delete`.
 * No exceptions or run-time type information.
 * Pass pointers instead of references.

### Building and Running

```
sudo apt-add-repository ppa:andrewrk/rucksack
sudo apt-get update
sudo apt-get install libglfw3-dev libepoxy-dev libglm-dev \
    libfreetype6-dev libpng12-dev librucksack-dev unicode-data libavcodec-dev \
    libavformat-dev libavutil-dev libavfilter-dev libpulse-dev rucksack
mkdir build
cd build
cmake ..
make
./genesis
```

## Roadmap

 0. example program which watches devices
 0. example program which opens an audio file, normalizes the volume, and saves it
 0. example synth program which makes a note sound
    0. builtin simple sine wave input device
    0. builtin playback output device
    0. building a pipeline

## Grand Plans

### Nodes

A node is the basic component in the audio pipeline. At the end of the audio
graph is likely a playback node - one that sends its audio to a playback
device. A node can have these things, all time-synchronized:

 * Audio input
 * Note data input
 * Audio output
 * Note data output
 * Parameters input
 * Controller output

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

### Panicking

While Genesis is in heavy development and things are changing quickly, the
`panic` function will be used nearly everywhere instead of error checking. Once
Genesis's user interface, backend, and API begin to stabilize, there will be
a global code audit where `panic` usage is replaced with robust error checking
and handling.

For example, memory allocation functions currently call `panic` for out of
memory conditions. Once the codebase begins to stabilize, this will be removed
and everywhere that allocates memory must check for a null pointer and report
the appropriate error.

## License

Genesis is licensed with the Lesser General Public License. A full copy of the
license text is included in the LICENSE file, but here's a non-normative
summary:

As a user you have access to the source code, and if you are willing to compile
from source yourself, you can get the software for free.

As a company, this means you may freely use the source code in your software,
the only restriction being that if you modify Genesis source code, you must
contribute those modifications back to the Genesis project.
