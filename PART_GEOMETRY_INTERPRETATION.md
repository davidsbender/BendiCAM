# BendiCAM 3D Part Model & Toolpath Interpretation Specification

## 1. Executive Summary & Paradigm Overview

In BendiCAM, a 3D **Part** is represented as a 2.5D solid constructed from **stepped shape extrusions**. Each shape in a part represents a 2D geometric domain extruded vertically from the machine table ($Z = 0.0\,\text{mm}$) up to the height specified by its `depth` field ($Z = h$).

### Fundamental Coordinates & Parameters
* **Table Reference ($Z = 0.0\,\text{mm}$)**: The bottom surface of the raw material stock resting on the CNC machine table.
* **Material Stock Top Height ($H_{\text{mat}}$)**: Total thickness of the raw stock block (`topHeight`, e.g., $10.0\,\text{mm}$), defining the stock volume $Z \in [0.0, H_{\text{mat}}]$.
* **Extrusion Heights ($h_i$)**: A shape $S_i$ with height parameter $h_i \in (0.0, H_{\text{mat}}]$ defines a solid volume extending from $Z = 0.0\,\text{mm}$ up to $Z = h_i\,\text{mm}$.
* **Material Removal (Milling)**: Milling removes raw stock from $Z = H_{\text{mat}}$ down to the target top surface height $Z = h_i$ of each feature, and clears the stepped regions between features.

---

## 2. 2D Planar Regions & Analytical Boundary Definitions

To maintain consistency with BendiCAM's analytical toolpath engine (`TOOLPATH_ARCHITECTURE.md`), geometry is represented using **2D planar regions** bounded by ordered analytical primitives (`LINE` and `ARC`).

### Definitions
* **Analytical Boundary Loop ($\partial \Omega$)**: A closed, oriented 2D curve composed of an ordered vector of `PathPrimitive` elements (`LINE` segments and circular `ARC` segments). Counter-Clockwise (CCW) orientation defines an outer boundary; Clockwise (CW) orientation defines an inner hole/island boundary.
* **Planar Region ($\Omega \subset \mathbb{R}^2$)**: The closed 2D area domain enclosed by an analytical boundary loop $\partial \Omega$.
* **Minkowski Disk ($\mathcal{B}_r$)**: A closed 2D circular disk of radius $r$ centered at the origin:
  $$\mathcal{B}_r = \{ \mathbf{p} \in \mathbb{R}^2 \mid \|\mathbf{p}\| \le r \}$$
  *(For $\delta \ge 0$, erosion is $\Omega \ominus \mathcal{B}_\delta \equiv \Omega \oplus \mathcal{B}_{-\delta}$).*
* **Cutter Centerline vs. Cutter Envelope**:
  * For a tool of diameter $D_{\text{tool}}$, the tool radius is $R_{\text{tool}} = D_{\text{tool}} / 2.0$.
  * If the tool centerline is positioned at 2D point $\mathbf{c} = (x, y)$, the **cutter envelope** (physical volume swept by the cutting edges) covers the 2D disk $\mathbf{c} \oplus \mathcal{B}_{R_{\text{tool}}}$.
  * Toolpath generation determines the locus of **tool centerline coordinates** $\mathbf{c}(t)$ such that the cutter envelope clears the intended 2D domain without gouging standing material.

---

## 3. Geometric Rules for Feature Hierarchy, Extrusion & Milling

### 3.1 Rule 1: Solid Feature Extrusion & Heights
* A shape $S_i$ with height $h_i$ defines a solid feature occupying 3D space $V_i = \Omega_i \times [0.0, h_i]$.
* Material within $\Omega_i$ must **remain standing** up to $Z = h_i\,\text{mm}$. No material removal is permitted inside $\Omega_i$ below $Z = h_i\,\text{mm}$.
* If $h_i = H_{\text{mat}}$ (e.g., $10.0\,\text{mm} = 10.0\,\text{mm}$):
  * The feature extends to the full height of the raw stock.
  * **Zero top clearing / pocket milling** is required inside $\Omega_i$, as the raw stock surface already sits at $Z = H_{\text{mat}}$.

---

### 3.2 Rule 2: Generalized Feature Hierarchy (Bosses & Pockets)

BendiCAM supports two complementary 2.5D feature topologies:

#### A. Bosses / Islands ($h_{\text{island}} > h_{\text{shelf}}$)
* A taller feature $\Omega_{\text{island}}$ (e.g., height $h_{\text{island}} = 10.0\,\text{mm}$) stands on top of a lower base platform $\Omega_{\text{shelf}}$ (e.g., height $h_{\text{shelf}} = 5.0\,\text{mm}$).
* **Nominal Containment**: The 2D region of the taller island must lie inside the base platform:
  $$\Omega_{\text{island}} \subset \text{Int}(\Omega_{\text{shelf}})$$

