#ifndef ANALYTICALTOOLPATH_H
#define ANALYTICALTOOLPATH_H

#include "DxfParser.h"
#include <vector>
#include <set>
#include <string>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>

struct CamShape {
    int shapeId = 1;
    double depth = 5.0;
    std::set<size_t> entityIndices;
};

struct CamPart {
    int id = 1;
    std::string name;
    std::vector<CamShape> shapes;
};

struct HierarchyValidationResult {
    bool hasViolation = false;
    std::vector<std::string> warnings;
};

struct HeightBand {
    double zUpper;
    double zFloor;
};

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
    bool isCCW = true; // true = Counter-Clockwise sweep from startPt to endPt around center

    PathPrimitive() = default;

    static PathPrimitive CreateLine(const DxfPoint& start, const DxfPoint& end) {
        PathPrimitive p;
        p.type = PrimitiveType::LINE;
        p.startPt = start;
        p.endPt = end;
        return p;
    }

    static PathPrimitive CreateArc(const DxfPoint& start, const DxfPoint& end, const DxfPoint& centerPt, double r, bool ccw) {
        PathPrimitive p;
        p.type = PrimitiveType::ARC;
        p.startPt = start;
        p.endPt = end;
        p.center = centerPt;
        p.radius = r;
        p.isCCW = ccw;
        return p;
    }
};

using AnalyticalPath = std::vector<PathPrimitive>;

// Geometry & Utility Functions
double getPrimitiveLength(const PathPrimitive& prim);
double getPathLength(const AnalyticalPath& path);
double getAnalyticalPathArea(const AnalyticalPath& path); // Signed area (+ for CCW, - for CW)
DxfPoint getPrimitiveStartTangent(const PathPrimitive& prim);
DxfPoint getPrimitiveEndTangent(const PathPrimitive& prim);
DxfPoint evaluatePrimitive(const PathPrimitive& prim, double t);

// Winding & Orientation Normalization
void ensureLoopOrientation(AnalyticalPath& path, bool targetCCW = true);
AnalyticalPath reverseAnalyticalPath(const AnalyticalPath& path);
PathPrimitive reversePrimitive(const PathPrimitive& prim);

// Point-in-Polygon & Distance Utilities
bool isPointInsideAnalyticalPath(const DxfPoint& pt, const AnalyticalPath& path);
double pointToAnalyticalPathDistance(const DxfPoint& pt, const AnalyticalPath& path);

// Extraction from DXF Model / CamShape
std::vector<AnalyticalPath> getShapeAnalyticalPaths(const DxfModel& model, const std::vector<size_t>& entityIndices);

// Sampling for Screen/3D Rendering
std::vector<DxfPoint> sampleAnalyticalPath(const AnalyticalPath& path, double maxStepLen = 1.0);

// Offsetting Algorithms
PathPrimitive offsetSinglePrimitive(const PathPrimitive& prim, double dist);
bool checkCornerConvexity(const PathPrimitive& prim1, const PathPrimitive& prim2, double offsetDist);
PathPrimitive createRollAroundArc(const PathPrimitive& prim1, const PathPrimitive& prim2, const PathPrimitive& offsetPrim1, double dist);
bool findPrimitiveIntersection(const PathPrimitive& p1, const PathPrimitive& p2, DxfPoint& outIntersect);
bool findPrimitiveIntersectionBounded(const PathPrimitive& p1, const PathPrimitive& p2, DxfPoint& outIntersect);

std::vector<AnalyticalPath> offsetAnalyticalPath(const AnalyticalPath& path, double dist);
AnalyticalPath cleanDegeneratePrimitives(const AnalyticalPath& path);

// Self-Intersection Pruning & Loop Splitting
std::vector<AnalyticalPath> pruneSelfIntersectionsAndSplit(const AnalyticalPath& path, const AnalyticalPath* parentBoundary = nullptr, double distFromParent = 0.0, bool isOutward = false);

// Concentric Pocket Ring Generation
std::vector<AnalyticalPath> generatePocketRings(const AnalyticalPath& outerBoundary, double toolRad, double stepoverDist);

// G-Code Export & Multi-Feature Domain Processing
HierarchyValidationResult validatePartHierarchy(const DxfModel& model, const std::vector<CamShape>& shapes, double topHeight);
std::vector<HeightBand> computePartHeightBands(const std::vector<CamShape>& shapes, double topHeight);
std::vector<AnalyticalPath> generateBandClearingLoops(const DxfModel& model, const std::vector<CamShape>& shapes, double zUpper, double zFloor, double topHeight, double toolDiameter, double stepoverPercent);
std::vector<AnalyticalPath> generateOuterCutoutPaths(const DxfModel& model, const std::vector<CamShape>& shapes, double toolDiameter);

void emitAnalyticalLoopGCode(std::ofstream& out, const AnalyticalPath& path,
                            double prevZ, double curZ, double feedZ, double feedXY, double retractZ,
                            double rampAngleDeg, bool isRamping, DxfPoint& currentToolPos, bool& isRetracted);
void emitPrimitiveGCode(std::ofstream& out, const PathPrimitive& prim, double z, double feed);

#endif // ANALYTICALTOOLPATH_H