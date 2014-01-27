#ifndef LV2MODULE_H
#define LV2MODULE_H

#include "module.h"
#include <lilv/lilv.h>

class Lv2Module : public Module
{
public:
    Lv2Module(const LilvPlugin *plugin, int sampleRate);
    ~Lv2Module();

protected:

    virtual void rawRender(int frameCount) override;

private:

    const LilvPlugin *plugin;
    LilvInstance *instance;

};

#endif // LV2MODULE_H
