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

---

## Dual-Role CAM Geometry Architecture

In 2.5D machining, CAD shapes are inherently **role-fluid**. A CAD shape (`CamShape`) is a purely geometric entity defined by closed DXF entity loops and an extrusion depth. In CAM operations, however, a shape takes on distinct roles depending on the machining context:

```
                  ┌───────────────────────────────────────────┐
                  │            CamShape (CAD Geometry)        │
                  │       (id, depth, DXF entity indices)     │
                  └─────────────────────┬─────────────────────┘
                                        │
                         Intrinsic Path Generation
                       (toolDiameter, overcutPercent)
                                        │
                  ┌─────────────────────┴─────────────────────┐
                  │                 ShapePaths                │
                  │   ┌───────────────────┬────────────────┐  │
                  │   │    IslandPaths    │ MaterialPaths  │  │
                  │   │   (Obstacle/Boss) │ (Floor/Stock)  │  │
                  │   └───────────────────┴────────────────┘  │
                  └─────────────────────┬─────────────────────┘
                                        │
                           Context-Driven Usage
                                        │
           ┌────────────────────────────┴────────────────────────────┐
           ▼                                                         ▼
┌──────────────────────────────┐          ┌──────────────────────────────────────┐
│     Part Definition Mode     │          │            Machining Mode            │
│  - Focused Shape -> Island   │          │  - Floor Shape   -> Material         │
│  - Below Shape   -> Material │          │  - Level Shapes  -> Array of Islands │
│  - Siblings      -> Islands  │          └──────────────────────────────────────┘
└──────────────────────────────┘
```

For example, in a multi-tiered part with a Base Plate, Lower Pocket, and Raised Island:
- When cutting the pocket down to the Base Plate, the Base Plate is the **Material (Floor)**.
- When contouring out the Base Plate itself, it acts as the **Shape / Island**.
- The Lower Pocket acts as an **Island** relative to the Base Plate, but becomes the **Material (Floor)** for features milled on top of it.

### 1. Dual-Role Sub-Structures

Because all 7 analytical paths depend strictly on the shape's boundary and the tool configuration (`toolDiameter`, `overcutPercent`), a shape's paths are intrinsic to the shape itself and are computed once:

#### A. `IslandPaths` (Role: Obstacle / Boss to Preserve)
When a shape rises above a floor, the cutter must clear material around it while preserving the shape:
* **`outline`**: The nominal unoffset CAD boundary loop.
* **`toolPath`**: Offset inward by tool radius ($r = D/2$).
* **`cutZone`**: Offset inward by tool diameter ($D$), defining the boundary of material cleared by the toolpath.

#### B. `MaterialPaths` (Role: Floor / Stock to Clear)
When a shape serves as the floor platform being cleared down to, the cutter operates within its boundaries:
* **`outline`**: The nominal unoffset CAD boundary loop.
* **`toolPath`**: Offset inward or outward based on the overcut setting ($E = (\text{overcut}\% / 100 - 0.5) \times D$). When $\text{overcut}\% < 50\%$, automatic out-and-back bisector spurs are added at concave corners to eliminate uncut cusps.
* **`cutZoneInner`**: Un-spurred toolpath offset inward by tool radius ($r$).
* **`cutZoneOuter`**: Un-spurred toolpath offset outward by tool radius ($r$), with apex arcs around corner spurs.

#### C. `ShapePaths` (Complete Dual-Role Package)
Groups the source CAD shape pointer, shape ID, and both role structures:
```cpp
struct ShapePaths {
    int shapeId = -1;
    const CamShape* shape = nullptr;
    IslandPaths   island;    // 3 paths when acting as an Island / Boss
    MaterialPaths material;  // 4 paths when acting as Material / Floor
};
```

---

### 2. Relational Level & Processing Representation (Zero Redundancy)

Because shapes own their dual-role toolpaths, operations and machining levels no longer copy or recompute geometry—they simply act as relational coordinators using non-owning pointers:

```cpp
// Machining Level: 1 Material Floor + N Islands
struct MachiningLevelPaths {
    int levelIndex = -1;
    double depth = 0.0;
    int belowShapeId = -1;

    const ShapePaths* material = nullptr;       // The floor shape (nullptr for raw stock)
    std::vector<const ShapePaths*> islands;     // All shapes/islands on this level
};

// Part Definition: Single Shape Focus
struct SelectedShapeProcessingPaths {
    const ShapePaths* shape = nullptr;           // Primary selected shape (Island role)
    const ShapePaths* material = nullptr;        // Shape below (Material role)
    std::vector<const ShapePaths*> islandShapes; // Sibling obstacles on same floor (Island role)
};
```

---

