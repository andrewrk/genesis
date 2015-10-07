# Genesis Digital Audio Workstation

Genesis is a work-in-progress digital audio workstation with some ambitious
goals: peer-to-peer multiplayer editing, complete plugin safety, and a built-in
peer-to-peer community which shares plugins, projects, and samples.

Cross platform support is planned, however, currently only Linux is supported.

Genesis is not ready for serious users yet. If you're excited about the project,
follow the [development blog](http://genesisdaw.org/) or sustain development
with [financial support](https://salt.bountysource.com/teams/gdaw).

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
 * Browse sample files in a tree widget and hear file previews.
 * Drag audio clips into the track editor.

#### Screenshots

![](https://s3.amazonaws.com/genesisdaw.org/img/genesis-2015-05-22.png)

## Contributing

libgenesis is programmed in a tiny subset of C++:

 * No STL.
 * No `new` or `delete`.
 * No exceptions or run-time type information.
 * Pass pointers instead of references.
 * No linking against libstdc++.
 * All fields and methods in structs and classes are `public`.
 * Avoid constructor initialization lists.

### Dependencies

 * [ALSA Library](http://www.alsa-project.org/)
 * [cmake](http://www.cmake.org/)
 * [libepoxy](https://github.com/anholt/libepoxy)
 * [freetype](http://www.freetype.org/)
 * [GLFW](http://www.glfw.org/)
 * [glm](http://glm.g-truc.net/0.9.6/index.html)
 * [FFmpeg](http://ffmpeg.org/)
 * [libjack2](http://jackaudio.org/)
 * [liblaxjson](https://github.com/andrewrk/liblaxjson)
 * [libpng](http://www.libpng.org/pub/png/libpng.html)
 * [libpulseaudio](http://www.freedesktop.org/wiki/Software/PulseAudio/)
 * [rhash](http://rhash.anz.ru/)
 * [rucksack](https://github.com/andrewrk/rucksack)

### Building and Running

```
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

 0. also default playback hangs when recovering from stream error
 0. get alsa raw playback working and jack playback working
 0. make toggling playback and seeking the play head work
 0. start playing in the middle of an audio clip segment
 0. make a playback selection
 0. ability to export audio to wav
 0. make sure recording works and is stable
 0. sequencer / piano roll
 0. drag samples directly to track editor
 0. audio clip segment: display waveform
 0. audio clip segment: drag to move
 0. right click to delete audio clip segments
 0. drag audio clip: display clip even while drag ongoing
 0. quantization when dragging (hold alt to disable)
 0. resample node needed for audio clip nodes
 0. undo/redo crash

## Design

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

Genesis is licensed with the General Public License. A full copy of the
license text is included in the LICENSE file, but here's a non-normative
summary:

As a user you have access to the source code, and if you are willing to compile
from source yourself, you can get the software for free.

As a company you may use the source code in your software as long as you
publish your software's source code under a free software license.
