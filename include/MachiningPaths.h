#ifndef MACHININGPATHS_H
#define MACHININGPATHS_H

#include "AnalyticalToolpath.h"
#include "AnalyticalBoolean.h"
#include <vector>

// Generic 2D planar domain represented as outer loop(s) with hole(s)
struct ClearingDomain {
    // Outer boundary loops (tool center must stay INSIDE)
    std::vector<AnalyticalPath> outerBoundaries;

    // Hole / obstacle loops (tool center must stay OUTSIDE)
    std::vector<AnalyticalPath> holeBoundaries;

    bool empty() const {
        return outerBoundaries.empty() && holeBoundaries.empty();
    }
};

// Dual-domain geometric descriptors for level floor clearing
struct LevelClearingZones {
    // 1. Hard safety envelope: cutter has complete freedom to roam here
    ClearingDomain allowedEnvelope;

    // 2. Mandatory target coverage: must be completely evacuated
    ClearingDomain requiredCoverage;

    bool empty() const {
        return allowedEnvelope.empty() && requiredCoverage.empty();
    }
};

struct MachiningPocket {
    AnalyticalPath outerBoundary;                 // CCW (pocket outer boundary)
    std::vector<AnalyticalPath> islandBoundaries; // CCW (boss obstacles inside this pocket)

    bool empty() const { return outerBoundary.empty(); }

    double getNetArea() const {
        double area = std::abs(getAnalyticalPathArea(outerBoundary));
        for (const auto& isl : islandBoundaries) {
            area -= std::abs(getAnalyticalPathArea(isl));
        }
        return (area > 0.0) ? area : 0.0;
    }
};

struct LevelMachiningToolpaths {
    int levelIndex = -1;
    double depth = 0.0;
    int belowShapeId = -1;

    // Derived discrete pockets to clear on this level floor
    std::vector<MachiningPocket> pockets;

    // Concrete boss wall outline milling toolpaths (always run at level stepdowns)
    std::vector<AnalyticalPath> bossToolpaths;

    // Dual-domain clearing descriptors for floor clearing
    LevelClearingZones clearingZones;

    // Infill passes (optional concentric or raster toolpaths)
    std::vector<AnalyticalPath> infillToolPaths;

    bool empty() const {
        return pockets.empty() && bossToolpaths.empty() && clearingZones.empty();
    }
};

// Generates zig-zag floor clearing toolpaths covering requiredCoverage with optimal angle
std::vector<AnalyticalPath> generateFloorClearingToolpaths(
    const LevelClearingZones& zones,
    double toolDiameter = 2.0,
    double stepoverPercent = 70.0);

// Orchestrates boolean subtraction of all islands from the material boundary for a level
LevelMachiningToolpaths computeLevelMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevelPaths& levelPaths,
    double toolDiameter = 2.0,
    double bossAvoidancePercent = 20.0,
    double overcutPercent = 30.0,
    double stepoverPercent = 70.0);

// Batch compute for all levels in a part
std::vector<LevelMachiningToolpaths> computeAllPartMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const std::vector<MachiningLevel>& levels,
    const std::vector<ShapePaths>& allShapePaths);

#endif // MACHININGPATHS_H
