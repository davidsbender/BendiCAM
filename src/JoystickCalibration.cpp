#include "JoystickCalibration.h"
#include <vector>
#include <cstdio>
#include <cstring>

JoystickCalibration::JoystickCalibration()
{
    //ctor
}

JoystickCalibration::~JoystickCalibration()
{
    //dtor
}

void JoystickCalibration::read(int axisIndex)
{
    // Fallback or handle error - set to 0 if we can't read
    a = 0;
    b = 0;
    c = 0;

    // Build key path dynamically
    wchar_t keyPath[256];
    // The user provided registry key:
    // Computer\HKEY_CURRENT_USER\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\DirectInput\VID_04D8&PID_0057\Calibration\0\Type\Axes\1
    // The previous code had: ...\Calibration\0\Type\Axes\%d
    // This matches.
    // Let me check if there is a way to verify the key exists or get more info.

    _snwprintf(keyPath, 256, L"System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\DirectInput\\VID_04D8&PID_0057\\Calibration\\0\\Type\\Axes\\%d", axisIndex);

    LPCWSTR key = keyPath;


    DWORD bufferSize = 0;
    // 1. Get the required size first
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        key,
        L"Calibration",
        RRF_RT_REG_BINARY,
        NULL,
        NULL,
        &bufferSize
    );

    if (status == ERROR_SUCCESS && bufferSize >= 12) {
        std::vector<BYTE> buffer(bufferSize);

        // 2. Fetch the actual binary data
        status = RegGetValueW(
            HKEY_CURRENT_USER,
            key,
            L"Calibration",
            RRF_RT_REG_BINARY,
            NULL,
            &buffer[0],
            &bufferSize
        );

        if ((status == ERROR_SUCCESS) && (bufferSize >= 12)) {
            a = *(DWORD*)&buffer[0];
            b = *(DWORD*)&buffer[4];
            c = *(DWORD*)&buffer[8];
        }
    }
}

bool JoystickCalibration::write(int min, int center, int max, int axisIndex) {
    wchar_t subKey[256];
    _snwprintf(subKey, 256, L"System\\CurrentControlSet\\Control\\MediaProperties\\PrivateProperties\\DirectInput\\VID_04D8&PID_0057\\Calibration\\0\\Type\\Axes\\%d", axisIndex);

    HKEY hKey;

    // 1. Open the key with write permissions
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, subKey, 0, KEY_SET_VALUE, &hKey);


    if (status != ERROR_SUCCESS) {
        return false;
    }

    // 2. Prepare the 12-byte binary buffer (3 x 4-byte integers)
    int calibrationData[3] = { min, center, max };

    // 3. Write the value
    status = RegSetValueExW(
        hKey,
        L"Calibration",
        0,
        REG_BINARY,
        (const BYTE*)calibrationData,
        sizeof(calibrationData)
    );

    // 4. Always close the handle
    RegCloseKey(hKey);

    if (status == ERROR_SUCCESS) {
        // Update members
        a = min;
        b = center;
        c = max;
        return true;
    } else {
        return false;
    }
}
