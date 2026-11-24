# BendiCAM 3D Part Geometry Interpretation Specification

## 1. Executive Summary & Geometric Paradigm

In BendiCAM, a 3D **Part** is represented as a **2.5D solid** constructed from **stepped planar extrusions**. Each shape in a part defines a closed 2D geometric domain extruded vertically from a datum plane ($Z = 0.0\,\text{mm}$) up to its specified extrusion height ($Z = h_i$), bounded within a raw stock envelope of thickness $H_{\text{mat}}$.

### Fundamental Coordinate System & Dimensions
* **Datum Reference Plane ($Z = 0.0\,\text{mm}$)**: The bottom base plane of the part and stock volume. All solid extrusions originate at $Z = 0.0\,\text{mm}$.
* **Stock Envelope Height ($H_{\text{mat}}$)**: Total vertical thickness of the bounding stock volume (`topHeight`, e.g., $10.0\,\text{mm}$), defining the vertical domain $Z \in [0.0, H_{\text{mat}}]$.
* **Shape Extrusion Height ($h_i$)**: Each shape $S_i$ carries a scalar height parameter $h_i \in (0.0, H_{\text{mat}}]$ (`depth` field), defining the upper $Z$-elevation of that feature's top horizontal face.
* **2.5D Solid Assumption**: All vertical side walls connecting horizontal faces at different $Z$-elevations are strictly orthogonal to the $XY$-plane (parallel to the $Z$-axis). Overhangs, undercuts, and non-vertical walls do not exist in this representation.

---

## 2. 2D Planar Regions & Analytical Boundary Definitions

Part geometry is defined using **2D planar regions** bounded by closed, ordered analytical primitives (`LINE` and circular `ARC` segments).

### Formal Definitions
* **Analytical Boundary Loop ($\partial \Omega$)**: A closed, non-self-intersecting, oriented 2D curve composed of an ordered sequence of connected `PathPrimitive` elements (`LINE` segments and circular `ARC` segments).
  * **Counter-Clockwise (CCW)** orientation defines an outer exterior boundary.
  * **Clockwise (CW)** orientation defines an inner void, hole, or nested boundary.
* **Planar Region ($\Omega \subset \mathbb{R}^2$)**: The closed, bounded 2D area domain enclosed by an analytical boundary loop $\partial \Omega$, inclusive of its boundary ($\Omega = \text{Int}(\Omega) \cup \partial \Omega$).
* **Primitive Continuity ($C^0$)**: For an ordered loop of $N$ primitives $\{e_0, e_1, \dots, e_{N-1}\}$, the terminal endpoint of $e_k$ coincides with the initial endpoint of $e_{(k+1) \bmod N}$ within geometric tolerance $\epsilon_{\text{geom}}$.

---

## 3. Feature Extrusion & Topological Hierarchy Rules

### 3.1 Rule 1: Individual Shape Volume Extrusion
* Taken in isolation, a shape $S_i = (\Omega_i, h_i)$ represents a right vertical prism occupying the 3D point set:
  $$V_i = \Omega_i \times [0.0, h_i] = \{ (x, y, z) \in \mathbb{R}^3 \mid (x, y) \in \Omega_i \text{ and } 0.0 \le z \le h_i \}$$
* If $h_i = H_{\text{mat}}$, the top face of $S_i$ is coplanar with the upper boundary plane of the stock envelope ($Z = H_{\text{mat}}$).
* If $h_i < H_{\text{mat}}$, the top face of $S_i$ forms a recessed or stepped horizontal surface at $Z = h_i$, leaving an empty void volume $\Omega_i \times (h_i, H_{\text{mat}}]$ above it unless occupied by a nested taller feature.

---

### 3.2 Rule 2: Topological Containment & Feature Classifications

A multi-shape part is structured through spatial containment relationships between planar regions in the $XY$-plane. Two fundamental 2.5D feature topologies arise from nested boundaries:

#### A. Bosses / Islands ($h_{\text{child}} > h_{\text{parent}}$)
* A taller feature $S_{\text{island}} = (\Omega_{\text{island}}, h_{\text{island}})$ rises above a lower surrounding base platform $S_{\text{shelf}} = (\Omega_{\text{shelf}}, h_{\text{shelf}})$, where $h_{\text{island}} > h_{\text{shelf}}$.
* **Strict Containment Condition**: The 2D footprint of the island lies strictly inside the interior of the base platform:
  $$\Omega_{\text{island}} \subset \text{Int}(\Omega_{\text{shelf}})$$
