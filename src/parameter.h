#ifndef PARAMETER_H
#define PARAMETER_H

#include <QString>

class Parameter
{
public:
    Parameter(QString name);
    Parameter(QString name, float min, float max, float defaultValue);

    void setMin(float min);
    void setMax(float max);
    void setDefault(float value);
    void setInt(bool value);
private:
    float min = 0.0f;
    float max = 1.0f;
    float defaultValue = 0.5f;
    QString name;
    bool isInt = false;

};

#endif // PARAMETER_H
