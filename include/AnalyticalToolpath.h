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
#include <cstdint>

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
    bool isSpur = false;

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

enum class PathRole {
    UNKNOWN = 0,
    RAW_SHAPE,
    SHAPE_OUTLINE,
    SHAPE_TOOLPATH,
    SHAPE_CUTZONE,
    MATERIAL_OUTLINE,
    MATERIAL_CUTZONE_INNER,
    MATERIAL_TOOLPATH,
    MATERIAL_CUTZONE_OUTER,
    ISLAND_OUTLINE,
    ISLAND_TOOLPATH,
    ISLAND_CUTZONE,
    POCKET_RING,
    BAND_CLEARING,
    OUTER_CUTOUT
};

struct PathTraceInfo {
    // Ownership & Semantic Role
    int sourcePartId = -1;
    int sourceShapeId = -1;
    PathRole role = PathRole::UNKNOWN;

    // Lineage Parameters
    uint64_t entityHash = 0;       // 64-bit hash of sorted base DXF entity indices
    double offsetDistance = 0.0;   // In millimeters (outward +, inward -)

    // Signatures
    uint64_t entitySignature = 0;   // hash(entityHash, quantized offsetDistance)
    uint64_t geometrySignature = 0; // hash(Area, Perimeter, Centroid, BoundingBox)

    // Comparison Predicates
    // 1. Same source CAD geometry (ignores role & offset)
    bool isSameSourceEntity(const PathTraceInfo& other) const {
        return entityHash != 0 && entityHash == other.entityHash;
    }

    // 2. Same source CAD geometry AND same offset (ignores role)
    bool isSameOffsetLineage(const PathTraceInfo& other) const {
        return entitySignature != 0 && entitySignature == other.entitySignature;
    }

    // 3. Same physical curve in (X, Y) space (ignores CAD entity provenance)
    bool isSameGeometry(const PathTraceInfo& other) const {
        return geometrySignature != 0 && geometrySignature == other.geometrySignature;
    }

    // 4. True duplicate (same CAD source, same offset, same physical geometry)
    bool isRedundantWith(const PathTraceInfo& other) const {
        return isSameOffsetLineage(other) && isSameGeometry(other);
    }
};

struct AnalyticalPath : public std::vector<PathPrimitive> {
    PathTraceInfo trace;

    using std::vector<PathPrimitive>::vector;
};

// Dual-Role Toolpath Structures
struct IslandPaths {
    std::vector<AnalyticalPath> outline;
    std::vector<AnalyticalPath> toolPath;
    std::vector<AnalyticalPath> cutZone;

    bool empty() const { return outline.empty(); }
};

struct MaterialPaths {
    std::vector<AnalyticalPath> outline;
    std::vector<AnalyticalPath> toolPath;
    std::vector<AnalyticalPath> cutZoneInner;
    std::vector<AnalyticalPath> cutZoneOuter;

    bool empty() const { return outline.empty(); }
};

struct ShapePaths {
    int shapeId = -1;
    const CamShape* shape = nullptr;
    IslandPaths   island;    // 3 paths when acting as an Island / Boss
    MaterialPaths material;  // 4 paths when acting as Material / Floor
};

struct SelectedShapeProcessingPaths {
    const ShapePaths* shape = nullptr;           // Primary selected shape (Island role)
    const ShapePaths* material = nullptr;        // Shape below acting as Material floor
    std::vector<const ShapePaths*> islandShapes; // Sibling shapes on same floor (Island role)

    bool empty() const {
        return shape == nullptr && material == nullptr;
    }
};

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

AnalyticalPath offsetAnalyticalPathRaw(const AnalyticalPath& inPath, double dist);
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

std::vector<AnalyticalPath> getShapeAnalyticalPaths(const DxfModel& model, const CamShape& shape);

// Traceability & Signature Functions
uint64_t computeEntityHash(const std::vector<size_t>& entityIndices);
uint64_t computeEntitySignature(uint64_t entityHash, double offsetDistMm);
uint64_t computeGeometrySignature(const AnalyticalPath& path);

struct MachiningLevel {
    int levelIndex = 0;
    double depth = 0.0;
    int belowShapeId = -1;
    std::vector<int> shapeIndices;
};

struct MachiningLevelPaths {
    int levelIndex = -1;
    double depth = 0.0;
    int belowShapeId = -1;

    // Pointer to the shape acting as the Floor / Material (nullptr if raw stock boundary)
    const ShapePaths* material = nullptr;

    // Pointers to all shapes acting as Islands on this floor
    std::vector<const ShapePaths*> islands;

    bool empty() const {
        return islands.empty() && material == nullptr;
    }
};

const CamShape* findShapeBelow(
    const DxfModel& model,
    const CamPart& part,
    const CamShape& targetShape);

std::vector<MachiningLevel> computePartMachiningLevels(
    const DxfModel& model,
    const CamPart& part);

// Dual-role path generators
ShapePaths computeShapePaths(
    const DxfModel& model,
    const CamPart& part,
    const CamShape& shape,
    double toolDiameter,
    double overcutPercent);

std::vector<ShapePaths> computeAllPartShapePaths(
    const DxfModel& model,
    const CamPart& part,
    double toolDiameter,
    double overcutPercent);

SelectedShapeProcessingPaths computeSelectedShapeProcessingPaths(
    const DxfModel& model,
    const CamPart& part,
    int selectedShapeIdx,
    const std::vector<ShapePaths>& allShapePaths);

MachiningLevelPaths computeMachiningLevelPaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevel& level,
    const std::vector<ShapePaths>& allShapePaths);

void emitAnalyticalLoopGCode(std::ofstream& out, const AnalyticalPath& path,
                            double prevZ, double curZ, double feedZ, double feedXY, double retractZ,
                            double rampAngleDeg, bool isRamping, DxfPoint& currentToolPos, bool& isRetracted);
void emitPrimitiveGCode(std::ofstream& out, const PathPrimitive& prim, double z, double feed);

#endif // ANALYTICALTOOLPATH_H