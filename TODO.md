# TODO

## Floor Clearing Cusp Prevention & High-Speed Boundary Linking along Boss/Island Outlines

### 1. Problem Statement & Root Cause Diagnosis
- **The Early Termination / Disconnection Phenomenon:**
  In floor clearing infill, parallel zig-zag passes terminate against curved island boundaries (e.g. in `examples/Kugellift simpel.dxf` Level 2, the vertical pass at $X = 374.989$ terminates at $Y = 155.796$ against the $R = 13.4\text{ mm}$ fillet arc, while the adjacent pass at $X = 373.599$ travels all the way to $Y = 200.958$).
  - The 1D interval subtraction ($I_{\text{valid}} = I_{\text{outer}} \setminus I_{\text{hole}}$) correctly clips $X = 374.989$ because above $Y = 155.796$ that pass falls inside the forbidden hole boundary ($X_{\text{hole}} = 374.6\text{ mm}$).
  - However, direct straight linking between $(374.989, 155.796)$ and $(373.599, 200.958)$ ($45.2\text{ mm}$) would slice straight through the solid island.
  - The linker therefore breaks the path, causing **18 unnecessary retracts to Safe Z and re-plunges** on a single level floor (fragmenting infill into 19 isolated chains), and leaving a visible gap in the blue floor.

- **The "Wrong-Side" Link Scallop & Mathematical Cusp Condition:**
  In standard zig-zag rastering, every second link occurs on the "wrong side" (the far boundary). Between passes returning to the boss boundary, the tool does not travel along the boundary, leaving an unvisited gap along the wall proportional to stepover $S$.
  - The envisioned boss wall finish pass (`bossToolpaths`) runs at distance $R = 0.5 D$ from the wall, reaching an outer cutting boundary of $D$ ($100\%$).
  - Floor clearing stops at distance $M$ from the wall ($M = \text{boss margin} = D - \text{overcut} = 100\% - \text{overcut}$).
  - The radial overlap between outline milling and floor clearing is $(D - M) = 100\% - M$.
  - **The Zero-Cusp Threshold:**
    $$\mathbf{S + M \le 100\%} \quad (\text{No Cusps Form})$$
    $$\mathbf{S + M > 100\%} \quad (\text{Uncut Cusps Guaranteed to Form})$$
    *Where $S = \text{Stepover}$ and $M = \text{Boss Margin} = (100\% - \text{Overcut})$.*
  - For example, with default Stepover $= 70\%$ and Overcut $= 30\%$ ($M = 70\%$):
    $$S + M = 70\% + 70\% = \mathbf{140\%} > 100\%$$
    This leaves an uncut triangular cusp of up to **$0.151\text{ mm}$** along the fillet arc.
  - Higher stepover (e.g. $95\%$) and larger boss margin (e.g. $30\%$) increase $S + M$ (e.g. $125\%$), widening the uncut gap.

---

### 2. The Two-Part Solution

#### Part A: High-Speed Boundary Linking (Zero Retracts)
- When adjacent zig-zag passes end against an obstacle boundary, do not break the chain and retract to Safe Z.
- Instead, link the two endpoints by **gliding along the boundary contour** (`requiredCoverage.holeBoundaries` or `outerBoundaries`) inside `allowedEnvelope`.
- Keeps the tool down at floor depth, merging fragmented chains into continuous machining loops, eliminating plunge dwell marks, and dramatically reducing cycle time.

#### Part B: Outer Perimeter Loop on `requiredCoverage` (Zero Cusps at Any Stepover)
- To guarantee $0.000\text{ mm}$ cusps when $S + M > 100\%$, run a dedicated **Floor Perimeter Loop** directly on `requiredCoverage.holeBoundaries` at floor depth.
- **Clarification: Relationship between the Two Runs Around an Island:**
  1. **Envisioned Outline Milling (`bossToolpaths`):**
     - Runs at distance $0.5 D$ ($1.0\text{ mm}$ for $D = 2.0\text{ mm}$) from the boss wall.
     - Cutter inner edge touches the wall ($0.0\text{ mm}$), outer edge reaches $1.0 D$ ($2.0\text{ mm}$).
     - Dedicated finish pass at stepdown/final depth for wall precision.
  2. **Floor Perimeter Loop (`requiredCoverage.holeBoundaries`):**
     - A **second run around the island at floor depth, further outside**.
     - Runs at distance $D - \text{overcut}$ ($1.4\text{ mm}$ for $30\%$ overcut).
     - Cutter inner edge is at $0.4\text{ mm}$ (safely in open air, never touching or gouging the finished wall), outer edge reaches $2.4\text{ mm}$.
     - Wipes across all jagged zig-zag ends and permanently seals the "wrong-side" gaps along curves and angles.