* **Resulting Solid Geometry**:
  * The annulus $\Omega_{\text{shelf}} \setminus \Omega_{\text{island}}$ has top surface elevation $Z = h_{\text{shelf}}$.
  * The inner domain $\Omega_{\text{island}}$ has top surface elevation $Z = h_{\text{island}}$.
  * A vertical step wall of height $\Delta h = h_{\text{island}} - h_{\text{shelf}}$ rises along $\partial \Omega_{\text{island}}$ from $Z = h_{\text{shelf}}$ to $Z = h_{\text{island}}$.

#### B. Pockets / Recesses ($h_{\text{child}} < h_{\text{parent}}$)
* A lower floor feature $S_{\text{floor}} = (\Omega_{\text{floor}}, h_{\text{floor}})$ is recessed into a taller surrounding plateau/wall $S_{\text{wall}} = (\Omega_{\text{wall}}, h_{\text{wall}})$, where $h_{\text{floor}} < h_{\text{wall}}$.
* **Strict Containment Condition**: The 2D footprint of the pocket floor lies strictly inside the interior of the surrounding plateau:
  $$\Omega_{\text{floor}} \subset \text{Int}(\Omega_{\text{wall}})$$
* **Resulting Solid Geometry**:
  * The surrounding rim $\Omega_{\text{wall}} \setminus \Omega_{\text{floor}}$ retains top surface elevation $Z = h_{\text{wall}}$.
  * The inner cavity $\Omega_{\text{floor}}$ has its floor surface elevation depressed to $Z = h_{\text{floor}}$ (overriding the taller height of $S_{\text{wall}}$ within $\Omega_{\text{floor}}$).
  * A vertical cavity wall of depth $\Delta h = h_{\text{wall}} - h_{\text{floor}}$ drops along $\partial \Omega_{\text{floor}}$ from $Z = h_{\text{wall}}$ down to $Z = h_{\text{floor}}$.

#### C. Multi-Level Nesting (Arbitrary Depth Trees)
* Bosses and pockets can be nested recursively to arbitrary depth (e.g., an island rising from the floor of a pocket, or a pocket recessed into the top face of a boss).
* Each nesting relationship forms a **Containment Tree** $\mathcal{T} = (\mathcal{V}, \mathcal{E})$, where a directed edge $S_{\text{parent}} \to S_{\text{child}}$ exists if and only if $\Omega_{\text{child}} \subset \text{Int}(\Omega_{\text{parent}})$ and there is no intermediate shape $S_{\text{mid}}$ such that $\Omega_{\text{child}} \subset \text{Int}(\Omega_{\text{mid}}) \subset \text{Int}(\Omega_{\text{parent}})$.

---

### 3.3 Rule 3: Scalar 2D Height Field & Solid Reconstruction

Because a pocket shape ($h_{\text{child}} < h_{\text{parent}}$) subtracts height from its enclosing parent rather than performing a naive Boolean union of extruded prisms, the exact 3D solid geometry of a BendiCAM Part is defined via a **piecewise-constant 2D height field** $H: \mathbb{R}^2 \to [0.0, H_{\text{mat}}]$.

