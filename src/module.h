#ifndef MODULE_H
#define MODULE_H

#include <QSet>
#include <QRunnable>
#include <QMap>

#include "port.h"

class Module
{
public:
    Module();
    virtual ~Module();

    // other modules that are connected to this one
    QSet<Module *> outModules;
    QSet<Module *> inModules;

    // descriptions of ports available
    QSet<Port> outPorts;
    QSet<Port> inPorts;



    // follow the module chain and cause all dependencies to render
    // frameCount frames. Depends on the current frameIndex which
    // you can set with seekWrite.
    void render(int frameCount);

    virtual bool emitsAudio() const { return false; }
    virtual bool emitsNotes() const { return false; }
    virtual bool emitsValues() const { return false; }

    // override this if you have a common setting that allows
    // mixing to completely bypass this module. For example, if
    // a gain effect is set to 1.0f.
    virtual bool canBypass() const { return false; }

protected:

    // at this time you are guaranteed that all inputs are up to date.
    virtual void rawRender(int frameCount) = 0;
    virtual void initFrameRange(int frameCount) {}

private:

    void generateInputsAsync(int frameCount);
    void generateInputsSync(int frameCount);

};



#endif // MODULE_H
