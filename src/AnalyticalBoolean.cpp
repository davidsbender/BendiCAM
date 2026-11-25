#include "AnalyticalBoolean.h"
#include <cmath>
#include <algorithm>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double ptDist(const DxfPoint& p1, const DxfPoint& p2) {
    return std::hypot(p1.x - p2.x, p1.y - p2.y);
}

DxfPoint evaluatePrimitiveParam(const PathPrimitive& prim, double t) {
    t = std::max(0.0, std::min(1.0, t));
    if (prim.type == PrimitiveType::LINE) {
        return DxfPoint(prim.startPt.x + t * (prim.endPt.x - prim.startPt.x),
                        prim.startPt.y + t * (prim.endPt.y - prim.startPt.y));
    } else {
        if (prim.radius <= 1e-6) return prim.startPt;
        double a1 = std::atan2(prim.startPt.y - prim.center.y, prim.startPt.x - prim.center.x);
        double a2 = std::atan2(prim.endPt.y - prim.center.y, prim.endPt.x - prim.center.x);
        double da = a2 - a1;
        if (prim.isCCW) {
            while (da <= 0.0) da += 2.0 * M_PI;
        } else {
            while (da >= 0.0) da -= 2.0 * M_PI;
        }
        double phi = a1 + da * t;
        return DxfPoint(prim.center.x + prim.radius * std::cos(phi),
                        prim.center.y + prim.radius * std::sin(phi));
    }
}

