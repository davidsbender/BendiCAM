# Vergleich: Clipper2, CavalierContours und OpenCASCADE

Kurzer Überblick über Einsatzmöglichkeiten sowie Vor- und Nachteile für die Verarbeitung von 2D-Konturen (Linien, Kreisbögen, Splines) und Boolean-Operationen (`AND`, `OR`, `DIFF`) im CAD/CAM-Umfeld.

---

## 1. Clipper2

**Einsatzmöglichkeit:**  
Hochperformante, sehr robuste 2D-Boolean-Operationen (`Union`, `Intersection`, `Difference`, `XOR`), Offsetting und Clipping (auch offene Pfade an geschlossenen Konturen) auf rein **polygonaler Basis** (nur gerade Segmente). Kreisbögen und Splines müssen vorab diskretisiert (*flattening*) werden.

* **Vorteile:**
  * Extrem schnell und numerisch sehr stabil (Integer-Koordinatenarchitektur).
  * Unterstützt auch das Clippen offener Pfade gegen geschlossene Flächen.
  * Leichtgewichtig, einfache API, Ports für viele Sprachen (C++, C#, Python, JS/TS).
* **Nachteile & CAM-Nacharbeit:**
  * **Keine echten Bögen oder Splines:** Arbeitet intern ausschließlich mit Liniensegmenten.
  * **Arc-Fitting erforderlich:** Für saubere CAM-Daten (z. B. `G2`/`G3`-Kreisbögen statt hunderter kleiner `G1`-Segmente) muss nach den Boolean-/Offset-Operationen zwingend ein **Arc-Fitting** durchgeführt werden, um Kreisbögen wieder aus den Punktfolgen zu rekonstruieren.

---

## 2. CavalierContours

**Einsatzmöglichkeit:**  
Spezialisierte 2D-CAD/CAM-Bibliothek für **Polylines aus Linien und echten Kreisbögen** (über `bulge`-Faktoren). Bietet Parallel-Offsetting (inkl. Inseln/Taschen) und Booleans (`OR`, `AND`, `XOR`, `NOT`) für **geschlossene** Konturen.

* **Vorteile:**
  * Erhält echte analytische Kreisbögen durchgängig – kein nachträgliches Arc-Fitting nötig.
  * Sehr schnell durch räumliche Indexierung (AABB / Hilbert R-Tree) und schlanke Codebasis.
  * Hervorragend auf 2D-CAM-Anforderungen (Taschenfräsen, Kontur-Offsets) zugeschnitten.
* **Nachteile:**
  * **Keine Splines / Bézierkurven:** Müssen vorab in Linien/Kreisbögen angenähert werden.
  * **Booleans nur für geschlossene Pfade:** Schnitte oder Trimming offener Polylines erfordern eigene Logik.
  * Aktive Entwicklung nur in **Rust** (Einbindung in C/C++ über C-FFI erforderlich).

---

## 3. OpenCASCADE (OCCT)

**Einsatzmöglichkeit:**  
Vollwertiger 2D/3D-B-Rep-CAD-Kernel für **exakte analytische Geometrie** (Linien, Kreis-/Ellipsenbögen, Bézier, BSplines/NURBS), exakte Kurvenschnitte sowie topologische 2D- und 3D-Booleans.

* **Vorteile:**
  * Beherrscht **echte Splines/NURBS** und Kreisbögen nativ ohne Approximationsverlust.
  * Sehr mächtige Schnitt-, Projektions- und Trimming-Funktionen auch für offene Kurven.
  * Direkter Support für industrielle CAD-Formate (`STEP`, `IGES`, `BREP`).
* **Nachteile:**
  * Sehr hohe API-Komplexität, steile Lernkurve und viel Boilerplate-Code.
  * Schwergewichtiger Build, große Binaries und höhere Latenz bei vielen kleinen 2D-Operationen.
  * Empfindlich bei Toleranzen und fast-kollinearen bzw. tangierenden Randfällen in Booleans.

---

## Kompakte Gegenüberstellung

| Kriterium | Clipper2 | CavalierContours | OpenCASCADE (OCCT) |
| :--- | :--- | :--- | :--- |
| **Geometrietypen** | Nur Linien (Polygone) | Linien + echte Kreisbögen | Linien, Bögen, Splines/NURBS |
| **CAM-Nacharbeit** | **Arc-Fitting nötig** (`G2`/`G3`) | Direkt CAM-tauglich (Bögen erhalten) | Direkt CAD/CAM-tauglich |
| **Geschlossene Booleans** | Sehr stark & extrem robust | Sehr stark (für Linien + Bögen) | Stark, aber komplex & toleranzsensibel |
| **Offene Pfade / Schnitte** | Clipping an geschlossenen Flächen | Nur über eigene Schnittlogik | Volle Kurven-/Kurvenschnitte |
| **Integration & Gewicht** | Sehr leichtgewichtig (C++, Python, …) | Schlank (Rust / C-FFI) | Schwergewichtig (C++ / Python-Bindings) |

---

## Entscheidungs-Faustregel

* **Maximale Robustheit & Tempo + offene Pfade clippen:** **Clipper2** (mit anschließendem *Arc-Fitting* für CAM).
* **2D-Konturen mit echten Kreisbögen & Offsets ohne CAD-Overhead:** **CavalierContours**.
* **Echte Splines/NURBS und komplexe analytische Kurvenschnitte:** **OpenCASCADE**.