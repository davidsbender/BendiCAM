#include "MachiningPaths.h"
#include <cmath>
#include <algorithm>

LevelMachiningToolpaths computeLevelMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevelPaths& levelPaths,
    double toolDiameter,
    double bossAvoidancePercent,
    double overcutPercent)
{
    LevelMachiningToolpaths result;
    result.levelIndex = levelPaths.levelIndex;
    result.depth = levelPaths.depth;
    result.belowShapeId = levelPaths.belowShapeId;

    double toolRadius = toolDiameter * 0.5;
    if (bossAvoidancePercent < 0.0) bossAvoidancePercent = 0.0;
    if (bossAvoidancePercent > 100.0) bossAvoidancePercent = 100.0;
    double avoidanceMargin = (bossAvoidancePercent / 100.0) * toolDiameter;

    if (overcutPercent < 0.0) overcutPercent = 0.0;
    double overcutDist = (overcutPercent / 100.0) * toolDiameter;
    double reqHoleOffset = std::max(0.0, toolDiameter - overcutDist);

    // 1. Collect concrete boss wall outline milling toolpaths for EVERY boss/island on this level
    for (const auto* island : levelPaths.islands) {
        if (!island || island->island.toolPath.empty()) continue;
        for (const auto& bossLoop : island->island.toolPath) {
            if (bossLoop.size() < 2) continue;
            AnalyticalPath cleanBoss = bossLoop;
            ensureLoopOrientation(cleanBoss, true);
            result.bossToolpaths.push_back(cleanBoss);
        }
    }

    if (!levelPaths.material || levelPaths.material->material.toolPath.empty()) {
        // No floor/material shape below: this level's shapes are root bosses on the datum bed (Z = 0)
        return result;
    }

    // 2. Compute the Two Geometric Clearing Domains for Floor Clearing
    // A. allowedEnvelope (hard safety roaming boundary for cutter center)
    // Outer: floor boundary expanded into stock clearance/air (+toolRadius)
    if (!levelPaths.material->material.cutZoneOuter.empty()) {
        for (const auto& loop : levelPaths.material->material.cutZoneOuter) {
            if (loop.size() < 2) continue;
            auto airLoops = offsetAnalyticalPath(loop, -toolRadius);
            for (auto airLoop : airLoops) {
                ensureLoopOrientation(airLoop, true);
                result.clearingZones.allowedEnvelope.outerBoundaries.push_back(airLoop);
            }
        }
    } else {
        for (const auto& loop : levelPaths.material->material.toolPath) {
            if (loop.size() < 2) continue;
            auto airLoops = offsetAnalyticalPath(loop, -2.0 * toolRadius);
            for (auto airLoop : airLoops) {
                ensureLoopOrientation(airLoop, true);
                result.clearingZones.allowedEnvelope.outerBoundaries.push_back(airLoop);
            }
        }
    }

    // Holes: boss outline offset outward by avoidanceMargin (for zero margin, aligns exactly with boss outline)
    for (const auto* island : levelPaths.islands) {
        if (!island || island->island.outline.empty()) continue;
        const auto& baseLoops = island->island.outline;
        if (avoidanceMargin <= 1e-6) {
            for (const auto& loop : baseLoops) {
                if (loop.size() < 2) continue;
                AnalyticalPath cleanLoop = loop;
                ensureLoopOrientation(cleanLoop, true);
                result.clearingZones.allowedEnvelope.holeBoundaries.push_back(cleanLoop);
            }
        } else {
            for (const auto& loop : baseLoops) {
                if (loop.size() < 2) continue;
                auto avoidLoops = offsetAnalyticalPath(loop, -avoidanceMargin);
                if (avoidLoops.empty()) {
                    AnalyticalPath cleanLoop = loop;
                    ensureLoopOrientation(cleanLoop, true);
                    result.clearingZones.allowedEnvelope.holeBoundaries.push_back(cleanLoop);
                } else {
                    for (auto avLoop : avoidLoops) {
                        ensureLoopOrientation(avLoop, true);
                        result.clearingZones.allowedEnvelope.holeBoundaries.push_back(avLoop);
                    }
                }
            }
        }
    }

    // B. requiredCoverage (mandatory target floor platform to evacuate)
    // Outer: Stock outer cut zone
    const auto& outerSources = !levelPaths.material->material.cutZoneOuter.empty()
        ? levelPaths.material->material.cutZoneOuter
        : levelPaths.material->material.toolPath;
    for (const auto& loop : outerSources) {
        if (loop.size() < 2) continue;
        AnalyticalPath cleanLoop = loop;
        ensureLoopOrientation(cleanLoop, true);
        result.clearingZones.requiredCoverage.outerBoundaries.push_back(cleanLoop);
    }

    // Holes: island outline offset outwards by [toolDiameter - overcutDist]
    for (const auto* island : levelPaths.islands) {
        if (!island || island->island.outline.empty()) continue;
        const auto& baseLoops = island->island.outline;
        if (reqHoleOffset <= 1e-6) {
            for (const auto& loop : baseLoops) {
                if (loop.size() < 2) continue;
                AnalyticalPath cleanLoop = loop;
                ensureLoopOrientation(cleanLoop, true);
                result.clearingZones.requiredCoverage.holeBoundaries.push_back(cleanLoop);
            }
        } else {
            for (const auto& loop : baseLoops) {
                if (loop.size() < 2) continue;
                auto holeLoops = offsetAnalyticalPath(loop, -reqHoleOffset);
                if (holeLoops.empty()) {
                    AnalyticalPath cleanLoop = loop;
                    ensureLoopOrientation(cleanLoop, true);
                    result.clearingZones.requiredCoverage.holeBoundaries.push_back(cleanLoop);
                } else {
                    for (auto hLoop : holeLoops) {
                        ensureLoopOrientation(hLoop, true);
                        result.clearingZones.requiredCoverage.holeBoundaries.push_back(hLoop);
                    }
                }
            }
        }
    }

    // 3. (Legacy/Transitional) Initialize active pockets with boolean subtraction
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
