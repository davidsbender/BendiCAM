#include "MachiningPaths.h"
#include <cmath>
#include <algorithm>

LevelMachiningToolpaths computeLevelMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevelPaths& levelPaths)
{
    LevelMachiningToolpaths result;
    result.levelIndex = levelPaths.levelIndex;
    result.depth = levelPaths.depth;
    result.belowShapeId = levelPaths.belowShapeId;

    if (!levelPaths.material || levelPaths.material->material.toolPath.empty()) {
        // No floor/material shape below: this level's shapes are root bosses on the datum bed (Z = 0)
        for (const auto* island : levelPaths.islands) {
            if (!island || island->island.toolPath.empty()) continue;
            for (const auto& bossLoop : island->island.toolPath) {
                if (bossLoop.size() < 2) continue;
                AnalyticalPath cleanBoss = bossLoop;
                ensureLoopOrientation(cleanBoss, true);
                result.bossToolpaths.push_back(cleanBoss);
            }
        }
        return result;
    }

    // 1. Initialize active pockets with the material/floor toolpaths
    std::vector<MachiningPocket> activePockets;
    for (const auto& mLoop : levelPaths.material->material.toolPath) {
        if (mLoop.size() < 2) continue;
        MachiningPocket p;
        p.outerBoundary = mLoop;
        ensureLoopOrientation(p.outerBoundary, true);
        activePockets.push_back(p);
    }

    // 2. Sequentially subtract each island boss toolpath
    for (const auto* island : levelPaths.islands) {
        if (!island || island->island.toolPath.empty()) continue;

        for (const auto& bossLoop : island->island.toolPath) {
            if (bossLoop.size() < 2) continue;
            AnalyticalPath cleanBoss = bossLoop;
            ensureLoopOrientation(cleanBoss, true);

            std::vector<MachiningPocket> nextPockets;

            for (const auto& curPocket : activePockets) {
                BooleanSubtractionResult sub = subtractAnalyticalLoops(curPocket.outerBoundary, cleanBoss);

                if (sub.type == SubtractionResultType::EMPTY) {
                    // Pocket completely covered by boss; eliminated
                    continue;
                } else if (sub.type == SubtractionResultType::DISJOINT) {
                    // Boss is completely outside this pocket; keep unchanged
                    nextPockets.push_back(curPocket);
                } else if (sub.type == SubtractionResultType::ISLAND_INSIDE) {
                    // Case 2: Boss is strictly within pocket boundary -> add as island
                    MachiningPocket updatedPocket = curPocket;
                    updatedPocket.islandBoundaries.push_back(cleanBoss);
                    nextPockets.push_back(updatedPocket);
                } else if (sub.type == SubtractionResultType::CLIPPED_LOOPS) {
                    // Case 3: Boss intersects pocket boundary -> 1 or more new outer boundaries
                    for (const auto& newOuter : sub.loops) {
                        MachiningPocket newPocket;
                        newPocket.outerBoundary = newOuter;

                        // Re-distribute any existing island boundaries into the child pocket if contained
                        for (const auto& existingIsland : curPocket.islandBoundaries) {
                            if (loopContainsLoop(newPocket.outerBoundary, existingIsland)) {
                                newPocket.islandBoundaries.push_back(existingIsland);
                            }
                        }
                        nextPockets.push_back(newPocket);
                    }
                }
            }

            activePockets = nextPockets;
        }
    }

    result.pockets = activePockets;
    return result;
}

std::vector<LevelMachiningToolpaths> computeAllPartMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const std::vector<MachiningLevel>& levels,
    const std::vector<ShapePaths>& allShapePaths)
{
    std::vector<LevelMachiningToolpaths> result;
    result.reserve(levels.size());

    for (const auto& level : levels) {
        MachiningLevelPaths lp = computeMachiningLevelPaths(model, part, level, allShapePaths);
        result.push_back(computeLevelMachiningToolpaths(model, part, lp));
    }

    return result;
}