#### Alternative Part B: Localized Boundary Spurs on the "Wrong Side" (Faster / Shorter Path)
*(To be evaluated and discussed during implementation as an optimization over a full perimeter loop)*
- **Concept:** Instead of a full $360^\circ$ outer perimeter loop around the island, emit out-and-back **boundary linking spurs** strictly on the "wrong side" where links are skipped.
- **Selective Activation:**
  - On straight walls perpendicular to zig-zag passes ($\theta \approx 90^\circ$), or where $S + M \le 100\%$, cusps do not form naturally. Zero spurs are emitted there.
  - Spurs are only triggered locally where the angle between the pass and boundary is shallow (or along curved fillets) and $S + M > 100\%$.
- **Execution:**
  - Upon reaching the boss boundary on the unlinked side, the cutter steps outward along the `requiredCoverage` boundary curve towards the previous pass to evacuate the scallop apex, then retraces its path through empty air back to the pass endpoint before continuing down the zig-zag.
  - Matches BendiCAM's existing out-and-back corner spur architecture (`PathPrimitive::hasEndSpur`).
- **Speed Advantage:** Eliminates redundant air cutting along already-cleared straight wall segments, yielding a significantly shorter total path length and faster overall machining cycle time.

---

### 3. Implementation Steps in `src/MachiningPaths.cpp`
1. **Perimeter Loop Integration:**
   - In `generateFloorClearingToolpaths`, append closed loops from `zones.requiredCoverage.holeBoundaries` (and `outerBoundaries`) to the floor clearing toolpath.
   - Eliminates all cusps regardless of high stepover or large boss margins.
2. **Boundary-Tracing Linker:**
   - In `generateFloorClearingToolpaths`, when `dLink > 2.5 * actualStep` and both points lie on a common boundary loop, trace the boundary primitive(s) between them instead of falling back to a retract/plunge chain break.
3. **Verification:**
   - Verify infill chain count on Level 2 drops from 19 disconnected chains to continuous loops.
   - Verify uncut floor points drop to 0 across all stepover values ($50\% - 95\%$) and boss margins.

---

## allowedEnvelope around material too restrictive

allowedEnvelope = material.cutZoneOuter expanded by tool radius is not enough (efficient?) for high overcut?

## Sharp Machining Corner Spurs Along Boss Toolpath

When a boss toolpath clips against a material floor boundary (e.g. where a boss meets or crosses the edge of a pocket floor or stock boundary), an out-and-back spur following that boss toolpath is required to completely clear the corner down to the floor depth.

### 1. Geometric Necessity (Why Stock & Boss Passes Miss the Corner)
- **The Pocket Clearing Pass:** The pocket outer boundary turns at the inward-offset corner $(+R, +R)$, leaving an unmachined fillet/cusp of radius $R$ ($0 < X < R, 0 < Y < R$) anchored at the corner apex $(0, 0)$.
- **The Outer Profile / Stock Pass ("Material in Boss Role"):** Runs outside the part at $X = -R$. The cutter edge only reaches $X \le 0$ tangentially and touches **zero percent** of the uncut cusp in $X > 0$.
- **The Boss Clearing Pass Inside the Pocket:** Was clipped at $X = +R$ by Boolean subtraction against the floor boundary and never travels into $X < +R$.
- **The Solution:** Extending a spur along the boss toolpath from the pocket corner $(+R, +R)$ outward to the toolpath of the material in its boss role ($X = -R$) cleanly evacuates the entire corner cusp.

### 2. Why Out-and-Back on the Exact Same Path is Optimal
1. **Guaranteed Gouge-Free:**
   - **Outward Stroke:** The tool follows the boss offset line, remaining strictly tangent to the boss boundary.
   - **Return Stroke:** The cutter retraces the exact path it just evacuated through open space. It is mathematically impossible to gouge the boss wall or the floor.
