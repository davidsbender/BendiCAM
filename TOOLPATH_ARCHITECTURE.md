# BendiCAM Toolpath Generation Architecture & Implementation Guide

## 1. Executive Summary & Architectural Diagnosis

### The Problem
When exporting G-code for CNC machining, BendiCAM generates excessively large `.cnc` files containing thousands of lines of linear micro-segment moves (`G1 X... Y...`), even for simple shapes with straight lines and circular arcs.

### The Root Cause
BendiCAM currently employs an **early-tessellation** (point-cloud approximation) approach to toolpath generation:
1. **Premature Discretization:** `getShapeClosedPaths` converts exact CAD entities (DXF `ARC`, `CIRCLE`, `POLYLINE`) into dense 2D point arrays (`std::vector<DxfPoint>`).
2. **Vertex-by-Vertex Offsetting:** Tool compensation (`offsetPolygon`) offsets every vertex of the point cloud individually rather than shifting geometric primitives.
3. **Linear Micro-Segment Export:** `emitLoopGCode` outputs every segment of the offset point cloud as an individual `G1` motion command.

### The Solution
Re-architect the toolpath engine to operate **analytically**:
* Preserve exact CAD primitives (`Line` and `Arc`) throughout path creation, offset calculation, stepover generation, and G-code export.
* Emit native $G1$ (linear) and $G2 / G3$ (circular arc) motion commands.
* Correctly handle concave geometry, tight openings, narrow bottlenecks, and tool self-intersections.

---

## 2. Analysis of Current Flaws

| Feature | Current Point-Cloud Approach | Analytical CAM Approach |
| :--- | :--- | :--- |
| **Path Representation** | Vectors of discrete 2D points (`std::vector<DxfPoint>`) | Ordered list of geometric primitives (`LineSegment`, `ArcSegment`) |
| **Tool Offsetting** | Vertex miter shifting on discrete points | Geometric parallel line & concentric arc offsetting with corner fillets |
| **G-Code Commands** | 30–50 lines of `G1` micro-moves per arc | 1 line of `G2` or `G3` per arc segment |
| **Handling Tight Features** | Produces self-crossing "bowtie" loops & gouges | Trims intersections, splits pinched loops, avoids gouging |
| **File Size** | Hundreds of KB / Thousands of lines | Few KB / Tens of lines |
| **CNC Machine Performance** | Controller look-ahead buffer starvation, micro-stuttering | Smooth, constant-velocity axis motion |

---

## 3. Target Architecture: Analytical Toolpath Engine

### 3.1 Primitive Data Structures
Define analytical primitive types that explicitly retain curvature and center information:

```cpp
enum class PrimitiveType {
    LINE,
    ARC
};

struct PathPrimitive {
    PrimitiveType type = PrimitiveType::LINE;
    DxfPoint startPt;
    DxfPoint endPt;
    
    // ARC-specific properties
    DxfPoint center;
    double radius = 0.0;
    bool isCCW = true;
};

using AnalyticalPath = std::vector<PathPrimitive>;
```

---

## 4. Analytical Offset & Toolpath Algorithms

### 4.1 Offsetting Analytical Primitives

#### 1. Straight Line Offset (`LINE`)
For a line segment from $\mathbf{P}_1$ to $\mathbf{P}_2$ offset by distance $d$ (positive = left/outer, negative = right/inner):
* Compute unit tangent vector: $\mathbf{T} = \frac{\mathbf{P}_2 - \mathbf{P}_1}{\|\mathbf{P}_2 - \mathbf{P}_1\|} = (t_x, t_y)$
* Compute left normal vector: $\mathbf{N} = (-t_y, t_x)$
* Offset endpoints:
  $$\mathbf{P}'_1 = \mathbf{P}_1 + d \cdot \mathbf{N}$$
  $$\mathbf{P}'_2 = \mathbf{P}_2 + d \cdot \mathbf{N}$$

#### 2. Circular Arc Offset (`ARC`)
For an arc centered at $\mathbf{C}$ with radius $R$, start point $\mathbf{P}_1$, end point $\mathbf{P}_2$, and direction `isCCW`:
* The center $\mathbf{C}$ remains unchanged.
* If offsetting outwards (left of motion):
  * For CCW arcs: New radius $R' = R + d$
  * For CW arcs: New radius $R' = R - d$
