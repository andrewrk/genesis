#include "module.h"

#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>
#include <QVector>

Module::Module()
{
}

Module::~Module()
{
}

void Module::render(int frameCount)
{
    if (inModules.size() > 1) {
        generateInputsAsync(frameCount);
    } else {
        generateInputsSync(frameCount);
    }
    rawRender(frameCount);
}

void Module::generateInputsAsync(int frameCount)
{
    QVector<QFuture<void>> futures(0);
    futures.reserve(inModules.size());
    foreach(Module *module, inModules) {
        futures.append(QtConcurrent::run(module, &Module::render, frameCount));
    }
    initFrameRange(frameCount);
    foreach(QFuture<void> future, futures) {
        future.waitForFinished();
    }
}

void Module::generateInputsSync(int frameCount)
{
    foreach(Module *module, inModules) {
        if (module->canBypass()) continue;
        module->render(frameCount);
    }
    initFrameRange(frameCount);
}

