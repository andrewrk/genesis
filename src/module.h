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


protected:

    // at this time you are guaranteed that all inputs are up to date.
    virtual void rawRender(int frameCount) = 0;

private:

};



#endif // MODULE_H
