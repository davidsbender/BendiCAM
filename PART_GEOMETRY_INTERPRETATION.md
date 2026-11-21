# BendiCAM 3D Part Model & Toolpath Interpretation Specification

## 1. Executive Summary & Paradigm Overview

In BendiCAM, a 3D **Part** is defined using a 2.5D solid representation built from **stepped shape extrusions**. Each shape in a part represents a feature extruded vertically from the machine table ($Z = 0\,\text{mm}$) up to the height specified by its `depth` field ($Z = h$).

### Fundamental Principles
* **Raw Material Stock**: The material stock block has a total height $H_{\text{mat}}$ (`topHeight`, e.g., $10.0\,\text{mm}$), occupying the $Z$-range $[0, H_{\text{mat}}]$.
* **Extrusion Heights**: A shape $S_i$ with height $h_i$ defines a solid feature from $Z = 0\,\text{mm}$ up to $Z = h_i\,\text{mm}$.
* **Material Removal (Milling)**: Milling is the process of removing unwanted raw stock down to the top surface height $Z = h_i$ of each feature, and clearing the stepped regions between features.

---

## 2. Geometric Rules for Shape Extrusion & Milling

### Rule 1: Solid Feature Extrusion
* A shape $S_i$ with `depth` $h_i$ means material within $S_i$'s boundary must **remain standing** up to $Z = h_i\,\text{mm}$.
* No clearing/milling is allowed inside $S_i$'s boundary below $Z = h_i\,\text{mm}$.
* If $h_i = H_{\text{mat}}$ (e.g., $10.0\,\text{mm}$):
  * The feature extends to the full height of the raw stock.
  * **No clearing/pocket milling** is required on top of this shape because the raw stock surface is already at $Z = H_{\text{mat}}$.

### Rule 2: Stepped Region Clearing (Between Shapes)
* When a taller shape $S_{\text{tall}}$ ($h_{\text{tall}} = 10.0\,\text{mm}$) sits inside or adjacent to a shorter shape $S_{\text{short}}$ ($h_{\text{short}} = 5.0\,\text{mm}$):
  * The region between $S_{\text{tall}}$'s boundary and $S_{\text{short}}$'s boundary forms a **stepped shelf** existing between $Z = h_{\text{short}}$ and $Z = h_{\text{tall}}$.
  * Raw material in this annular region must be cleared from $Z = H_{\text{mat}}$ down to $Z = h_{\text{short}}$.
  * Pocket clearing must observe the configured tool **stepover** percentage (e.g., $70\%$).

### Rule 3: Stepover Extension at Boundary Edges
* At the outer boundary of a step (e.g. $S_{\text{short}}$ at height $h_{\text{short}}$), toolpath clearing must extend beyond the shape's boundary edge.
* With a tool stepover of $S\% = 70\%$, the clearing toolpath must extend at least:
  $$\text{Extension} = (100\% - S\%) \cdot D_{\text{tool}} = 30\% \cdot D_{\text{tool}}$$
  past the boundary of $S_{\text{short}}$ at $Z = h_{\text{short}}$ to guarantee complete material clearance over the step's edge.

### Rule 4: Outer Contour Cutout Pass
* The largest/outermost shape $S_{\text{outer}}$ (with height $h_{\text{outer}}$, e.g. $5.0\,\text{mm}$) defines the outer boundary of the part below $h_{\text{outer}}$.
* From $Z = h_{\text{outer}}$ down to $Z = 0.0\,\text{mm}$, the tool mills around the outer profile of $S_{\text{outer}}$ (offset outward by tool radius $R_{\text{tool}}$) to cut the finished part out of the stock block.
* No pocket clearing is performed below $Z = h_{\text{outer}}$—only a continuous perimeter cutout pass down to $Z = 0.0\,\text{mm}$.

### Rule 5: Geometric Shape Containment Hierarchy
For this stepped 2.5D extrusion scheme to be geometrically valid, **any taller shape ($S_{\text{tall}}$, higher $Z$) must lie completely contained within the 2D footprint of the shorter shape ($S_{\text{short}}$, lower $Z$) beneath it**:
$$\text{Footprint}(S_{\text{tall}}) \subseteq \text{Footprint}(S_{\text{short}})$$