#### B. Pockets / Recesses ($h_{\text{floor}} < h_{\text{wall}}$)
* A lower floor feature $\Omega_{\text{floor}}$ (e.g., height $h_{\text{floor}} = 4.0\,\text{mm}$) is recessed inside a taller surrounding wall/plateau $\Omega_{\text{wall}}$ (e.g., height $h_{\text{wall}} = 10.0\,\text{mm}$).
* **Nominal Containment**: The 2D region of the pocket floor must lie inside the surrounding wall:
  $$\Omega_{\text{floor}} \subset \text{Int}(\Omega_{\text{wall}})$$

#### C. 2D Height Field Precedence & Generalized Set Formulation
To resolve feature overlap without ambiguity, a 3D Part defines a scalar **2D height field** $H(x, y)$ over the stock domain $\Omega_{\text{stock}}$ via hierarchical feature precedence:
1. Inner/nested child features override the height of enclosing parent features within their 2D footprint.
   * For a recessed pocket $\Omega_{\text{floor}} \subset \Omega_{\text{wall}}$: $H(x, y) = h_{\text{floor}}$ inside $\Omega_{\text{floor}}$, and $H(x, y) = h_{\text{wall}}$ within $\Omega_{\text{wall}} \setminus \Omega_{\text{floor}}$.
   * For a standing boss $\Omega_{\text{boss}} \subset \Omega_{\text{shelf}}$: $H(x, y) = h_{\text{boss}}$ inside $\Omega_{\text{boss}}$, and $H(x, y) = h_{\text{shelf}}$ within $\Omega_{\text{shelf}} \setminus \Omega_{\text{boss}}$.
2. For any axial clearing pass targeting floor level $Z_{\text{floor}}$ beneath upper level $Z_{\text{upper}}$ ($Z_{\text{floor}} < Z_{\text{upper}}$):
   * **Standing Obstacle Domain ($\Omega_{\text{above}}$)**: Areas where solid material must remain standing at or above $Z_{\text{upper}}$:
     $$\Omega_{\text{above}}(Z_{\text{upper}}) = \{ (x, y) \in \mathbb{R}^2 \mid H(x, y) \ge Z_{\text{upper}} \}$$
   * **Nominal 2D Clearing Region ($\Omega_{\text{clear}}$)**: The 2D domain to be cleared down to $Z_{\text{floor}}$:
     $$\Omega_{\text{clear}}(Z_{\text{floor}}, Z_{\text{upper}}) = \{ (x, y) \in \mathbb{R}^2 \mid H(x, y) \le Z_{\text{floor}} \text{ and } (x, y) \text{ lies within enclosing parent boundary} \}$$

---

### 3.3 Rule 3: Stepover vs. Burr-Prevention Open Edge Overrun

#### A. Tool Stepover ($S\%$)
* **Stepover Percentage ($S\%$)**: Configured radial distance between adjacent parallel/concentric toolpath passes, expressed as a percentage of tool diameter $D_{\text{tool}}$ (e.g., $70\%$).
* **Radial Pass Spacing ($w_{\text{step}}$)**:
  $$w_{\text{step}} = D_{\text{tool}} \cdot \frac{S\%}{100\%}$$
  For $D_{\text{tool}} = 2.0\,\text{mm}$ and $S\% = 70\%$, $w_{\text{step}} = 1.4\,\text{mm}$.

#### B. Open Edge Overrun ($\Delta r_{\text{overrun}}$)
* The boundary $\partial \Omega_{\text{clear}}$ consists of two types of edges:
  1. **Standing Obstacle Edges**: Boundaries shared with standing features $\Omega_{\text{above}}$ (e.g., walls of an island or surrounding pocket walls). The cutter envelope **must not cross** into standing material. The tool centerline must be offset inward into $\Omega_{\text{clear}}$ by at least $+R_{\text{tool}}$ from standing walls.
  2. **Open Step Edges**: Outer boundaries of lower platforms (e.g., outer edge of $\Omega_{\text{shelf}}$) where material drops off to a lower $Z$-level or open space.
* **Burr-Prevention Overrun**: To ensure no uncut ridge or burr remains along an open step edge, the cutter outer edge must extend beyond the open boundary by an overrun distance $\Delta r_{\text{overrun}}$:
  $$\Delta r_{\text{overrun}} = D_{\text{tool}} \cdot \left(1.0 - \frac{S\%}{100\%}\right)$$
  For $D_{\text{tool}} = 2.0\,\text{mm}$ and $S\% = 70\%$, $\Delta r_{\text{overrun}} = 0.6\,\text{mm}$.
