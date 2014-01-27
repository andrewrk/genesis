#include "lv2module.h"

Lv2Module::Lv2Module(const LilvPlugin *plugin, int sampleRate) :
    plugin(plugin)
{
    instance = lilv_plugin_instantiate(plugin, sampleRate, NULL);
}

Lv2Module::~Lv2Module()
{
    lilv_instance_free(instance);
}

void Lv2Module::rawRender(int frameCount)
{

}
