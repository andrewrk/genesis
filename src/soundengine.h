#ifndef SOUNDENGINE_H
#define SOUNDENGINE_H

#include <QString>

namespace SoundEngine
{
	void initialize();

	int openDefaultOutput();

	QString errorString(int err);
}

#endif // SOUNDENGINE_H
