# BendiCAM

The **BendiCAM** is a custom Windows calibration and status utility for the Bendi-Lenki Gamecontroller (Vendor ID `0x04D8`, Product ID `0x0057`). It provides graphical real-time visualizations and updates physical direct input calibration curves inside the Windows Registry.

---

## Technical Details & Architecture

### Core Files
- **`BendiCAMMain.h` & `BendiCAMMain.cpp`**: Main UI window frame, raw input message processor, event router, and timer polling callback logic.
- **`JoystickCalibration.h` & `src/JoystickCalibration.cpp`**: Low-level abstraction representing axis calibration details (Calib A, Calib B, Calib C) and operations to read/write binary data from/to the Windows Registry.
- **`include/SteeringWheelCtrl.h`**: Renders the graphical steering wheel representation of the X-axis position.
- **`include/PositionBarCtrl.h`**: Generates a horizontal or vertical visual indicator bar displaying relative axis positions.
- **`wxsmith/BendiCAMframe.wxs`**: wxSmith project layout file that defines the GUI structure in Code::Blocks.

### Registry Paths & Calibration Mapping
DirectInput axis calibration values are stored as 12-byte binary structures (representing three 4-byte integers: `min`, `center`, and `max`) in the registry under:
`HKEY_CURRENT_USER\System\CurrentControlSet\Control\MediaProperties\PrivateProperties\DirectInput\VID_04D8&PID_0057\Calibration\0\Type\Axes\<index>`

Where:
- **Index `0`**: X-Axis (Steering Wheel)
- **Index `1`**: Y-Axis (Brake Pedal)
- **Index `2`**: Z-Axis (Throttle Pedal)

Writing to these registry keys changes how Windows scales raw input from the controller. To apply calibration changes globally immediately without reconnecting the device, the configurator triggers:
```cpp
joyConfigChanged(0);
```
It then temporarily re-registers raw input devices using the Windows Raw Input API (`RegisterRawInputDevices`) to refresh system-wide configurations.

---

## Axis-Specific Calibration Details

### X Axis (Steering Wheel)
- **Calibrate Full Left**: Binds the lower end of the X range (`min` / `Calib A`) to the current raw steering position.
- **Calibrate Center**: Binds the midpoint (`center` / `Calib B`) to the current raw position.
- **Calibrate Full Right**: Binds the upper end of the X range (`max` / `Calib C`) to the current raw steering position.

### Y Axis (Brake Pedal)
*Note: The Y-axis UI contains no dedicated "Center" button.*
- **"Up" / "Down" physical labels**: Refer directly to physical positions of the pedal.
- **Calibrate Up**: Updates the lower end of the value range (`min` / `Calib A`) with the current raw Y position.
- **Calibrate Down**: Updates the upper end of the value range (`max` / `Calib C`) with the current raw Y position.
- **Automatic Center-Calculation**: The center calibration value (`center` / `Calib B`) is always automatically calculated as the exact middle point between the minimum and maximum calibration limits:
  $$\text{Calib B} = \frac{\text{Calib A} + \text{Calib C}}{2}$$

### Z Axis (Throttle Pedal - Inverted Hardware Configuration)
*Note: Due to inverted hardware potentiometer characteristics on the Z axis, the logic is adapted to ensure natural physical control mapping.*
- **Calibrate Up**: Calibrates the **upper end** of the value range (`max` / `Calib C`) using the current raw Z value.
- **Calibrate Down**: Calibrates the **lower end** of the value range (`min` / `Calib A`) using the current raw Z value.
- **Automatic Center-Calculation**: Exactly like the Y-axis, the center calibration value is automatically calculated as the exact midpoint:
  $$\text{Calib B} = \frac{\text{Calib A} + \text{Calib C}}{2}$$
- **Inverted Display Indicator**: The graphical indicator bar (`positionBarCtrlZ`) displays the inverted relative position computed as:
  $$\text{Display Position} = 65535 - \text{Raw Position}$$
  This maps the physical pedal deflection directly to a natural, positive indicator increase (UP deflection fills the bar, DOWN empties it).

---

## How to Compile & Build

The project is structured for **Code::Blocks** with the **MinGW** compiler toolchain and **wxWidgets 3.3.x** library.

### Option A: From Code::Blocks IDE
1. Open the project workspace `BendiCAM.cbp` in Code::Blocks.
2. Select the `Release` target from the build target dropdown menu.
3. Use the wxSmith resource editor (or build commands directly) to construct and compile.

### Option B: Command Line (using cbp2make & mingw32-make)
1. Ensure wxWidgets is extracted or built at `C:\wxWidgets-3.3.1` (or set `WX` environment variable).
2. Ensure MinGW is available under system path.
3. Compile using the included Windows Makefile:
   ```powershell
   $env:WX = "C:\wxWidgets-3.3.1"
   $env:PATH = "C:\Program Files\CodeBlocks\MinGW\bin;" + $env:PATH
   mingw32-make -f Makefile.win release
   ```

---

## Future Extensibility Guide

To add a new axis or a new button control:
1. **Define the Layout**: Open `wxsmith/BendiCAMframe.wxs` or add standard wxWidgets controls under the target static box container. Ensure all coordinates and size bounds are aligned.
2. **Setup Identifiers & Declarations**:
   - In `BendiCAMMain.h`, add unique `wxWindowID` values within the `//(*Identifiers` block.
   - Declare any pointer to standard wxWidgets structures within the `//(*Declarations` block.
3. **Register Raw Inputs**:
   - Inside `BendiCAMMain.cpp`'s `MSWWindowProc` function, retrieve the target Usage Page and Usage ID value of the target axis or button using the Windows HID interface (`HidP_GetUsageValue`).
4. **Update Polling Callback**:
   - In `OnTimerTrigger`, request and fetch the corresponding coordinate value (`joyInfo.dwZpos`, `joyInfo.dwRpos`, `joyInfo.dwUpos`, or `joyInfo.dwVpos`) via standard Multimedia SDK methods.
5. **Update Registry Calibrator**:
   - To read/write calibration configs for the new axis, instantiate `JoystickCalibration` with the correct axis index offset parameter (e.g., `3` for R-axis / Rudder) and invoke `.write(...)`.
