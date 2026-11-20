# BendiCAM TODO

All planned features and enhancements are complete!

## Completed Features
- [x] Added graphical Steering Wheel representation (calibrated X).
- [x] Added Position Bar (calibrated X) with red dash indicator.
- [x] Implemented "Calibrate Center" functionality (Registry update + `joyConfigChanged(0)`).
- [x] UI Refactoring: Right-aligned numerical labels with monospace fonts, removed redundant "Label" text.
- [x] Replaced `wxMessageBox` for calibration status with `TextCtrl1` logging.
- [x] Fixed UI design issues caused by `wxsmith` overriding code changes.
- [x] Implemented "Calibrate Full Left" functionality for the X axis.
- [x] Implemented "Calibrate Full Right" functionality for the X axis.
- [x] Implemented "Calibrate Up" and "Calibrate Down" functionality for the Y axis.
- [x] Created Z-axis UI elements within `StaticBoxZ` identical to the Y axis layout.
- [x] Implemented "Calibrate Up" and "Calibrate Down" functionality for the Z axis (designed for inverted hardware).
- [x] Provided automatic center-calculation for the Y and Z axes (exact middle point).
- [x] Updated Z axis graphical bar to display inverted position (`65535 - position`) due to reversed hardware pot configuration.
