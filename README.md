# BendiCAM

**BendiCAM** is a 2.5D CAD/CAM application for Windows built with C++ and **wxWidgets**. It allows users to import CAD drawings (DXF), group entities into 2.5D machining parts and multi-depth shapes, visualize 2D paths and 3D extruded parts, and export optimized **EdingCNC G-code** (`.cnc`).

---

## Key Features

### 1. CAD Drawing Import & Entity Processing
- **DXF Import**: Parses AutoCAD DXF files (lines, arcs, circles, polylines, splines, ellipses, solids, points).
- **Layer Control**: Toggle layer visibility with real-time viewport and bounding box updates.
- **Entity Connection & Tolerance**: Connects coinciding endpoints into closed polyline shapes using configurable matching tolerances.

### 2. CAM Part & Shape Hierarchy
- **Parts & Shapes**: Organize DXF geometry into named CAM parts containing multiple depth shapes.
- **Interactive 2D Viewport**: Mouse pan, zoom, entity hit-testing, selection highlights, and depth-gradient color fills.
- **Interactive 3D Viewport**: Real-time 3D solid part rendering with camera orbit rotation, pan, zoom, back-to-front depth sorting, and metallic CAD surface shading.

### 3. Tooling & Material Settings
- **Cutter Parameters**: Tool diameter, spindle RPM, XY feed rate, Z feed rate, stepdown, stepover percentage, and ramp angle.
- **Material Settings**: Material top height (stock Z reference) and Safe Z retract height.

### 4. EdingCNC G-Code Export (`.cnc`)
- **Island-Preserving Clear Milling**: Clears material down to step depths around raised islands respecting cutter radius offset.
- **Stepover & Ramp Entry**: Computes stepover ring passes and helical/ramp entry down to target Z levels.
- **Outer Profile Contour Milling**: Mills single tool-radius-offset contour passes around part perimeters down to Z = 0 mm.
- **Post-Processing**: Generates EdingCNC-compatible header blocks, spindle control (`M3`/`M5`), feed rates, and program termination (`M30`).

### 5. Project Persistence (`.bcam`)
- Save and open BendiCAM project files (`.bcam` / `.json`), persisting CAD file references, layer states, part/shape depth hierarchies, and machining parameters.

---

## Project Structure

- **`BendiCAMMain.h` & `BendiCAMMain.cpp`**: Main application frame, 2D drawing canvas, 3D part viewport, tree controls, and G-code generator.
- **`include/DxfParser.h` & `src/DxfParser.cpp`**: DXF parser, layer manager, entity geometry sampler, and pathfinder.
- **`BendiCAM.cbp` & `Makefile.win`**: Code::Blocks project workspace and MinGW build configuration.
- **`examples/`**: Sample CAD drawings (`example.dxf`) and project files (`example.bcam`).

---

## How to Compile & Build

Built using **Code::Blocks** or **MinGW** with **wxWidgets 3.x**.

### Option A: Code::Blocks IDE
1. Open `BendiCAM.cbp` in Code::Blocks.
2. Select the `Release` target.
3. Click **Build and Run** (`Ctrl+F9`).

### Option B: Command Line (MinGW Make)
Run `mingw32-make` with `Makefile.win`:
```powershell
$env:PATH = "C:\Program Files\CodeBlocks\MinGW\bin;" + $env:PATH
mingw32-make -f Makefile.win release
```
