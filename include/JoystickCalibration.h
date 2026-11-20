#ifndef JOYSTICKCALIBRATION_H
#define JOYSTICKCALIBRATION_H

#include <windows.h>

class JoystickCalibration
{
    public:
        JoystickCalibration();
        virtual ~JoystickCalibration();
        void read(int axisIndex = 0);
        bool write(int min, int center, int max, int axisIndex = 0);
        unsigned int a;
        unsigned int b;
        unsigned int c;

    protected:

    private:
};

#endif // JOYSTICKCALIBRATION_H
