# Genesis Digital Audio Workstation

Genesis is a work-in-progress digital audio workstation with some ambitious
goals: peer-to-peer multiplayer editing, complete plugin safety, and a built-in
peer-to-peer community which shares plugins, projects, and samples.

Cross platform support is planned, however, currently only Linux is supported.

Genesis is not ready for serious users yet. If you're excited about the project,
follow the <a href="http://genesisdaw.org/">development blog</a>.

## Status

### Core Library (libgenesis)

 * Load and save any format audio file.
 * Multi-threaded audio pipeline working.
 * MIDI Keyboard support.
 * Recording and playback audio devices. PulseAudio only so far.
 * Basic synthesizer plugin. [YouTube Demo](https://www.youtube.com/watch?v=K5r_o331Eqo)
 * Basic delay plugin.
 * Resampling and channel remapping plugin.

#### Examples

 * `delay.c` - default recording device connected to a delay effect connected
   to default playback device.
 * `synth.c` - default midi keyboard connected to a synth connected to default
   playback device.
 * `list_devices.c` - list available audio and midi devices and watch for when
   devices are added or removed.
 * `list_supported_formats.c` - list available audio import and export formats.
 * `normalize_audio.c` - open an audio file, normalize it, and export it as a
   new file.

### GUI

 * Display recording and playback devices in a tree view.

#### Screenshots

![](https://s3.amazonaws.com/genesisdaw.org/img/beginnings-of-track-editor.png)

## Contributing

libgenesis is programmed in a tiny subset of C++:

 * No STL.
 * No `new` or `delete`.
 * No exceptions or run-time type information.
 * Pass pointers instead of references.
 * No linking against libstdc++.
 * All fields and methods in structs and classes are `public`.
 * Avoid constructor initialization lists.

### Building and Running

```
sudo apt-add-repository ppa:andrewrk/rucksack
sudo apt-get update
sudo apt-get install libglfw3-dev libepoxy-dev libglm-dev \
    libfreetype6-dev libpng12-dev librucksack-dev unicode-data libavcodec-dev \
    libavformat-dev libavutil-dev libavfilter-dev libpulse-dev rucksack cmake \
    libasound2-dev liblaxjson-dev
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
   misbehaves, it is difficult to troubleshoot.

To build in `Release` mode:

```
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./genesis
```

## Roadmap

 0. resources tree widget has view for project samples
    - list all audio assets
 0. drag from audio asset in project tree to track editor
    - makes an audio clip
 0. drag from samples directly into track editor
    - calculates shasum. if it matches existing audio asset,
      just makes an audio clip based on that. otherwise,
      adds as audio asset and then does it.

## Grand Plans

### libgenesis

The core backend and the GUI are decoupled. The core backend is in a shared
library called libgenesis which does not link against any GUI-related
libraries - not even libstdc++.

Meanwhile, the GUI depends on libgenesis and puts a user-interface on top of it.

libgenesis is intended to be a general-purpose utility library for doing
digital audio workstation related things, such as using it as the backend for
a headless computer-created music stream.

### Real-time Safety and Multithreading

Most operating systems do not attempt to make any real-time guarantees. This
means that various operations do not guarantee a maximum bound on how long it
might take. For example, when you allocate memory, it might be very quick, or
the kernel might have to do some memory defragmentation and cache invalidation
to make room for your new segment of memory. Writing to a hard drive might
cause it to have to warm up and start spinning.

This can be a disaster if one of these non-bounded operations is in the
audio rendering pipeline, especially if the latency is low. The buffer of audio
going to the sound card might empty before it gets filled up, causing a nasty
audio glitch sound known as a "buffer underrun".

In general, all syscalls are suspect when it comes to real-time guarantees. The
careful audio programmer will avoid them all.

libgenesis meets this criteria with one exception. libgenesis takes advantage
of hardware concurrency by using one worker thread per CPU core to render
audio. It carefully uses a lock-free queue data structure to avoid making
syscalls, but when there is not enough work to do and some threads are sitting
idly by, those threads need to suspend execution until there is work to do.

So if there is more work to be done than worker threads, no syscalls are made.
However, if a worker thread has nothing to do and needs to suspend execution,
it makes a `FUTEX_WAIT` syscall, and then is woken up by another worker thread
making a `FUTEX_WAKE` syscall.

### Compatibility

libgenesis follows [semver](http://semver.org/). Major version is bumped when
API or ABI compatibility is broken. Minor version is bumped when new features
are added. Patch version is bumped only for bug fixes. Until 1.0.0 is released
no effort is made toward backward compatibility whatsoever.

Genesis Audio Studio has an independent version from libgenesis. Major version
is bumped when a project file will no longer generate the same output audio as
the previous version. Minor version is bumped when new features are added.
Patch version is bumped only for bug fixes. Until 1.0.0 is released no effort
is made toward project files being backward compatible to previous versions.

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

When two (or more) users are simultaneously working on a project, the playback
head is not synchronized. The users are free to roam about the project, making
changes here and there. However, each person will see "where" in the project
the other person is working, and see the changes that they are making. So it
would be trivial, for example, for both users to look at a particular bassline,
both listening to it on loop, albeit at different offsets, while one person
works on the drums, and the other person works on the bass rhythm.

### Plugin Registry and Safety

Plugins must be provided as source code and published to the Genesis registry.
The Genesis registry will not be a single server, but once again a peer-to-peer
network. Downloading from the plugin registry will be like downloading a
torrent. By default Genesis will act as a peer on LANs when other instances of
Genesis request plugins over the LAN.

It's not clear how this goal will be accomplished, but we will attempt to build
a system where these constraints are met:

 * Plugins are provided as source code that is guaranteed to build on all
   supported platforms. It's not possible to have a plugin that works on one
   person's computer and not another.
 * Plugins either have compile-time protection against malicious code and
   crashes (such as segfaults) or run-time protection.
   - One idea: instead of one sandboxed process per plugin, have one sandboxed
     process that runs all the untrusted plugin code; the entire real-time
     execution path.

DRM will never be supported although paid plugins are not out of the question,
as long as the constraint is met that if a user wants another user to join their
project, the other user is able to use the plugin with no restrictions.

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

This project, sample, and plugin network should be easily browsable directly
from the Genesis user interface. It should be very easy to use content from the
network, and equally easy to publish content *to* the network. It should almost
be easier to save it open source than to save it privately.

### General Principles

 * Never require the user to restart the program.
 * Let's get these things right the first time around:
   - Undo/redo. Make sure it works correctly with multiplayer.
   - Support for N audio channels instead of hardcoded stereo.

### Project File Format

The first 16 bytes of every Genesis project file are:

```
ca2f 5ef5 00d8 ef0b 8074 18d0 e40b 7a4f
```

This uniquely identifies the file as a Genesis project file. The default file
extension is `.gdaw`.

After this contains an ordered list of transactions. A transaction is a set of
edits. There are 2 types of edits: put and delete.

A put contains a key and a value, each of which is a variable number of bytes.
A delete contains only a key, which is a variable number of bytes.

A transaction looks like this:

```
Offset | Description
-------+------------
     0 | uint32be crc32 of this transaction, everything excluding this field
     4 | uint32be length of transaction in bytes including this and crc32
     8 | uint32be number of put edits in this transaction
    12 | uint32be number of delete edits in this transaction
    16 | the put edits in this transaction
     - | the delete edits in this transaction
```

A put edit looks like:

```
Offset | Description
-------+------------
     0 | uint32be key length in bytes
     4 | uint32be value length in bytes
     8 | key bytes
     - | value bytes
```

A delete edit looks like:

```
Offset | Description
-------+------------
     0 | uint32be key length in bytes
     4 | key bytes
```

That's it. To read the file, apply the transactions in order. To update a file,
append a transaction. Periodically "garbage collect" by creating a new project
file with a single transaction with all the data, and then atomically rename
the new project file over the old one.

## License

Genesis is licensed with the Lesser General Public License. A full copy of the
license text is included in the LICENSE file, but here's a non-normative
summary:

As a user you have access to the source code, and if you are willing to compile
from source yourself, you can get the software for free.

As a company you may freely use the source code in your software; the only
restriction is that if you modify Genesis source code, you must contribute those
modifications back to the Genesis project.
