#include "lv2module.h"

Lv2Module::Lv2Module(const LilvPlugin *plugin, int sampleRate) :
    plugin(plugin)
{
    instance = lilv_plugin_instantiate(plugin, sampleRate, NULL);
    lilv_instance_activate(instance);
}

Lv2Module::~Lv2Module()
{
    lilv_instance_deactivate(instance);
    lilv_instance_free(instance);
}

void Lv2Module::rawRender(int frameCount)
{
    lilv_instance_run(instance, frameCount);
}
