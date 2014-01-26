#ifndef MODULE_H
#define MODULE_H

#include <QSet>

class Module
{
public:
    Module();

    QSet<Module *> parents;
    QSet<Module *> children;
};

#endif // MODULE_H