* **Effective Tool Centerline Offset for Open Edges**:
  $$\text{Offset}_{\text{open}} = -R_{\text{tool}} + \Delta r_{\text{overrun}} = -(R_{\text{tool}} - \Delta r_{\text{overrun}})$$
  For $R_{\text{tool}} = 1.0\,\text{mm}$ and $\Delta r_{\text{overrun}} = 0.6\,\text{mm}$, the tool centerline is set back (eroded) by $0.4\,\text{mm}$ inside the nominal open edge, placing the outer cutter edge $0.6\,\text{mm}$ beyond the open edge.

#### C. Centerline Domain & Safety Clipping Against Standing Obstacles
* **Hard Safety Constraint**: Open edge overrun applies **strictly to open step edges** and must be **clipped against standing features** ($\Omega_{\text{above}}$).
* The allowable 2D domain for tool centerline coordinates $\mathbf{c}$ is strictly bounded by:
  $$\mathcal{D}_{\text{centerline}} = \left( \Omega_{\text{clear}} \ominus \mathcal{B}_{R_{\text{tool}} - \Delta r_{\text{overrun}}} \right) \setminus \left( \Omega_{\text{above}} \oplus \mathcal{B}_{R_{\text{tool}}} \right)$$
  *(For general $\Delta r_{\text{overrun}}$, $\mathcal{D}_{\text{centerline}} = (\Omega_{\text{clear}} \oplus \mathcal{B}_{\Delta r_{\text{overrun}} - R_{\text{tool}}}) \setminus (\Omega_{\text{above}} \oplus \mathcal{B}_{R_{\text{tool}}})$).*
* This formula guarantees that the tool outer edge sweeps fully $\Delta r_{\text{overrun}}$ past open step edges while maintaining a strict zero-gouging clearance against standing material of higher features.

---

### 3.4 Rule 4: Axial Stepdown ($\Delta Z$) & Z-Pass Structuring

When clearing material in the height band between $Z_{\text{upper}}$ and $Z_{\text{floor}}$:
* **Axial Stepdown ($\Delta Z$)**: Maximum material depth cut in a single Z-pass (`stepdown`, e.g., $1.0\,\text{mm}$).
* **Pass Level Calculation**:
  1. Calculate the number of equal or sub-stepdown axial passes:
     $$N_{\text{passes}} = \left\lceil \frac{Z_{\text{upper}} - Z_{\text{floor}}}{\Delta Z} \right\rceil$$
  2. Discrete pass depths $Z_k$ for $k = 1, 2, \dots, N_{\text{passes}}$ are calculated as:
     $$Z_k = Z_{\text{upper}} - k \cdot \frac{Z_{\text{upper}} - Z_{\text{floor}}}{N_{\text{passes}}}$$
* **Ramping Entry**: Each Z-pass plunges into material along a helical or linear ramp at the configured `rampAngle` (e.g., $20^\circ$) at plunge feed rate `feedZ`, before executing 2D planar clearing at feed rate `feedXY`.

---

### 3.5 Rule 5: Outer Profile Cutout Pass & Stock Height Alignment

* **Base Part Boundary ($\Omega_{\text{base}}$)**: The shape with the lowest extrusion height $h_{\text{base}}$ (or the outermost boundary shape at $h_{\text{base}}$) defines the finished 2D perimeter of the part.
* **Cutout Starting Z-Height ($Z_{\text{start}}$)**:
  * Because raw stock outside the step overrun domain remains at stock top height $H_{\text{mat}}$, the outer profile cutout path (offset outward by $+R_{\text{tool}}$) **must start stepping down from $Z = H_{\text{mat}}$** down to $Z = 0.0\,\text{mm}$ (table level).
  * This prevents the cutter from performing an excessive, un-stepped plunge through raw stock between $H_{\text{mat}}$ and $h_{\text{base}}$.
* **Toolpath Offsetting & Axial Passes**:
  * Tool centerline is offset outward from $\partial \Omega_{\text{base}}$ by $+R_{\text{tool}}$ so the cutter edge touches the outer perimeter.
  * The profile is stepped down axially in increments of $\Delta Z$ from $Z = H_{\text{mat}}$ down to $Z = 0.0\,\text{mm}$.
* **No Pocket Clearing**: Below $Z = h_{\text{base}}$, no pocket clearing is performed—only the continuous 2D outer profile contour path is executed down to $Z = 0.0\,\text{mm}$.

---

## 4. Case Study: `rounded-large.bcam`

Applying this specification to `examples/rounded-large.bcam`:

