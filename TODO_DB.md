# BendiCAM Feature Implementation TODO

All requested features and enhancements are complete!

## Completed Features
- [x] **Shape Color Fills in Drawing**:
  - Non-overlapping color fills in the 2D drawing view using polygon clipping regions (`wxRegion::Subtract`). Shows only the topmost (highest Z/depth) shape's color fill.
- [x] **Window Title**:
  - Displays file name (e.g., `drawing.dxf - BendiCAM`).
  - Appends an asterisk `*` when there are unsaved changes (e.g., `drawing.dxf * - BendiCAM`).
- [x] **Unsaved Changes Prompt on Close**:
  - Prompts user to save unsaved changes before closing the application (Yes / No / Cancel).
- [x] **Tool Configuration UI**:
  - Tool properties saved and restored as part of `.bcam` JSON project file:
    - Diameter: default 2 mm
    - RPM: default 12000 min^-1
    - Horizontal Feed Speed (Feed XY): default 500 mm/min
    - Vertical Feed Speed (Feed Z): default 250 mm/min
    - Stepdown: default 1 mm
    - Stepover: default 70 %
    - Ramp Angle: default 20°
- [x] **Material Top Height UI**:
  - Material top height input in mm, default 10 mm. Saved in project JSON (`material.topHeight`).
- [x] **Safe Z UI**:
  - Safe Z height input in mm, default 10 mm. Saved in project JSON (`material.safeZ`).
- [x] **EdingCNC GCODE Export**:
  - Full EdingCNC standard G-code generator exported via menu (`File -> Export EdingCNC G-Code...`) and UI button.
- [x] **Milling Scheme Implementation**:
  - Shape 2 (10 mm High - Stock Height): preserved as raised island without facing/milling on top.
  - Shape 1 (5 mm High - Clear-Milling Around Shape 2): pocket clearing down to Z = 5 mm with tool radius offset around Shape 2 and perimeter finish pass.
  - Outer Contour Around Shape 1: single pass to Z = 0 mm offset outward by tool radius without extra area clearing.
