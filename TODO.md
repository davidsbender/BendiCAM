# TODO

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