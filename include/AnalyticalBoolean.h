#ifndef ANALYTICALBOOLEAN_H
#define ANALYTICALBOOLEAN_H

#include "AnalyticalToolpath.h"
#include <vector>
#include <utility>

struct PrimitiveIntersection {
    DxfPoint pt;
    double t1 = 0.0; // [0, 1] parameter along primitive 1
    double t2 = 0.0; // [0, 1] parameter along primitive 2
};

enum class SubtractionResultType {
    EMPTY,          // Subtrahend completely covers Minuend
    DISJOINT,       // Subtrahend is completely outside Minuend
    ISLAND_INSIDE,  // Subtrahend is strictly within Minuend (creates internal island)
    CLIPPED_LOOPS   // Subtrahend intersects Minuend, producing 1 or more outer loops
};

struct BooleanSubtractionResult {
    SubtractionResultType type = SubtractionResultType::DISJOINT;
    std::vector<AnalyticalPath> loops; // CCW outer loops (when CLIPPED_LOOPS or DISJOINT)
};

// Analytical intersection between two primitives
// Handles all cases: Line-Line (0-1), Line-Arc (0-2), Arc-Line (0-2), Arc-Arc (0-2)
std::vector<PrimitiveIntersection> intersectPrimitivesAll(
    const PathPrimitive& p1,
    const PathPrimitive& p2);

// Split a primitive at parametric coordinate t in [0, 1]
// Preserves corner overcut spur tags on the second segment if prim.hasEndSpur is true
std::pair<PathPrimitive, PathPrimitive> splitPrimitive(
    const PathPrimitive& prim,
    double t);

// Reverses primitive direction (swaps startPt/endPt, inverts arc isCCW)
PathPrimitive reversePrimitiveExact(const PathPrimitive& prim);

// Evaluates a primitive at parametric coordinate t in [0, 1]
DxfPoint evaluatePrimitiveParam(const PathPrimitive& prim, double t);

// Boolean Difference between two closed analytical loops: minuend (CCW) - subtrahend (CCW)
BooleanSubtractionResult subtractAnalyticalLoops(
    const AnalyticalPath& minuend,
    const AnalyticalPath& subtrahend);

// Fast check if loop A contains all sample points of loop B
bool loopContainsLoop(const AnalyticalPath& outer, const AnalyticalPath& inner);

#endif // ANALYTICALBOOLEAN_H
