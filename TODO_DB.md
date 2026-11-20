- [x] In file save dialogs, suggest a file name in those cases:
  - If a drawing <drawing>.dxf was opened, suggest <drawing>.bcam for project save and <drawing>.cnc for GCODE export.
  - If a project <project>.bcam was opened or saved, suggest <project>.cnc for GCODE export. This has priority over <drawing>.cnc.
- [x] Not all UI elements appear in WxSmith. Verify and correct WxSmith project files.
- [x] Fix GCODE export. See square.bcam, square.csv, square.cnc.
  The part consists of two extruded squares, but the GCODE is for circles.

- For circles and arcs, use G2/G3 IJK CNC commands for circular interpolation on the machine.

- Render circles, arcs, ellipses, splines etc. as such in the drawing.
  For the 3D Part View, the current implementation with linear sections is fine, keep it as is.