std::vector<PrimitiveIntersection> intersectPrimitivesAll(
    const PathPrimitive& p1,
    const PathPrimitive& p2)
{
    std::vector<PrimitiveIntersection> results;

    if (p1.type == PrimitiveType::LINE && p2.type == PrimitiveType::LINE) {
        double dx1 = p1.endPt.x - p1.startPt.x;
        double dy1 = p1.endPt.y - p1.startPt.y;
        double dx2 = p2.endPt.x - p2.startPt.x;
        double dy2 = p2.endPt.y - p2.startPt.y;
        double det = dx1 * dy2 - dy1 * dx2;
        if (std::abs(det) > 1e-9) {
            double t1 = ((p2.startPt.x - p1.startPt.x) * dy2 - (p2.startPt.y - p1.startPt.y) * dx2) / det;
            double t2 = ((p2.startPt.x - p1.startPt.x) * dy1 - (p2.startPt.y - p1.startPt.y) * dx1) / det;
            if (t1 >= -1e-6 && t1 <= 1.0 + 1e-6 && t2 >= -1e-6 && t2 <= 1.0 + 1e-6) {
                t1 = std::max(0.0, std::min(1.0, t1));
                t2 = std::max(0.0, std::min(1.0, t2));
                DxfPoint pt(p1.startPt.x + t1 * dx1, p1.startPt.y + t1 * dy1);
                results.push_back({pt, t1, t2});
            }
        }
    } else if (p1.type == PrimitiveType::LINE && p2.type == PrimitiveType::ARC) {
        double dx = p1.endPt.x - p1.startPt.x;
        double dy = p1.endPt.y - p1.startPt.y;
        double len = std::hypot(dx, dy);
        if (len > 1e-9 && p2.radius > 1e-6) {
            double ux = dx / len;
            double uy = dy / len;
            double delx = p1.startPt.x - p2.center.x;
            double dely = p1.startPt.y - p2.center.y;
            double B = 2.0 * (delx * ux + dely * uy);
            double C = (delx * delx + dely * dely) - p2.radius * p2.radius;
            double disc = B * B - 4.0 * C;
            if (disc >= -1e-9) {
                double sqD = (disc > 0.0) ? std::sqrt(disc) : 0.0;
                double sRoots[2] = { (-B - sqD) * 0.5, (-B + sqD) * 0.5 };
                int numRoots = (sqD < 1e-7) ? 1 : 2;

                for (int r = 0; r < numRoots; ++r) {
                    double t1 = sRoots[r] / len;
                    if (t1 >= -1e-6 && t1 <= 1.0 + 1e-6) {
                        t1 = std::max(0.0, std::min(1.0, t1));
                        DxfPoint pt(p1.startPt.x + t1 * dx, p1.startPt.y + t1 * dy);

                        double a1 = std::atan2(p2.startPt.y - p2.center.y, p2.startPt.x - p2.center.x);
                        double a2 = std::atan2(p2.endPt.y - p2.center.y, p2.endPt.x - p2.center.x);
                        double ap = std::atan2(pt.y - p2.center.y, pt.x - p2.center.x);

                        double da = a2 - a1;
                        double dap = ap - a1;
                        if (p2.isCCW) {
                            while (da <= 0.0) da += 2.0 * M_PI;
                            while (dap < 0.0) dap += 2.0 * M_PI;
                        } else {
                            while (da >= 0.0) da -= 2.0 * M_PI;
                            while (dap > 0.0) dap -= 2.0 * M_PI;
                        }
                        double t2 = (std::abs(da) > 1e-9) ? (dap / da) : 0.0;
                        if (t2 >= -1e-6 && t2 <= 1.0 + 1e-6) {
                            t2 = std::max(0.0, std::min(1.0, t2));
                            results.push_back({pt, t1, t2});
                        }
                    }
                }
            }
        }
    } else if (p1.type == PrimitiveType::ARC && p2.type == PrimitiveType::LINE) {
        auto subRes = intersectPrimitivesAll(p2, p1);
        for (const auto& inter : subRes) {
            results.push_back({inter.pt, inter.t2, inter.t1});
        }
    } else { // ARC - ARC
        if (p1.radius > 1e-6 && p2.radius > 1e-6) {
            double d = ptDist(p1.center, p2.center);
            if (d > 1e-7 && d <= p1.radius + p2.radius + 1e-6 && d >= std::abs(p1.radius - p2.radius) - 1e-6) {
                double a = (p1.radius * p1.radius - p2.radius * p2.radius + d * d) / (2.0 * d);
                double h2 = p1.radius * p1.radius - a * a;
                double h = (h2 > 0.0) ? std::sqrt(h2) : 0.0;

                double p0x = p1.center.x + a * (p2.center.x - p1.center.x) / d;
                double p0y = p1.center.y + a * (p2.center.y - p1.center.y) / d;
                double nx = -(p2.center.y - p1.center.y) / d;
                double ny = (p2.center.x - p1.center.x) / d;

                DxfPoint cands[2] = {
                    DxfPoint(p0x + h * nx, p0y + h * ny),
                    DxfPoint(p0x - h * nx, p0y - h * ny)
                };
                int numCands = (h < 1e-7) ? 1 : 2;

                for (int c = 0; c < numCands; ++c) {
                    const auto& pt = cands[c];

                    // Param on p1
                    double a1_1 = std::atan2(p1.startPt.y - p1.center.y, p1.startPt.x - p1.center.x);
                    double a2_1 = std::atan2(p1.endPt.y - p1.center.y, p1.endPt.x - p1.center.x);
                    double ap_1 = std::atan2(pt.y - p1.center.y, pt.x - p1.center.x);
                    double da_1 = a2_1 - a1_1;
                    double dap_1 = ap_1 - a1_1;
                    if (p1.isCCW) {
                        while (da_1 <= 0.0) da_1 += 2.0 * M_PI;
                        while (dap_1 < 0.0) dap_1 += 2.0 * M_PI;
                    } else {
                        while (da_1 >= 0.0) da_1 -= 2.0 * M_PI;
                        while (dap_1 > 0.0) dap_1 -= 2.0 * M_PI;
                    }
                    double t1 = (std::abs(da_1) > 1e-9) ? (dap_1 / da_1) : 0.0;

                    // Param on p2
                    double a1_2 = std::atan2(p2.startPt.y - p2.center.y, p2.startPt.x - p2.center.x);
                    double a2_2 = std::atan2(p2.endPt.y - p2.center.y, p2.endPt.x - p2.center.x);
                    double ap_2 = std::atan2(pt.y - p2.center.y, pt.x - p2.center.x);
                    double da_2 = a2_2 - a1_2;
                    double dap_2 = ap_2 - a1_2;
                    if (p2.isCCW) {
                        while (da_2 <= 0.0) da_2 += 2.0 * M_PI;
                        while (dap_2 < 0.0) dap_2 += 2.0 * M_PI;
                    } else {
                        while (da_2 >= 0.0) da_2 -= 2.0 * M_PI;
                        while (dap_2 > 0.0) dap_2 -= 2.0 * M_PI;
                    }
                    double t2 = (std::abs(da_2) > 1e-9) ? (dap_2 / da_2) : 0.0;

                    if (t1 >= -1e-6 && t1 <= 1.0 + 1e-6 && t2 >= -1e-6 && t2 <= 1.0 + 1e-6) {
                        t1 = std::max(0.0, std::min(1.0, t1));
                        t2 = std::max(0.0, std::min(1.0, t2));
                        results.push_back({pt, t1, t2});
                    }
                }
            }
        }
    }

    // Deduplicate close intersection points
    std::vector<PrimitiveIntersection> uniqueResults;
    for (const auto& inter : results) {
        bool dup = false;
        for (const auto& u : uniqueResults) {
            if (ptDist(inter.pt, u.pt) < 1e-4) {
                dup = true;
                break;
            }
        }
        if (!dup) uniqueResults.push_back(inter);
    }
    return uniqueResults;
}