### 3. Viewport Coloring & Visual Hierarchy

The 2D drawing viewport automatically adapts its rendering depending on whether the user is in **Part Definition** or **Machining** mode:

| Mode | Visual Element | Role | Pen Color | Palette |
| :--- | :--- | :--- | :--- | :--- |
| **Machining Mode** | Floor Clearing Toolpath | Infill Passes (Background) | Light Pastel Slate Blue | `wxColour(175, 195, 215)` |
| | Material Outline / Cut Zones | Material Floor | Dark Amber (`#E66400`) | `wxColour(230, 100, 0)` |
| | Material Toolpath | Material Floor | Dark Blue (`#0000B4`) | `wxColour(0, 0, 180)` |
| | Level Shapes Outline / Cut Zones | Islands | Light Amber (`#FFAF28`) | `wxColour(255, 175, 40)` |
| | Level Shapes Toolpath | Islands | Light Blue (`#4182F5`) | `wxColour(65, 130, 245)` |
| | Required Coverage Outer Boundary | Included Target Floor | Solid Vibrant Blue | `wxColour(0, 120, 230)` |
| | Required Coverage Hole Boundary | Excluded Boss Cutout | Solid Vibrant Red | `wxColour(220, 50, 50)` |
| | Allowed Envelope Outer Boundary | Air Roaming Limit | Dashed Soft Cyan | `wxColour(80, 180, 240)` |
| | Allowed Envelope Hole Boundary | Boss Avoidance Limit | Dashed Soft Red | `wxColour(230, 80, 80)` |
| | Derived Pocket Outer Boundary | Legacy Cleared Floor | Solid Vibrant Blue | `wxColour(0, 140, 240)` |
| | Derived Pocket Island Boundary | Legacy Boss Limit | Solid Warm Amber | `wxColour(240, 120, 0)` |
| **Part Definition Mode** | Primary Shape Outline / Cut Zones | Selected Island | Dark Amber (`#E66400`) | `wxColour(230, 100, 0)` |
| | Primary Shape Toolpath | Selected Island | Dark Blue (`#0000B4`) | `wxColour(0, 0, 180)` |
| | Material Base Outline / Cut Zones | Floor | Dark Amber (`#E66400`) | `wxColour(230, 100, 0)` |
| | Material Base Toolpath | Floor | Dark Blue (`#0000B4`) | `wxColour(0, 0, 180)` |
| | Sibling Shapes Outline / Cut Zones | Sibling Islands | Bright Amber (`#FFAF28`) | `wxColour(255, 175, 40)` |
| | Sibling Shapes Toolpath | Sibling Islands | Bright Blue (`#4182F5`) | `wxColour(65, 130, 245)` |

* Circular vertex dots are rendered at every segment and arc endpoint, matching the stroke color of their respective paths for instant geometric inspection.

---

### 4. Analytical Boolean Subtraction & Concrete Machining Paths

To generate final milling paths for each level, the system computes the 2D boolean difference:
$$\text{Clearing Toolpath Area} = \text{Material Outline Toolpath} \setminus \bigcup_{i} \text{Boss Toolpaths}_i$$

Because boolean operations create composite geometry with newly split line/arc segments and changing topology, the clearing boundaries are concrete, owned structures rather than pointers.

#### A. Architecture Modules
* **`AnalyticalBoolean` (`include/AnalyticalBoolean.h`, `src/AnalyticalBoolean.cpp`)**:
  - Multi-root analytical intersections for Line–Line (0–1), Line–Arc (0–2), and Arc–Arc (0–2) without line discretization.
  - Parametric primitive splitting at $t \in (0, 1)$ preserving corner spur metadata.
  - Weiler-Atherton directed traversal for true analytical curve boolean difference ($A \setminus B$).
* **`MachiningPaths` (`include/MachiningPaths.h`, `src/MachiningPaths.cpp`)**:
  - `MachiningPocket`: Explicit domain representation with `outerBoundary` (CCW internal wall) and `islandBoundaries` (CW/CCW boss limits).
  - `LevelMachiningToolpaths`: Contains the resulting pockets to clear on each machining level.
  - Sequentially subtracts boss toolpaths from material floor toolpaths, correctly handling complete coverage (empty), islands strictly within floor (pockets with holes), and bisecting cuts (multiple independent pockets).

#### B. Corner Overcut Spurs as Vertex Metadata
- Rather than inserting 1D zero-area antenna lines ($V \to \text{Apex} \to V$) that violate 2D manifold topology, convex corner overcut relief is tagged directly on `PathPrimitive` via `hasEndSpur` and `endSpurApex`.
- Surviving corners automatically retain their spur motion through boolean operations; corners clipped away by islands are naturally discarded.