### Setup & Stock Parameters
* **Material Stock Top Height ($H_{\text{mat}}$)**: $10.0\,\text{mm}$
* **Tool Diameter ($D_{\text{tool}}$)**: $2.0\,\text{mm}$ ($R_{\text{tool}} = 1.0\,\text{mm}$)
* **Stepover ($S\%$)**: $70\%$ ($w_{\text{step}} = 1.4\,\text{mm}$, $\Delta r_{\text{overrun}} = 0.6\,\text{mm}$)
* **Axial Stepdown ($\Delta Z$)**: $1.0\,\text{mm}$

### Part Geometry Breakdown
```
       Z = 10.0 mm (Material Top)
       +---------------+   <- Shape 2 (Tower, h2 = 10.0 mm)
       |    Shape 2    |
       |  (h = 10mm)   |
  +----+---------------+----+   <- Step Level Z = 5.0 mm (Shape 1, Base Platform)
  |      Cleared Step       |
  |       (h = 5mm)         |
==+=========================+== <- Machine Bed / Table (Z = 0.0 mm)
```

1. **Shape 2** ($\Omega_2$, Entities 4–7, Height $h_2 = 10.0\,\text{mm}$):
   * Solid feature extruded from $Z = 0.0$ to $Z = 10.0\,\text{mm}$.
   * Matches $H_{\text{mat}} \implies$ **Zero top clearing**.
   * Serves as the standing obstacle $\Omega_{\text{above}}$ for the step clearing operation in $Z \in (5.0, 10.0]\,\text{mm}$.

2. **Shape 1** ($\Omega_1$, Entities 0–3, Height $h_1 = 5.0\,\text{mm}$):
   * Base platform extruded from $Z = 0.0$ to $Z = 5.0\,\text{mm}$.
   * **Step Region Clearing ($Z = 10.0 \to Z = 5.0\,\text{mm}$)**:
     $$\Omega_{\text{clear}} = \Omega_1 \setminus \Omega_2$$
     * Tool centerline domain $\mathcal{D}_{\text{centerline}} = (\Omega_1 \ominus \mathcal{B}_{0.4\,\text{mm}}) \setminus (\Omega_2 \oplus \mathcal{B}_{1.0\,\text{mm}})$ sets back open outer edges of $\Omega_1$ by $0.4\,\text{mm}$ (extending outer cutter edge $0.6\,\text{mm}$ past $\partial \Omega_1$), while clipping strictly against $\Omega_2 \oplus \mathcal{B}_{1.0\,\text{mm}}$.
     * Cleared in 5 axial passes ($Z = 9.0, 8.0, 7.0, 6.0, 5.0\,\text{mm}$) with concentric pocket loops spaced at $w_{\text{step}} = 1.4\,\text{mm}$.

3. **Part Cutout Pass ($Z = 10.0 \to Z = 0.0\,\text{mm}$)**:
   * Outer perimeter profile of $\Omega_1$ offset outward by $+R_{\text{tool}} = 1.0\,\text{mm}$.
   * Stepped down axially in 10 passes ($Z = 9.0, 8.0, \dots, 0.0\,\text{mm}$) from $H_{\text{mat}} = 10.0\,\text{mm}$ down to table level $Z = 0.0\,\text{mm}$ to safely cut the part out from raw stock without exceeding stepdown $\Delta Z$.

---

## 5. Handling Hierarchy Violations (Containment Warnings)

### Validation & Non-Blocking Warning Policy
Before toolpath generation, the CAM engine validates the topological containment hierarchy between shapes at adjacent $Z$-height levels (e.g., verifying $\Omega_{\text{island}} \subset \text{Int}(\Omega_{\text{shelf}})$ for bosses or $\Omega_{\text{floor}} \subset \text{Int}(\Omega_{\text{wall}})$ for pockets).

If a taller shape $S_{\text{tall}}$ extends outside its supporting base shape $S_{\text{short}}$:

1. **Warning Diagnostics**:
   * **GUI Mode**: Appends a prominent warning message to the status text log / message dialog:
     `"Warning: Shape X (h=10.0mm) extends outside base Shape Y (h=5.0mm). G-code will be generated, but toolpaths may result in unsupported geometry."`
   * **CLI Mode**: Prints a diagnostic warning message to `std::cerr`.
   * **G-Code Comment Header**: Embeds a formal warning header near the top of the exported `.cnc` file:
     `( WARNING: Containment hierarchy violation detected - Shape X (h=10.0mm) extends outside base Shape Y (h=5.0mm) )`

2. **Non-Blocking G-Code Output**:
   * The CAM engine **does not abort** G-code generation.
   * Toolpaths are produced using best-effort geometric 2D domain calculations (clipping against available stock boundaries) so the `.cnc` file is written for inspection, simulation, or machining.
