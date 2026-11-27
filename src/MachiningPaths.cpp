#include "MachiningPaths.h"
#include <cmath>
#include <algorithm>

LevelMachiningToolpaths computeLevelMachiningToolpaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevelPaths& levelPaths,
    double toolDiameter,
    double bossAvoidancePercent,
    double overcutPercent,
    double stepoverPercent)
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

    // 4. Generate continuous zig-zag floor clearing toolpaths covering requiredCoverage
    result.infillToolPaths = generateFloorClearingToolpaths(result.clearingZones, toolDiameter, stepoverPercent);

    return result;
}

static inline double distPt(const DxfPoint& a, const DxfPoint& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

static bool isPointOnArcSweepLocal(const DxfPoint& pt, const DxfPoint& center, double radius, const DxfPoint& sPt, const DxfPoint& ePt, bool isCCW, double tol = 1e-2) {
    if (std::abs(distPt(pt, center) - radius) > tol) return false;
    double a = std::atan2(pt.y - center.y, pt.x - center.x);
    double a1 = std::atan2(sPt.y - center.y, sPt.x - center.x);
    double a2 = std::atan2(ePt.y - center.y, ePt.x - center.x);
    double da = a - a1;
    double sweep = a2 - a1;
    if (isCCW) {
        while (da < -1e-4) da += 2.0 * M_PI;
        while (sweep <= 1e-4) sweep += 2.0 * M_PI;
        return (da <= sweep + 1e-3);
    } else {
        while (da > 1e-4) da -= 2.0 * M_PI;
        while (sweep >= -1e-4) sweep -= 2.0 * M_PI;
        return (std::abs(da) <= std::abs(sweep) + 1e-3);
    }
}

static PathPrimitive rotatePrimitiveLocal(const PathPrimitive& p, double cosA, double sinA) {
    PathPrimitive r = p;
    auto rot = [&](const DxfPoint& pt) {
        return DxfPoint(pt.x * cosA + pt.y * sinA, -pt.x * sinA + pt.y * cosA);
    };
    r.startPt = rot(p.startPt);
    r.endPt = rot(p.endPt);
    if (p.type == PrimitiveType::ARC) {
        r.center = rot(p.center);
    }
    return r;
}

static void intersectHLineLocal(double Y, const PathPrimitive& p, std::vector<double>& outX) {
    if (p.type == PrimitiveType::LINE) {
        double y1 = p.startPt.y, y2 = p.endPt.y;
        double x1 = p.startPt.x, x2 = p.endPt.x;
        if ((y1 < Y && y2 >= Y) || (y2 < Y && y1 >= Y)) {
            double t = (Y - y1) / (y2 - y1);
            outX.push_back(x1 + t * (x2 - x1));
        }
    } else if (p.type == PrimitiveType::ARC) {
        double cy = p.center.y, cx = p.center.x, r = p.radius;
        if (std::abs(Y - cy) < r - 1e-4) {
            double dx = std::sqrt(r * r - (Y - cy) * (Y - cy));
            DxfPoint ptA(cx - dx, Y);
            DxfPoint ptB(cx + dx, Y);
            if (isPointOnArcSweepLocal(ptA, p.center, r, p.startPt, p.endPt, p.isCCW)) outX.push_back(ptA.x);
            if (isPointOnArcSweepLocal(ptB, p.center, r, p.startPt, p.endPt, p.isCCW)) outX.push_back(ptB.x);
        }
    }
}

struct ClearingCutSegment {
    int passIdx;
    double y;
    double xStart;
    double xEnd;
    bool visited;
};

struct Interval1D {
    double start;
    double end;
};

static std::vector<Interval1D> mergeIntervals1D(std::vector<Interval1D> intervals) {
    if (intervals.empty()) return {};
    std::sort(intervals.begin(), intervals.end(), [](const Interval1D& a, const Interval1D& b) {
        return a.start < b.start;
    });
    std::vector<Interval1D> merged;
    merged.push_back(intervals[0]);
    for (size_t i = 1; i < intervals.size(); ++i) {
        if (intervals[i].start <= merged.back().end + 1e-4) {
            merged.back().end = std::max(merged.back().end, intervals[i].end);
        } else {
            merged.push_back(intervals[i]);
        }
    }
    return merged;
}

static std::vector<Interval1D> subtractIntervals1D(const std::vector<Interval1D>& outer, const std::vector<Interval1D>& holes) {
    std::vector<Interval1D> current = outer;
    for (const auto& h : holes) {
        std::vector<Interval1D> next;
        for (const auto& c : current) {
            if (h.end <= c.start + 1e-4 || h.start >= c.end - 1e-4) {
                // No overlap
                next.push_back(c);
            } else {
                // Overlap: carve hole out of current interval
                if (h.start > c.start + 1e-4) {
                    next.push_back({c.start, h.start});
                }
                if (h.end < c.end - 1e-4) {
                    next.push_back({h.end, c.end});
                }
            }
        }
        current = next;
    }
    std::vector<Interval1D> result;
    for (const auto& inv : current) {
        if (inv.end - inv.start > 1e-3) {
            result.push_back(inv);
        }
    }
    return result;
}

static void extractLoopIntervals1D(double Y, const std::vector<PathPrimitive>& loop, std::vector<Interval1D>& outIntervals) {
    std::vector<double> xs;
    for (const auto& p : loop) intersectHLineLocal(Y, p, xs);
    if (xs.size() < 2) return;
    std::sort(xs.begin(), xs.end());
    std::vector<double> uXs;
    for (double x : xs) {
        if (uXs.empty() || std::abs(x - uXs.back()) > 1e-3) uXs.push_back(x);
    }
    if (uXs.size() % 2 != 0) {
        std::vector<double> paired;
        for (size_t k = 0; k < xs.size(); ++k) {
            if (k + 1 < xs.size() && std::abs(xs[k+1] - xs[k]) < 0.05) {
                k++; // Tangency touch: skip
            } else {
                if (paired.empty() || std::abs(xs[k] - paired.back()) > 1e-3) {
                    paired.push_back(xs[k]);
                }
            }
        }
        if (paired.size() % 2 == 0) {
            uXs = paired;
        } else if (uXs.size() > 1) {
            uXs.pop_back();
        }
    }
    for (size_t k = 0; k + 1 < uXs.size(); k += 2) {
        if (uXs[k+1] - uXs[k] > 1e-3) {
            outIntervals.push_back({uXs[k], uXs[k+1]});
        }
    }
}

std::vector<AnalyticalPath> generateFloorClearingToolpaths(
    const LevelClearingZones& zones,
    double toolDiameter,
    double stepoverPercent)
{
    std::vector<AnalyticalPath> result;
    if (zones.requiredCoverage.outerBoundaries.empty()) return result;

    if (toolDiameter <= 1e-4) toolDiameter = 2.0;
    if (stepoverPercent <= 0.0) stepoverPercent = 70.0;
    if (stepoverPercent > 95.0) stepoverPercent = 95.0;

    double maxStepover = (stepoverPercent / 100.0) * toolDiameter;

    double minTotalLength = 1e12;
    std::vector<AnalyticalPath> bestPaths;

    for (int deg = 0; deg < 180; deg += 10) {
        double rad = deg * M_PI / 180.0;
        double cosA = std::cos(rad), sinA = std::sin(rad);

        std::vector<std::vector<PathPrimitive>> rotOuterLoops;
        rotOuterLoops.reserve(zones.requiredCoverage.outerBoundaries.size());
        double minY = 1e9, maxY = -1e9;
        for (const auto& loop : zones.requiredCoverage.outerBoundaries) {
            std::vector<PathPrimitive> rotLoop;
            rotLoop.reserve(loop.size());
            for (const auto& p : loop) {
                auto rp = rotatePrimitiveLocal(p, cosA, sinA);
                rotLoop.push_back(rp);
                minY = std::min({minY, rp.startPt.y, rp.endPt.y});
                maxY = std::max({maxY, rp.startPt.y, rp.endPt.y});
                if (rp.type == PrimitiveType::ARC) {
                    minY = std::min(minY, rp.center.y - rp.radius);
                    maxY = std::max(maxY, rp.center.y + rp.radius);
                }
            }
            rotOuterLoops.push_back(rotLoop);
        }

        if (rotOuterLoops.empty() || maxY - minY <= 1e-4) continue;

        std::vector<std::vector<PathPrimitive>> rotHoleLoops;
        rotHoleLoops.reserve(zones.requiredCoverage.holeBoundaries.size());
        for (const auto& loop : zones.requiredCoverage.holeBoundaries) {
            std::vector<PathPrimitive> rotLoop;
            rotLoop.reserve(loop.size());
            for (const auto& p : loop) {
                rotLoop.push_back(rotatePrimitiveLocal(p, cosA, sinA));
            }
            rotHoleLoops.push_back(rotLoop);
        }

        int N = std::max(1, (int)std::ceil((maxY - minY) / maxStepover));
        double actualStep = (maxY - minY) / N;

        std::vector<ClearingCutSegment> segments;
        for (int i = 0; i < N; ++i) {
            double Y = minY + (i + 0.5) * actualStep;

            // 1. Collect candidate intervals strictly inside the outer boundary
            std::vector<Interval1D> outerInv;
            for (const auto& loop : rotOuterLoops) {
                extractLoopIntervals1D(Y, loop, outerInv);
            }
            outerInv = mergeIntervals1D(outerInv);
            if (outerInv.empty()) continue;

            // 2. Collect forbidden intervals strictly inside hole boundaries
            std::vector<Interval1D> holeInv;
            for (const auto& loop : rotHoleLoops) {
                extractLoopIntervals1D(Y, loop, holeInv);
            }
            holeInv = mergeIntervals1D(holeInv);

            // 3. Exact 1D interval subtraction: (Inside Outer) AND NOT (Inside Hole)
            auto validSegs = subtractIntervals1D(outerInv, holeInv);
            for (const auto& seg : validSegs) {
                segments.push_back({i, Y, seg.start, seg.end, false});
            }
        }

        if (segments.empty()) continue;

        std::vector<AnalyticalPath> paths;
        AnalyticalPath currentPath;
        double totalCutLen = 0.0;
        double totalLinkLen = 0.0;
        size_t visitedCount = 0;
        DxfPoint curPt(0, 0);
        int curPass = -1;
        int passDir = 1;
        bool hasCur = false;

        auto unrot = [&](double x, double y) {
            return DxfPoint(x * cosA - y * sinA, x * sinA + y * cosA);
        };

        while (visitedCount < segments.size()) {
            int bestSegIdx = -1;
            bool enterFromLeft = true;
            double bestDist = 1e12;

            if (!hasCur) {
                for (size_t s = 0; s < segments.size(); ++s) {
                    if (!segments[s].visited) {
                        bestSegIdx = (int)s;
                        enterFromLeft = true;
                        passDir = 1;
                        break;
                    }
                }
            } else {
                int targetPass = curPass + passDir;
                for (size_t s = 0; s < segments.size(); ++s) {
                    if (segments[s].visited || segments[s].passIdx != targetPass) continue;
                    DxfPoint pL(segments[s].xStart, segments[s].y);
                    DxfPoint pR(segments[s].xEnd, segments[s].y);
                    double dL = distPt(curPt, pL);
                    double dR = distPt(curPt, pR);
                    if (dL < bestDist) { bestDist = dL; bestSegIdx = (int)s; enterFromLeft = true; }
                    if (dR < bestDist) { bestDist = dR; bestSegIdx = (int)s; enterFromLeft = false; }
                }

                if (bestSegIdx < 0) {
                    targetPass = curPass - passDir;
                    for (size_t s = 0; s < segments.size(); ++s) {
                        if (segments[s].visited || segments[s].passIdx != targetPass) continue;
                        DxfPoint pL(segments[s].xStart, segments[s].y);
                        DxfPoint pR(segments[s].xEnd, segments[s].y);
                        double dL = distPt(curPt, pL);
                        double dR = distPt(curPt, pR);
                        if (dL < bestDist) { bestDist = dL; bestSegIdx = (int)s; enterFromLeft = true; }
                        if (dR < bestDist) { bestDist = dR; bestSegIdx = (int)s; enterFromLeft = false; }
                    }
                    if (bestSegIdx >= 0) passDir = -passDir;
                }

                if (bestSegIdx < 0) {
                    for (size_t s = 0; s < segments.size(); ++s) {
                        if (segments[s].visited) continue;
                        DxfPoint pL(segments[s].xStart, segments[s].y);
                        DxfPoint pR(segments[s].xEnd, segments[s].y);
                        double dL = distPt(curPt, pL);
                        double dR = distPt(curPt, pR);
                        if (dL < bestDist) { bestDist = dL; bestSegIdx = (int)s; enterFromLeft = true; }
                        if (dR < bestDist) { bestDist = dR; bestSegIdx = (int)s; enterFromLeft = false; }
                    }
                    passDir = 1;
                }
            }

            if (bestSegIdx < 0) break;

            auto& seg = segments[bestSegIdx];
            seg.visited = true;
            visitedCount++;

            DxfPoint entryPt = enterFromLeft ? DxfPoint(seg.xStart, seg.y) : DxfPoint(seg.xEnd, seg.y);
            DxfPoint exitPt  = enterFromLeft ? DxfPoint(seg.xEnd, seg.y)   : DxfPoint(seg.xStart, seg.y);

            if (hasCur) {
                double dLink = distPt(curPt, entryPt);
                totalLinkLen += dLink;
                if (dLink <= 2.5 * actualStep && std::abs(seg.passIdx - curPass) == 1) {
                    PathPrimitive linkPrim;
                    linkPrim.type = PrimitiveType::LINE;
                    linkPrim.startPt = unrot(curPt.x, curPt.y);
                    linkPrim.endPt = unrot(entryPt.x, entryPt.y);
                    currentPath.push_back(linkPrim);
                } else {
                    if (!currentPath.empty()) paths.push_back(currentPath);
                    currentPath.clear();
                }
            }

            PathPrimitive cutPrim;
            cutPrim.type = PrimitiveType::LINE;
            cutPrim.startPt = unrot(entryPt.x, entryPt.y);
            cutPrim.endPt = unrot(exitPt.x, exitPt.y);
            currentPath.push_back(cutPrim);

            totalCutLen += distPt(entryPt, exitPt);
            curPt = exitPt;
            curPass = seg.passIdx;
            hasCur = true;
        }

        if (!currentPath.empty()) paths.push_back(currentPath);

        double totalLen = totalCutLen + totalLinkLen;
        if (totalLen < minTotalLength) {
            minTotalLength = totalLen;
            bestPaths = paths;
        }
    }

    return bestPaths;
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