* Endpoint offset:
  $$\mathbf{P}'_1 = \mathbf{C} + \frac{R'}{R}(\mathbf{P}_1 - \mathbf{C})$$
  $$\mathbf{P}'_2 = \mathbf{C} + \frac{R'}{R}(\mathbf{P}_2 - \mathbf{C})$$

#### 3. Convex Corner Fillets (Roll-Around Arcs)
When connecting two adjacent offset primitives $\text{Prim}_A$ and $\text{Prim}_B$ at a convex corner:
* Insert a connecting `ARC` primitive centered at the original vertex $\mathbf{V}$, with radius equal to the tool radius $r_{\text{tool}}$, sweeping from $\text{Prim}_A.\text{endPt}$ to $\text{Prim}_B.\text{startPt}$.

---

### 4.2 Handling Tight Openings, Concave Features & Bottlenecks

When offsetting geometry inwards (for pockets) or outwards (for outer profile cuts), tight concave features present three geometric challenges:

```
    Convex Corner (Roll-around arc)           Concave Corner (Trimmed intersection)
            +---------+                                 +         +
            |  Tool   |                                  \       /
            |  Path   |                                   \  *  /  <- Intersect & Trim
            +----*----+                                    \   /
                / \                                         \ /
               /   \                                         +
              /     \                                    Original CAD
          Original CAD                                   Boundary
```

#### Desired Behavior for Tight / Concave Geometry:

1. **Concave Corner Trimming (Corner Inset Arc / Intersection Point):**
   * At an internal concave corner, adjacent offset primitives cross over each other.
   * **Desired Behavior:** Compute the exact intersection point of the two offset primitives ($\text{Line}\cap\text{Line}$, $\text{Line}\cap\text{Arc}$, or $\text{Arc}\cap\text{Arc}$). Trim both primitives at their intersection point. The tool rolls cleanly into the corner up to the point of tangency without over-cutting or gouging the part walls.

2. **Narrow Openings & Un-machinable Features ($W_{\text{feature}} < 2 \cdot r_{\text{tool}}$):**
   * If a slot, hole, or channel width $W$ is smaller than the tool diameter ($2 \cdot r_{\text{tool}}$), the cutter physically cannot fit inside.
   * **Desired Behavior:** The offset path in that feature completely vanishes or collapses. The CAM engine automatically skips generating toolpaths inside the un-machinable feature while continuing to clear the rest of the reachable pocket.

3. **Bottleneck Pinch-off & Path Splitting:**
   * When a pocket has a narrow neck connecting two larger sub-pockets, as inward stepover rings progress, the offset path pinches together at the neck.
   * **Desired Behavior:** Detect self-intersection at the neck. Split the single parent loop into **two independent closed sub-loops** (sub-pockets). Each sub-pocket is then cleared independently in subsequent inward passes until fully machined.

4. **Self-Intersection Pruning (Elimination of Bowtie Loops):**
   * Simple point miter offsets create self-crossing "figure-eight" or "bowtie" loops when features collapse.
   * **Desired Behavior:** Any self-crossing loops are detected using planar curve intersection algorithms and trimmed away. Only valid, non-self-intersecting closed loops lying strictly inside the machinable region are retained.

---

## 5. Step-by-Step Implementation Instructions

### Step 1: Extract Analytical Paths from DXF Entities
Create a function `getShapeAnalyticalPaths(const CamShape& shape)`:
```cpp
std::vector<AnalyticalPath> BendiCAMFrame::getShapeAnalyticalPaths(const CamShape& shape) const {
    // 1. Group entities in shape into connected chains.
    // 2. For DXF LINE: create LINE primitive (startPt, endPt).
    // 3. For DXF ARC: create ARC primitive (startPt, endPt, center, radius, isCCW).
    // 4. For DXF CIRCLE: create two 180-degree ARC primitives (or single 360-degree ARC).
    // 5. Return list of analytical closed loops.
}
```