std::pair<PathPrimitive, PathPrimitive> splitPrimitive(const PathPrimitive& prim, double t) {
    t = std::max(0.0, std::min(1.0, t));
    DxfPoint splitPt = evaluatePrimitiveParam(prim, t);

    PathPrimitive sub1, sub2;
    if (prim.type == PrimitiveType::LINE) {
        sub1 = PathPrimitive::CreateLine(prim.startPt, splitPt);
        sub2 = PathPrimitive::CreateLine(splitPt, prim.endPt);
    } else {
        sub1 = PathPrimitive::CreateArc(prim.startPt, splitPt, prim.center, prim.radius, prim.isCCW);
        sub2 = PathPrimitive::CreateArc(splitPt, prim.endPt, prim.center, prim.radius, prim.isCCW);
    }

    sub1.hasEndSpur = false;
    sub2.hasEndSpur = prim.hasEndSpur;
    sub2.endSpurApex = prim.endSpurApex;

    return {sub1, sub2};
}

PathPrimitive reversePrimitiveExact(const PathPrimitive& prim) {
    PathPrimitive rev;
    rev.type = prim.type;
    rev.startPt = prim.endPt;
    rev.endPt = prim.startPt;
    rev.center = prim.center;
    rev.radius = prim.radius;
    rev.isCCW = !prim.isCCW;
    rev.hasEndSpur = false;
    return rev;
}

bool loopContainsLoop(const AnalyticalPath& outer, const AnalyticalPath& inner) {
    if (outer.empty() || inner.empty()) return false;
    auto samplePts = sampleAnalyticalPath(inner, 0.5);
    if (samplePts.empty()) return false;
    int insideCount = 0;
    for (const auto& pt : samplePts) {
        if (isPointInsideAnalyticalPath(pt, outer)) {
            insideCount++;
        }
    }
    return ((double)insideCount / samplePts.size()) > 0.85;
}

static void computePathBoundingBox(const AnalyticalPath& path, double& minX, double& minY, double& maxX, double& maxY) {
    minX = minY = std::numeric_limits<double>::max();
    maxX = maxY = -std::numeric_limits<double>::max();
    for (const auto& prim : path) {
        minX = std::min({minX, prim.startPt.x, prim.endPt.x});
        minY = std::min({minY, prim.startPt.y, prim.endPt.y});
        maxX = std::max({maxX, prim.startPt.x, prim.endPt.x});
        maxY = std::max({maxY, prim.startPt.y, prim.endPt.y});
        if (prim.type == PrimitiveType::ARC) {
            minX = std::min(minX, prim.center.x - prim.radius);
            minY = std::min(minY, prim.center.y - prim.radius);
            maxX = std::max(maxX, prim.center.x + prim.radius);
            maxY = std::max(maxY, prim.center.y + prim.radius);
        }
    }
}