#### Height Field Precedence Law
For any point $\mathbf{p} = (x, y) \in \mathbb{R}^2$:
1. Let $\mathcal{S}(\mathbf{p}) = \{ S_i \mid \mathbf{p} \in \Omega_i \}$ be the set of all shapes whose 2D planar region contains $\mathbf{p}$.
2. If $\mathcal{S}(\mathbf{p}) = \emptyset$, the point lies outside the part footprint ($H(\mathbf{p}) = 0.0$, void).
3. If $\mathcal{S}(\mathbf{p}) \neq \emptyset$, the **innermost (deepest descendant) shape** $S^* \in \mathcal{S}(\mathbf{p})$ in the containment tree $\mathcal{T}$ uniquely determines the local surface elevation:
   $$H(x, y) = h_{S^*}$$
   *(Equivalently, child features always override the elevation of their enclosing ancestor features within the child's 2D footprint $\Omega_{\text{child}}$).*

#### Exact 3D Part Solid ($V_{\text{part}}$)
Given the global height field $H(x, y)$ over the union of all shapes $\Omega_{\text{part}} = \bigcup_i \Omega_i$, the exact 3D solid volume of the part is:
$$V_{\text{part}} = \left\{ (x, y, z) \in \mathbb{R}^3 \;\middle|\; (x, y) \in \Omega_{\text{part}} \text{ and } 0.0 \le z \le H(x, y) \right\}$$

#### Horizontal Cross-Sectional Slices
For any horizontal elevation $z \in (0.0, H_{\text{mat}}]$, the 2D solid cross-section of the part at height $z$ is the upper level set:
$$\Omega_{\text{solid}}(z) = \left\{ (x, y) \in \Omega_{\text{part}} \;\middle|\; H(x, y) \ge z \right\}$$
And the complementary void/step region within the part footprint at elevation $z$ is:
$$\Omega_{\text{void}}(z) = \Omega_{\text{part}} \setminus \Omega_{\text{solid}}(z) = \left\{ (x, y) \in \Omega_{\text{part}} \;\middle|\; H(x, y) < z \right\}$$

---

### 3.4 Rule 4: Outer Part Perimeter & Base Footprint

* **Part Footprint ($\Omega_{\text{part}}$)**: The union of all root-level shape regions in the containment forest $\mathcal{T}$:
  $$\Omega_{\text{part}} = \bigcup_{S_r \in \text{Roots}(\mathcal{T})} \Omega_r$$
* **Exterior Perimeter ($\partial \Omega_{\text{part}}$)**: The outer boundary loops of $\Omega_{\text{part}}$ define the full 2D silhouette of the finished part projected onto the datum plane $Z = 0.0\,\text{mm}$.
* Outside $\Omega_{\text{part}}$, no part material exists at any elevation $z \in [0.0, H_{\text{mat}}]$.

---

## 4. Structural Example: Stepped Boss Geometry (`rounded-large.bcam`)

Consider a two-shape stepped part within a stock envelope of $H_{\text{mat}} = 10.0\,\text{mm}$:

```
       Z = 10.0 mm (Upper Stock Plane)
       +---------------+   <- Shape 2: Top Face at Z = 10.0 mm
       |    Shape 2    |
       |  (h2 = 10mm)  |
  +----+---------------+----+   <- Shape 1: Step Face at Z = 5.0 mm
  |    Annular Shelf        |
  |      (h1 = 5mm)         |
==+=========================+== <- Datum Plane (Z = 0.0 mm)
```

1. **Shape 1** ($S_1 = (\Omega_1, h_1 = 5.0\,\text{mm})$, Entities 0–3):
   * Root node in the containment tree $\mathcal{T}$.
   * Defines the outer part footprint $\Omega_{\text{part}} = \Omega_1$ and exterior vertical perimeter $\partial \Omega_1$ extending from $Z = 0.0\,\text{mm}$ to $Z = 5.0\,\text{mm}$.
2. **Shape 2** ($S_2 = (\Omega_2, h_2 = 10.0\,\text{mm})$, Entities 4–7):
   * Child node of $S_1$ satisfying $\Omega_2 \subset \text{Int}(\Omega_1)$.
   * Because $h_2 > h_1$, $S_2$ forms a **standing boss** rising $5.0\,\text{mm}$ above the top face of $S_1$.
3. **Resolved Height Field $H(x, y)$**:
   $$H(x, y) = \begin{cases} 10.0\,\text{mm} & \text{if } (x, y) \in \Omega_2 \\ 5.0\,\text{mm} & \text{if } (x, y) \in \Omega_1 \setminus \Omega_2 \\ 0.0\,\text{mm} & \text{if } (x, y) \notin \Omega_1 \end{cases}$$

---

## 5. Geometric Validity & Containment Invariants

For a multi-shape part definition to be topologically well-formed under 2.5D hierarchical extrusion, the shape collection $\{S_i\}$ must satisfy the following invariants:

1. **Height Bounds Invariant**:
   $$0.0 < h_i \le H_{\text{mat}} \quad \forall S_i$$
2. **Closed Loop & Non-Degeneracy Invariant**:
   Every $\partial \Omega_i$ must form a closed, simple (non-self-intersecting) boundary enclosing a strictly positive 2D area ($\text{Area}(\Omega_i) > 0$).
3. **Strict Nesting / Disjointness Invariant**:
   For any two distinct shapes $S_a$ and $S_b$, their planar regions must be either **strictly nested** or **mutually disjoint**:
   $$\Omega_a \subset \text{Int}(\Omega_b) \quad \lor \quad \Omega_b \subset \text{Int}(\Omega_a) \quad \lor \quad (\Omega_a \cap \Omega_b = \emptyset)$$
   * **Partial Boundary Crossing ($\partial \Omega_a \cap \partial \Omega_b \neq \emptyset$ without full containment)**: Violates strict nesting. If a taller feature extends outside the boundary of a lower base feature ($\Omega_{\text{tall}} \not\subset \text{Int}(\Omega_{\text{short}})$), the containment hierarchy is ambiguous and triggers a geometric containment warning.