#### Technical & CAM Justification
1. **Physical Support & Base Platform Hierarchy**: If $S_{\text{short}}$ ($h = 5.0\,\text{mm}$) is the base platform and $S_{\text{tall}}$ ($h = 10.0\,\text{mm}$) is a tower feature on top of it, $S_{\text{tall}}$ cannot overhang outside $S_{\text{short}}$. Overhanging geometry would extend down to $Z = 0.0\,\text{mm}$ outside $S_{\text{short}}$, violating the definition of $S_{\text{short}}$ as the supporting base.
2. **Well-Defined Annular Step Domain**: The stepped shelf region to be cleared between $Z = H_{\text{mat}}$ ($10.0\,\text{mm}$) and $Z = h_{\text{short}}$ ($5.0\,\text{mm}$) is defined by the set difference:
   $$\text{Clearing Domain} = \text{Footprint}(S_{\text{short}}) \setminus \text{Footprint}(S_{\text{tall}})$$
   For this domain to form a valid, closed annular shelf surrounding $S_{\text{tall}}$, $S_{\text{tall}}$ must be strictly enclosed within $S_{\text{short}}$.
3. **Multi-Feature Generalization**: If a part has multiple bosses/towers at $h = 10.0\,\text{mm}$ ($S_A, S_B$) standing on a $h = 5.0\,\text{mm}$ platform ($S_1$):
   * Each taller feature ($S_A, S_B$) must lie completely inside $S_1$.
   * Taller features at the same $Z$-level cannot overlap each other.
   * The step clearing domain becomes:
     $$\text{Clearing Domain} = \text{Footprint}(S_1) \setminus \left( \text{Footprint}(S_A) \cup \text{Footprint}(S_B) \right)$$

---

## 3. Case Study: `rounded-large.bcam`

Applying this specification to `examples/rounded-large.bcam`:

### Setup & Stock Parameters
* Material Top Height $H_{\text{mat}} = 10.0\,\text{mm}$
* Tool Diameter $D_{\text{tool}} = 2.0\,\text{mm}$ ($R_{\text{tool}} = 1.0\,\text{mm}$)
* Stepover $S\% = 70\%$ ($1.4\,\text{mm}$ stepover, $0.6\,\text{mm}$ outer boundary extension)

### Geometry Analysis
```
       Z = 10.0 mm (Material Top)
       +---------------+   <- Shape 2 (Tower, height = 10mm)
       |    Shape 2    |
       |  (h = 10mm)   |
  +----+---------------+----+   <- Step level Z = 5.0 mm (Shape 1, Base Platform)
  |      Cleared Step       |
  |       (h = 5mm)         |
==+=========================+== <- Machine Bed / Table (Z = 0.0 mm)
```

1. **Shape 2** (Inner Contour, Entities 4–7, Height $h_2 = 10.0\,\text{mm}$):
   * Extrudes from $Z = 0$ up to $Z = 10.0\,\text{mm}$.
   * Matches $H_{\text{mat}} \implies$ **Zero top clearing**.
   * Serves as the **inner island boundary** for the step clearing pass.

2. **Shape 1** (Outer Boundary, Entities 0–3, Height $h_1 = 5.0\,\text{mm}$):
   * Extrudes from $Z = 0$ up to $Z = 5.0\,\text{mm}$.
   * The region between Shape 2 (inner island) and Shape 1 (outer boundary $+ 0.6\,\text{mm}$ stepover extension) is cleared from $Z = 10.0\,\text{mm}$ down to $Z = 5.0\,\text{mm}$.

3. **Part Cutout Pass**:
   * Below $Z = 5.0\,\text{mm}$, the tool cuts along the outer perimeter of Shape 1 (offset outward by $R_{\text{tool}} = 1.0\,\text{mm}$) from $Z = 5.0\,\text{mm}$ down to $Z = 0.0\,\text{mm}$ to detach the completed part from the stock block.

---

## 4. Architectural Requirements for Toolpath Generation

To support this 3D part paradigm correctly, the CAM engine must execute the following pipeline:

1. **Height-Sorted Shape Hierarchy**:
   * Sort all shapes in a part by their height $h$ in descending order: $h_1 > h_2 > \dots > h_k$.
   * Group shapes into discrete $Z$-height levels.

2. **Step Region Calculation**:
   * For each height transition from level $Z_{\text{upper}}$ down to level $Z_{\text{lower}}$:
     * Construct the clearing domain as:
       $$\text{Domain} = \text{Boundary}(S_{\text{lower}}) \setminus \text{Boundary}(S_{\text{upper}})$$
     * Expand the outer boundary of $S_{\text{lower}}$ by $(100\% - S\%) \cdot D_{\text{tool}}$ to guarantee edge stepover clearance.
     * Generate pocket clearing toolpaths within this domain for $Z \in [Z_{\text{lower}}, Z_{\text{upper}}]$.

3. **Cutout Pass Generation**:
   * For the outermost boundary shape at $Z = h_{\text{base}}$, generate an outer profile contour path offset by $+R_{\text{tool}}$ running from $Z = h_{\text{base}}$ down to $Z = 0.0\,\text{mm}$.
