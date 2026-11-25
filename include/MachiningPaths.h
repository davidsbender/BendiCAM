#ifndef MACHININGPATHS_H
#define MACHININGPATHS_H

#include "AnalyticalToolpath.h"
#include "AnalyticalBoolean.h"
#include <vector>

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

    // Infill passes (optional concentric or raster toolpaths)
    std::vector<AnalyticalPath> infillToolPaths;

    bool empty() const { return pockets.empty(); }
};

// Orchestrates boolean subtraction of all islands from the material boundary for a level
LevelMachiningToolpaths computeLevelMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevelPaths& levelPaths);

// Batch compute for all levels in a part
std::vector<LevelMachiningToolpaths> computeAllPartMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const std::vector<MachiningLevel>& levels,
    const std::vector<ShapePaths>& allShapePaths);

#endif // MACHININGPATHS_H
