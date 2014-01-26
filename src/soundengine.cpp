#include "soundengine.h"
#include <portaudio.h>

#include <QCoreApplication>

#include <iostream>


PaStream *stream = NULL;

static void terminate() {
	Pa_Terminate();
}

static int callback(const void *input, void *output,
		unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo,
		PaStreamCallbackFlags statusFlags, void *userData)
{
	memset(output, 0, frameCount * 4);
	return paContinue;
}

void SoundEngine::initialize()
{
	if (Pa_Initialize() != paNoError) {
		std::cerr << "Critical: Unable to initialize PortAudio\n";
		QCoreApplication::instance()->exit(-1);
	}

	atexit(terminate);
}



int SoundEngine::openDefaultOutput()
{
	// hardcoded stereo, hardcoded 44100 sample rate
	PaError err = Pa_OpenDefaultStream(&stream, 0, 2, paFloat32, 44100, paFramesPerBufferUnspecified, callback, NULL);
	if (err != paNoError) {
		stream = NULL;
		return err;
	}
	return paNoError;
}


QString SoundEngine::errorString(int err)
{
	return Pa_GetErrorText(err);
}
