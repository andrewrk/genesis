#ifndef AUDIO_BACKEND_DUMMY_HPP
#define AUDIO_BACKEND_DUMMY_HPP

struct AudioHardware;

struct OpenPlaybackDeviceDummy {

};

struct OpenRecordingDeviceDummy {

};

struct AudioHardwareDummy {

};

int audio_hardware_init_dummy(AudioHardware *ah);

#endif

