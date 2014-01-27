#include "parameter.h"

Parameter::Parameter(QString name) :
    name(name)
{
}

Parameter::Parameter(QString name, float min, float max, float defaultValue) :
    Parameter(name)
{
    setMin(min);
    setMax(max);
    setDefault(defaultValue);
}

void Parameter::setMin(float min)
{
    this->min = min;
}

void Parameter::setMax(float max)
{
    this->max = max;
}

void Parameter::setDefault(float value)
{
    this->defaultValue = value;
}

void Parameter::setInt(bool value)
{
    isInt = value;
}