### Step 2: Implement Analytical Path Offsetting with Intersection Trimming
Create `std::vector<AnalyticalPath> offsetAnalyticalPath(const AnalyticalPath& path, double offsetDistance)`:

```cpp
std::vector<AnalyticalPath> offsetAnalyticalPath(const AnalyticalPath& path, double dist) {
    AnalyticalPath rawOffset;

    // 1. Offset each primitive individually
    for (size_t i = 0; i < path.size(); ++i) {
        PathPrimitive prim = path[i];
        PathPrimitive offsetPrim = offsetSinglePrimitive(prim, dist);

        // Check corner angle with next primitive
        PathPrimitive nextPrim = path[(i + 1) % path.size()];
        bool isConvex = checkCornerConvexity(prim, nextPrim, dist);

        if (isConvex) {
            // Convex corner: Add offset primitive + insert roll-around fillet arc
            rawOffset.push_back(offsetPrim);
            PathPrimitive fillet = createRollAroundArc(prim, nextPrim, offsetPrim, dist);
            rawOffset.push_back(fillet);
        } else {
            // Concave corner: Trim offsetPrim with next offsetPrim at intersection
            PathPrimitive nextOffsetPrim = offsetSinglePrimitive(nextPrim, dist);
            DxfPoint intersectPt;
            if (findPrimitiveIntersection(offsetPrim, nextOffsetPrim, intersectPt)) {
                offsetPrim.endPt = intersectPt;
            }
            rawOffset.push_back(offsetPrim);
        }
    }

    // 2. Detect self-intersections (bottlenecks / pinch-offs)
    std::vector<AnalyticalPath> validLoops = pruneSelfIntersectionsAndSplit(rawOffset);

    return validLoops;
}
```

### Step 3: Implement Loop Splitting for Bottlenecks & Pinch-Offs
```cpp
std::vector<AnalyticalPath> pruneSelfIntersectionsAndSplit(const AnalyticalPath& path) {
    std::vector<AnalyticalPath> resultLoops;
    
    // Find internal self-intersections between non-adjacent primitives
    std::vector<IntersectionPair> intersections = findSelfIntersections(path);

    if (intersections.empty()) {
        // No self-intersections: loop is clean
        if (getAnalyticalPathArea(path) > 1e-4) {
            resultLoops.push_back(path);
        }
        return resultLoops;
    }

    // Self-intersection detected (pinch-off at bottleneck):
    // Split the single path into two separate closed loops at the intersection point
    AnalyticalPath loop1, loop2;
    splitPathAtIntersection(path, intersections[0], loop1, loop2);

    // Recursively process each sub-loop
    auto clean1 = pruneSelfIntersectionsAndSplit(loop1);
    auto clean2 = pruneSelfIntersectionsAndSplit(loop2);

    resultLoops.insert(resultLoops.end(), clean1.begin(), clean1.end());
    resultLoops.insert(resultLoops.end(), clean2.begin(), clean2.end());

    return resultLoops;
}
```

### Step 4: Analytical Concentric Pocketing Ring Generation
Generate concentric pocketing rings analytically while supporting loop splitting:

```cpp
std::vector<AnalyticalPath> generatePocketRings(const AnalyticalPath& outerBoundary, double toolRad, double stepoverDist) {
    std::vector<AnalyticalPath> allRings;
    
    // Active front of pocket loops (can expand to multiple loops if split)
    std::vector<AnalyticalPath> activeLoops = offsetAnalyticalPath(outerBoundary, -toolRad);

    double currentOffset = -toolRad - stepoverDist;

    while (!activeLoops.empty()) {
        std::vector<AnalyticalPath> nextActiveLoops;

        for (const auto& loop : activeLoops) {
            allRings.push_back(loop);

            // Compute next inward offset ring for this loop
            std::vector<AnalyticalPath> offsetRings = offsetAnalyticalPath(loop, -stepoverDist);

            for (const auto& offsetRing : offsetRings) {
                if (getAnalyticalPathArea(offsetRing) > 1e-3) {
                    nextActiveLoops.push_back(offsetRing);
                }
            }
        }

        activeLoops = nextActiveLoops;
    }

    return allRings;
}
```