BooleanSubtractionResult subtractAnalyticalLoops(
    const AnalyticalPath& minuend,
    const AnalyticalPath& subtrahend)
{
    BooleanSubtractionResult result;
    if (minuend.empty()) {
        result.type = SubtractionResultType::EMPTY;
        return result;
    }
    if (subtrahend.empty()) {
        result.type = SubtractionResultType::DISJOINT;
        result.loops.push_back(minuend);
        return result;
    }

    // 1. Fast Bounding Box Reject
    double mMx, mMy, mXx, mXy;
    double sMx, sMy, sXx, sXy;
    computePathBoundingBox(minuend, mMx, mMy, mXx, mXy);
    computePathBoundingBox(subtrahend, sMx, sMy, sXx, sXy);

    if (mMx > sXx + 1e-4 || mXx < sMx - 1e-4 || mMy > sXy + 1e-4 || mXy < sMy - 1e-4) {
        result.type = SubtractionResultType::DISJOINT;
        result.loops.push_back(minuend);
        return result;
    }

    // 2. Find All Intersections
    size_t numM = minuend.size();
    size_t numS = subtrahend.size();
    std::vector<std::vector<double>> mSplits(numM);
    std::vector<std::vector<double>> sSplits(numS);
    size_t totalIntersections = 0;

    for (size_t i = 0; i < numM; ++i) {
        for (size_t j = 0; j < numS; ++j) {
            auto inters = intersectPrimitivesAll(minuend[i], subtrahend[j]);
            for (const auto& inter : inters) {
                mSplits[i].push_back(inter.t1);
                sSplits[j].push_back(inter.t2);
                totalIntersections++;
            }
        }
    }

    // 3. Containment Checks (When Boundaries Do Not Intersect)
    if (totalIntersections == 0) {
        if (loopContainsLoop(subtrahend, minuend)) {
            result.type = SubtractionResultType::EMPTY;
            return result;
        }
        if (loopContainsLoop(minuend, subtrahend)) {
            result.type = SubtractionResultType::ISLAND_INSIDE;
            result.loops.push_back(minuend);
            return result;
        }
        result.type = SubtractionResultType::DISJOINT;
        result.loops.push_back(minuend);
        return result;
    }

    // 4. Subdivide Minuend Primitives
    std::vector<PathPrimitive> subdividedM;
    for (size_t i = 0; i < numM; ++i) {
        auto& cuts = mSplits[i];
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double a, double b) {
            return std::abs(a - b) < 1e-4;
        }), cuts.end());

        // Remove extreme endpoints
        cuts.erase(std::remove_if(cuts.begin(), cuts.end(), [](double t) {
            return t < 1e-4 || t > 1.0 - 1e-4;
        }), cuts.end());

        if (cuts.empty()) {
            subdividedM.push_back(minuend[i]);
        } else {
            PathPrimitive curr = minuend[i];
            double prevT = 0.0;
            for (double t : cuts) {
                double relT = (t - prevT) / (1.0 - prevT);
                auto parts = splitPrimitive(curr, relT);
                subdividedM.push_back(parts.first);
                curr = parts.second;
                prevT = t;
            }
            subdividedM.push_back(curr);
        }
    }

    // 5. Subdivide Subtrahend Primitives
    std::vector<PathPrimitive> subdividedS;
    for (size_t j = 0; j < numS; ++j) {
        auto& cuts = sSplits[j];
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end(), [](double a, double b) {
            return std::abs(a - b) < 1e-4;
        }), cuts.end());

        cuts.erase(std::remove_if(cuts.begin(), cuts.end(), [](double t) {
            return t < 1e-4 || t > 1.0 - 1e-4;
        }), cuts.end());

        if (cuts.empty()) {
            subdividedS.push_back(subtrahend[j]);
        } else {
            PathPrimitive curr = subtrahend[j];
            double prevT = 0.0;
            for (double t : cuts) {
                double relT = (t - prevT) / (1.0 - prevT);
                auto parts = splitPrimitive(curr, relT);
                subdividedS.push_back(parts.first);
                curr = parts.second;
                prevT = t;
            }
            subdividedS.push_back(curr);
        }
    }

    // 6. Classify Segments
    std::vector<PathPrimitive> keptSegments;

    // Minuend segments: Keep if OUTSIDE subtrahend
    for (const auto& seg : subdividedM) {
        DxfPoint mid = evaluatePrimitiveParam(seg, 0.5);
        if (!isPointInsideAnalyticalPath(mid, subtrahend)) {
            keptSegments.push_back(seg);
        }
    }

    // Subtrahend segments: Keep and REVERSE if INSIDE minuend
    for (const auto& seg : subdividedS) {
        DxfPoint mid = evaluatePrimitiveParam(seg, 0.5);
        if (isPointInsideAnalyticalPath(mid, minuend)) {
            keptSegments.push_back(reversePrimitiveExact(seg));
        }
    }

    if (keptSegments.empty()) {
        result.type = SubtractionResultType::EMPTY;
        return result;
    }

    // 7. Directed Traversal & Loop Assembly
    std::vector<bool> visited(keptSegments.size(), false);

    for (size_t startIdx = 0; startIdx < keptSegments.size(); ++startIdx) {
        if (visited[startIdx]) continue;

        AnalyticalPath loop;
        loop.trace = minuend.trace;
        size_t curr = startIdx;
        bool closed = false;

        while (!visited[curr]) {
            visited[curr] = true;
            loop.push_back(keptSegments[curr]);

            DxfPoint needPt = keptSegments[curr].endPt;
            if (ptDist(needPt, loop.front().startPt) < 1e-3 && loop.size() >= 2) {
                closed = true;
                break;
            }

            int nextIdx = -1;
            double bestDist = 1e9;
            for (size_t k = 0; k < keptSegments.size(); ++k) {
                if (!visited[k]) {
                    double d = ptDist(needPt, keptSegments[k].startPt);
                    if (d < bestDist) {
                        bestDist = d;
                        nextIdx = (int)k;
                    }
                }
            }

            if (nextIdx != -1 && bestDist < 1e-2) {
                curr = nextIdx;
            } else {
                if (ptDist(needPt, loop.front().startPt) < 1e-2) {
                    closed = true;
                }
                break;
            }
        }

        if (closed && loop.size() >= 2) {
            AnalyticalPath cleanLoop = cleanDegeneratePrimitives(loop);
            ensureLoopOrientation(cleanLoop, true);
            if (getAnalyticalPathArea(cleanLoop) > 1e-3) {
                result.loops.push_back(cleanLoop);
            }
        }
    }

    if (result.loops.empty()) {
        result.type = SubtractionResultType::EMPTY;
    } else {
        result.type = SubtractionResultType::CLIPPED_LOOPS;
    }

    return result;
}