2. **Zero Cutting Load on Return:**
   - Because all material was removed during the outward stroke, the return stroke travels through empty air.
   - Eliminates lateral cutting forces, tool deflection, vibration, and chatter.
3. **Loop Continuity (No Retracts):**
   - The pocket loop remains a continuous 2D motion without needing to break, retract to Safe Z, or re-plunge.
   - The cutter reaches the corner, steps out to the apex, steps back, and smoothly continues around the pocket boundary.
4. **Natural Geometry Conformance:**
   - The spur inherits the underlying boss primitive: if the boss edge is a line, the spur is a straight line; if the boss edge is an arc, the spur follows that circular arc.
5. **Architectural Consistency:**
   - Matches BendiCAM's existing corner spur execution model (`PathPrimitive::hasEndSpur`, `endSpurApex`, and `G1` out-and-back emission).

---

## 1. Tangential Lead-In and Lead-Out Arcs for Boss Wall Contouring
- **Description:** When contouring boss outlines, use smooth tangential roll-in and roll-out arcs (or helical ramp entries) rather than direct radial plunges against the wall.
- **Benefits:**
  - Prevents visible dwell marks, tool deflection notches, and localized cutter stress at the start/end points of the wall contour pass.
  - Ensures a seamless blend on closed perimeter walls.

---

## 2. Separate Roughing (Coarse) and Finishing (Spring) Passes for Boss Walls
- **Description:** Support multi-pass wall machining with radial stock allowance:
  - **Coarse / Roughing Passes:** Run during stepdown clearing with a small radial offset allowance (e.g. 0.2–0.5 mm) to absorb heavy cutting loads.
  - **Final Finish / Spring Pass:** Run at full depth (or final stepdown) with zero allowance at high spindle RPM and controlled feed rate to remove deflection taper and deliver a mirror wall finish.

---

## 3. Dual Toolpath Generation Modes: High Finish vs. High Speed
- **Description:** Allow user selection of toolpath strategy in Machining Settings:
  - **Decoupled Mode (Default / High Finish):** Continuous outward boss contours with pure climb milling + separate floor clearing passes. Eliminates spurs and direction reversals.
  - **Integrated Mode (High Speed / Utility):** Single composite loop with out-and-back spurs at sharp corners to eliminate redundant cutting overlap at shared edges.

---

## 4. Floor Clearing Infill Strategies & Smooth Transitions
- **Description:** Implement raster/scanline and concentric offset infill generators consuming the Three-Zone clearing model:
  - **Scanline / Zig-Zag Clearing:** Configurable raster angle ($0^\circ$ horizontal, $90^\circ$ vertical, or along principal bounding box axis).
  - **High-Speed Smooth Reversals:** Cutter reverses direction inside the "May-Be-Cleared" freedom zone using tangential S-curves, loops, or roll-outs rather than abrupt linear stops.
  - **Concentric Offset Clearing:** Alternative offset-ring clearing strategy bounded by the Must-Clear and May-Clear zones.

---

## Precompiled wxWidgets Library Support

- Investigate if and how precompiled wxWidgets libraries can be integrated into the build configuration (Code::Blocks / MinGW Makefile).
- Enables building and developing BendiCAM on machines where wxWidgets source trees are not present.

---

## Margins

### allowedEnvelope

Element: [Boss 1 outline] -> Operation: [offset outwards by avoidance margin] -> Element: [Boss 1 allowed envelope (hole, outside is allowed)]
Element: [Boss 2 outline] -> Operation: [offset outwards by avoidance margin] -> Element: [Boss 2 allowed envelope (hole, outside is allowed)]
...etc. for all bosses on stock...

Element: [Stock outline] -> Operation: [offset outwards by air margin] -> Element: [Stock allowed envelope (area, inside is allowed)]

### requiredCoverage

Element: [Boss 1 outline] -> Operation: [offset outwards by (tool diameter - overcut)] -> Element: [Boss 1 required envelope (hole, outside is required)]
Element: [Boss 2 outline] -> Operation: [offset outwards by (tool diameter - overcut)] -> Element: [Boss 2 required envelope (hole, outside is required)]