### Step 5: Analytical G-Code Export (`emitAnalyticalLoopGCode`)
Emit native $G1$ and $G2/G3$ commands directly:

```cpp
void emitAnalyticalLoopGCode(std::ofstream& out, const AnalyticalPath& path, 
                            double prevZ, double curZ, double feedZ, double feedXY, 
                            bool isRamping) 
{
    if (path.empty()) return;
    char buf[256];

    // Rapid to start point
    snprintf(buf, sizeof(buf), "G0 X%.3f Y%.3f\n", path[0].startPt.x, path[0].startPt.y); 
    out << buf;

    // Ramping or plunge move
    if (isRamping) {
        snprintf(buf, sizeof(buf), "G1 Z%.3f F%.3f\n", prevZ, feedZ); out << buf;
        
        // Ramping pass over primitives
        double totalLen = getPathLength(path);
        double cumLen = 0.0;
        for (const auto& prim : path) {
            cumLen += getPrimitiveLength(prim);
            double pz = prevZ + (curZ - prevZ) * (cumLen / totalLen);
            emitPrimitiveGCode(out, prim, pz, feedZ);
        }
    } else {
        snprintf(buf, sizeof(buf), "G1 Z%.3f F%.3f\n", curZ, feedZ); out << buf;
    }

    // Flat cleanup pass at curZ with feedXY
    snprintf(buf, sizeof(buf), "G1 F%.3f\n", feedXY); out << buf;
    for (const auto& prim : path) {
        emitPrimitiveGCode(out, prim, curZ, feedXY);
    }
}

void emitPrimitiveGCode(std::ofstream& out, const PathPrimitive& prim, double z, double feed) {
    char buf[256];
    if (prim.type == PrimitiveType::LINE) {
        snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n", 
                 prim.endPt.x, prim.endPt.y, z, feed);
        out << buf;
    } else if (prim.type == PrimitiveType::ARC) {
        double I = prim.center.x - prim.startPt.x;
        double J = prim.center.y - prim.startPt.y;
        const char* cmd = prim.isCCW ? "G3" : "G2";
        snprintf(buf, sizeof(buf), "%s X%.3f Y%.3f Z%.3f I%.3f J%.3f F%.3f\n",
                 cmd, prim.endPt.x, prim.endPt.y, z, I, J, feed);
        out << buf;
    }
}
```

---

## 6. Summary of Desired Behavior for Special Features

| Geometry Condition | Desired Toolpath Behavior |
| :--- | :--- |
| **Convex Corners ($>180^\circ$ exterior)** | Roll-around arc of radius $r_{\text{tool}}$ inserted around corner vertex. |
| **Concave Corners ($<180^\circ$ exterior)** | Adjacent offset primitives trimmed at intersection; cutter rolls cleanly up to corner tangency. |
| **Slot/Feature Smaller than Tool ($W < 2 \cdot r_{\text{tool}}$)** | Feature is automatically skipped; no toolpath generated inside un-machinable zone. |
| **Pinch-Off Bottlenecks ($W_{\text{neck}} \approx 2 \cdot r_{\text{tool}}$)** | Single parent loop splits into two independent sub-pocket loops. |
| **Self-Crossing / Bowtie Loops** | Self-intersection pruning clips away inverted loops, preventing part gouging. |

---

## 7. Expected Results & Metrics

| Metric | Point-Cloud Method | Analytical Engine | Improvement |
| :--- | :--- | :--- | :--- |
| **G-Code Output Lines** | ~1,500 – 5,000 lines | ~40 – 120 lines | **~97% Reduction** |
| **File Size (`.cnc`)** | ~150 KB | ~4 KB | **~97% Reduction** |
| **Arc Motion Accuracy** | Discretized chordal error | Exact mathematical arc | **Zero Approximation Error** |
| **Gouging Avoidance** | High risk in narrow features | Guaranteed zero gouging | **Safe & Reliable Toolpaths** |
| **Machine Execution** | Buffer starvation / axis vibration | Smooth constant-velocity motion | **Optimal Machine Performance** |
