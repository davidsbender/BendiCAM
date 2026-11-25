#include "AnalyticalToolpath.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double pointDist(const DxfPoint& p1, const DxfPoint& p2) {
    return std::hypot(p1.x - p2.x, p1.y - p2.y);
}

double getPrimitiveLength(const PathPrimitive& prim) {
    if (prim.type == PrimitiveType::LINE) {
        return pointDist(prim.startPt, prim.endPt);
    } else {
        if (prim.radius <= 1e-6) return 0.0;
        double a1 = std::atan2(prim.startPt.y - prim.center.y, prim.startPt.x - prim.center.x);
        double a2 = std::atan2(prim.endPt.y - prim.center.y, prim.endPt.x - prim.center.x);
        double da = a2 - a1;
        if (prim.isCCW) {
            while (da <= 0.0) da += 2.0 * M_PI;
        } else {
            while (da >= 0.0) da -= 2.0 * M_PI;
        }
        return prim.radius * std::abs(da);
    }
}

double getPathLength(const AnalyticalPath& path) {
    double total = 0.0;
    for (const auto& prim : path) {
        total += getPrimitiveLength(prim);
    }
    return total;
}

std::vector<DxfPoint> sampleAnalyticalPath(const AnalyticalPath& path, double maxStepLen) {
    std::vector<DxfPoint> pts;
    if (path.empty()) return pts;

    for (const auto& prim : path) {
        if (prim.type == PrimitiveType::LINE) {
            pts.push_back(prim.startPt);
        } else {
            if (prim.radius <= 1e-6) {
                pts.push_back(prim.startPt);
                continue;
            }
            double a1 = std::atan2(prim.startPt.y - prim.center.y, prim.startPt.x - prim.center.x);
            double a2 = std::atan2(prim.endPt.y - prim.center.y, prim.endPt.x - prim.center.x);
            double da = a2 - a1;
            if (prim.isCCW) {
                while (da <= 0.0) da += 2.0 * M_PI;
            } else {
                while (da >= 0.0) da -= 2.0 * M_PI;
            }

            double arcLen = prim.radius * std::abs(da);
            int steps = std::max(4, (int)std::ceil(arcLen / std::max(0.05, maxStepLen)));

            for (int i = 0; i < steps; ++i) {
                double t = (double)i / steps;
                double phi = a1 + da * t;
                pts.push_back(DxfPoint(prim.center.x + prim.radius * std::cos(phi),
                                       prim.center.y + prim.radius * std::sin(phi)));
            }
        }
    }
    return pts;
}

double getAnalyticalPathArea(const AnalyticalPath& path) {
    auto pts = sampleAnalyticalPath(path, 0.2);
    if (pts.size() < 3) return 0.0;

    double area = 0.0;
    size_t n = pts.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    return area * 0.5;
}

static inline uint64_t fnv1a64(const void* data, size_t bytes, uint64_t seed = 14695981039346656037ULL) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < bytes; ++i) {
        h ^= ptr[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t computeEntityHash(const std::vector<size_t>& entityIndices) {
    if (entityIndices.empty()) return 0;
    std::vector<size_t> sorted = entityIndices;
    std::sort(sorted.begin(), sorted.end());
    return fnv1a64(sorted.data(), sorted.size() * sizeof(size_t));
}

uint64_t computeEntitySignature(uint64_t entityHash, double offsetDistMm) {
    if (entityHash == 0) return 0;
    int64_t offsetMicrons = static_cast<int64_t>(std::round(offsetDistMm * 1000.0));
    uint64_t h = fnv1a64(&entityHash, sizeof(entityHash));
    return fnv1a64(&offsetMicrons, sizeof(offsetMicrons), h);
}

uint64_t computeGeometrySignature(const AnalyticalPath& path) {
    if (path.empty()) return 0;
    double area = std::abs(getAnalyticalPathArea(path));
    int64_t qArea = static_cast<int64_t>(std::round(area * 100.0)); // 0.01 mm^2

    double perim = getPathLength(path);
    int64_t qPerim = static_cast<int64_t>(std::round(perim * 1000.0)); // 0.001 mm

    auto pts = sampleAnalyticalPath(path, 0.5);
    if (pts.empty()) return 0;

    double sumX = 0.0, sumY = 0.0;
    double minX = pts[0].x, maxX = pts[0].x;
    double minY = pts[0].y, maxY = pts[0].y;

    for (const auto& pt : pts) {
        sumX += pt.x;
        sumY += pt.y;
        if (pt.x < minX) minX = pt.x;
        if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y;
        if (pt.y > maxY) maxY = pt.y;
    }

    double cx = sumX / pts.size();
    double cy = sumY / pts.size();

    int64_t qCx = static_cast<int64_t>(std::round(cx * 1000.0));
    int64_t qCy = static_cast<int64_t>(std::round(cy * 1000.0));
    int64_t qMinX = static_cast<int64_t>(std::round(minX * 1000.0));
    int64_t qMinY = static_cast<int64_t>(std::round(minY * 1000.0));
    int64_t qMaxX = static_cast<int64_t>(std::round(maxX * 1000.0));
    int64_t qMaxY = static_cast<int64_t>(std::round(maxY * 1000.0));

    int64_t moments[8] = { qArea, qPerim, qCx, qCy, qMinX, qMinY, qMaxX, qMaxY };
    return fnv1a64(moments, sizeof(moments));
}

void ensureLoopOrientation(AnalyticalPath& path, bool targetCCW) {
    double area = getAnalyticalPathArea(path);
    if (std::abs(area) < 1e-6) return;
    bool isCCW = (area > 0.0);
    if (isCCW != targetCCW) {
        path = reverseAnalyticalPath(path);
    }
}

DxfPoint getPrimitiveStartTangent(const PathPrimitive& prim) {
    if (prim.type == PrimitiveType::LINE) {
        double dx = prim.endPt.x - prim.startPt.x;
        double dy = prim.endPt.y - prim.startPt.y;
        double len = std::hypot(dx, dy);
        if (len < 1e-9) return DxfPoint(1, 0);
        return DxfPoint(dx / len, dy / len);
    } else {
        if (prim.radius <= 1e-6) return DxfPoint(1, 0);
        double rx = (prim.startPt.x - prim.center.x) / prim.radius;
        double ry = (prim.startPt.y - prim.center.y) / prim.radius;
        return prim.isCCW ? DxfPoint(-ry, rx) : DxfPoint(ry, -rx);
    }
}

DxfPoint getPrimitiveEndTangent(const PathPrimitive& prim) {
    if (prim.type == PrimitiveType::LINE) {
        return getPrimitiveStartTangent(prim);
    } else {
        if (prim.radius <= 1e-6) return DxfPoint(1, 0);
        double rx = (prim.endPt.x - prim.center.x) / prim.radius;
        double ry = (prim.endPt.y - prim.center.y) / prim.radius;
        return prim.isCCW ? DxfPoint(-ry, rx) : DxfPoint(ry, -rx);
    }
}

PathPrimitive reversePrimitive(const PathPrimitive& prim) {
    PathPrimitive rev = prim;
    rev.startPt = prim.endPt;
    rev.endPt = prim.startPt;
    if (prim.type == PrimitiveType::ARC) {
        rev.isCCW = !prim.isCCW;
    }
    return rev;
}

AnalyticalPath reverseAnalyticalPath(const AnalyticalPath& path) {
    AnalyticalPath rev;
    rev.trace = path.trace;
    rev.reserve(path.size());
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        rev.push_back(reversePrimitive(*it));
    }
    return rev;
}

bool isPointInsideAnalyticalPath(const DxfPoint& pt, const AnalyticalPath& path) {
    auto pts = sampleAnalyticalPath(path, 0.2);
    if (pts.size() < 3) return false;

    bool inside = false;
    size_t n = pts.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        if (((pts[i].y > pt.y) != (pts[j].y > pt.y)) &&
            (pt.x < (pts[j].x - pts[i].x) * (pt.y - pts[i].y) / (pts[j].y - pts[i].y + 1e-12) + pts[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

double pointToAnalyticalPathDistance(const DxfPoint& pt, const AnalyticalPath& path) {
    auto pts = sampleAnalyticalPath(path, 0.2);
    if (pts.empty()) return 1e9;

    double minDistSq = 1e18;
    size_t n = pts.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        double dx = pts[j].x - pts[i].x;
        double dy = pts[j].y - pts[i].y;
        double lenSq = dx * dx + dy * dy;
        double t = (lenSq > 1e-12) ? (((pt.x - pts[i].x) * dx + (pt.y - pts[i].y) * dy) / lenSq) : 0.0;
        t = std::max(0.0, std::min(1.0, t));
        double projX = pts[i].x + t * dx;
        double projY = pts[i].y + t * dy;
        double distSq = (pt.x - projX) * (pt.x - projX) + (pt.y - projY) * (pt.y - projY);
        if (distSq < minDistSq) minDistSq = distSq;
    }
    return std::sqrt(minDistSq);
}

std::vector<AnalyticalPath> getShapeAnalyticalPaths(const DxfModel& model, const std::vector<size_t>& entityIndices) {
    struct ChainInternal {
        AnalyticalPath primitives;
        bool isClosed = false;
        std::vector<size_t> entityIndices;
    };

    std::vector<ChainInternal> rawChains;

    for (size_t idx : entityIndices) {
        if (idx >= model.entities.size()) continue;
        const auto& entity = model.entities[idx];
        const DxfLayer* layer = model.findLayer(entity.layerName);
        if (layer && !layer->visible) continue;

        ChainInternal chain;
        chain.entityIndices.push_back(idx);

        if (entity.type == DxfEntityType::LINE && entity.points.size() >= 2) {
            chain.primitives.push_back(PathPrimitive::CreateLine(entity.points[0], entity.points[1]));
            chain.isClosed = false;
        } else if (entity.type == DxfEntityType::ARC) {
            double sRad = entity.startAngle * M_PI / 180.0;
            double eRad = entity.endAngle * M_PI / 180.0;
            DxfPoint sPt(entity.center.x + entity.radius * std::cos(sRad),
                         entity.center.y + entity.radius * std::sin(sRad));
            DxfPoint ePt(entity.center.x + entity.radius * std::cos(eRad),
                         entity.center.y + entity.radius * std::sin(eRad));
            chain.primitives.push_back(PathPrimitive::CreateArc(sPt, ePt, entity.center, entity.radius, true));
            chain.isClosed = false;
        } else if (entity.type == DxfEntityType::CIRCLE) {
            DxfPoint p0(entity.center.x + entity.radius, entity.center.y);
            DxfPoint p180(entity.center.x - entity.radius, entity.center.y);
            chain.primitives.push_back(PathPrimitive::CreateArc(p0, p180, entity.center, entity.radius, true));
            chain.primitives.push_back(PathPrimitive::CreateArc(p180, p0, entity.center, entity.radius, true));
            chain.isClosed = true;
        } else if (entity.type == DxfEntityType::POLYLINE) {
            if (!entity.polylineVertices.empty()) {
                size_t count = entity.polylineVertices.size();
                size_t numSegs = entity.isClosed ? count : (count - 1);
                for (size_t i = 0; i < numSegs; ++i) {
                    const auto& v1 = entity.polylineVertices[i];
                    const auto& v2 = entity.polylineVertices[(i + 1) % count];
                    DxfPoint p1(v1.x, v1.y);
                    DxfPoint p2(v2.x, v2.y);
                    double b = v1.bulge;
                    if (std::abs(b) > 1e-6) {
                        double dx = p2.x - p1.x;
                        double dy = p2.y - p1.y;
                        double d = std::hypot(dx, dy);
                        if (d > 1e-6) {
                            double r = (d * (1.0 + b * b)) / (4.0 * std::abs(b));
                            double s = (d * (1.0 - b * b)) / (4.0 * b);
                            double mx = (p1.x + p2.x) / 2.0;
                            double my = (p1.y + p2.y) / 2.0;
                            double cx = mx - s * (dy / d);
                            double cy = my + s * (dx / d);
                            bool ccw = (b > 0);
                            chain.primitives.push_back(PathPrimitive::CreateArc(p1, p2, DxfPoint(cx, cy), r, ccw));
                        }
                    } else {
                        chain.primitives.push_back(PathPrimitive::CreateLine(p1, p2));
                    }
                }
                chain.isClosed = entity.isClosed;
            } else if (entity.points.size() >= 2) {
                for (size_t i = 0; i < entity.points.size() - 1; ++i) {
                    chain.primitives.push_back(PathPrimitive::CreateLine(entity.points[i], entity.points[i + 1]));
                }
                if (entity.isClosed) {
                    chain.primitives.push_back(PathPrimitive::CreateLine(entity.points.back(), entity.points.front()));
                    chain.isClosed = true;
                }
            }
        }

        if (chain.primitives.empty()) continue;

        if (!chain.isClosed && chain.primitives.size() > 0) {
            double d = pointDist(chain.primitives.front().startPt, chain.primitives.back().endPt);
            if (d < 1e-3) {
                chain.isClosed = true;
            }
        }

        rawChains.push_back(chain);
    }

    std::vector<ChainInternal> openChains;
    std::vector<AnalyticalPath> closedResult;

    for (auto& c : rawChains) {
        if (c.isClosed) {
            ensureLoopOrientation(c.primitives, true); // Normalize to CCW
            AnalyticalPath loop = c.primitives;
            loop.trace.entityHash = computeEntityHash(c.entityIndices);
            loop.trace.offsetDistance = 0.0;
            loop.trace.role = PathRole::RAW_SHAPE;
            loop.trace.entitySignature = computeEntitySignature(loop.trace.entityHash, 0.0);
            loop.trace.geometrySignature = computeGeometrySignature(loop);
            closedResult.push_back(loop);
        } else {
            openChains.push_back(c);
        }
    }

    while (!openChains.empty()) {
        ChainInternal current = openChains.back();
        openChains.pop_back();

        bool joined = true;
        while (joined && !current.isClosed) {
            joined = false;
            for (size_t i = 0; i < openChains.size(); ++i) {
                auto& candidate = openChains[i];
                double tol = 1e-3;

                double d1 = pointDist(current.primitives.back().endPt, candidate.primitives.front().startPt);
                double d2 = pointDist(current.primitives.back().endPt, candidate.primitives.back().endPt);
                double d3 = pointDist(current.primitives.front().startPt, candidate.primitives.back().endPt);
                double d4 = pointDist(current.primitives.front().startPt, candidate.primitives.front().startPt);

                if (d1 < tol) {
                    current.primitives.insert(current.primitives.end(), candidate.primitives.begin(), candidate.primitives.end());
                    current.entityIndices.insert(current.entityIndices.end(), candidate.entityIndices.begin(), candidate.entityIndices.end());
                    openChains.erase(openChains.begin() + i);
                    joined = true;
                    break;
                } else if (d2 < tol) {
                    auto rev = reverseAnalyticalPath(candidate.primitives);
                    current.primitives.insert(current.primitives.end(), rev.begin(), rev.end());
                    current.entityIndices.insert(current.entityIndices.end(), candidate.entityIndices.begin(), candidate.entityIndices.end());
                    openChains.erase(openChains.begin() + i);
                    joined = true;
                    break;
                } else if (d3 < tol) {
                    current.primitives.insert(current.primitives.begin(), candidate.primitives.begin(), candidate.primitives.end());
                    current.entityIndices.insert(current.entityIndices.end(), candidate.entityIndices.begin(), candidate.entityIndices.end());
                    openChains.erase(openChains.begin() + i);
                    joined = true;
                    break;
                } else if (d4 < tol) {
                    auto rev = reverseAnalyticalPath(candidate.primitives);
                    current.primitives.insert(current.primitives.begin(), rev.begin(), rev.end());
                    current.entityIndices.insert(current.entityIndices.end(), candidate.entityIndices.begin(), candidate.entityIndices.end());
                    openChains.erase(openChains.begin() + i);
                    joined = true;
                    break;
                }
            }

            if (!current.primitives.empty()) {
                double closeDist = pointDist(current.primitives.front().startPt, current.primitives.back().endPt);
                if (closeDist < 1e-3) {
                    current.isClosed = true;
                    current.primitives.back().endPt = current.primitives.front().startPt;
                }
            }
        }

        if (current.isClosed) {
            ensureLoopOrientation(current.primitives, true); // Normalize to CCW
            AnalyticalPath loop = current.primitives;
            loop.trace.entityHash = computeEntityHash(current.entityIndices);
            loop.trace.offsetDistance = 0.0;
            loop.trace.role = PathRole::RAW_SHAPE;
            loop.trace.entitySignature = computeEntitySignature(loop.trace.entityHash, 0.0);
            loop.trace.geometrySignature = computeGeometrySignature(loop);
            closedResult.push_back(loop);
        }
    }

    return closedResult;
}

PathPrimitive offsetSinglePrimitive(const PathPrimitive& prim, double dist) {
    PathPrimitive res = prim;
    if (prim.type == PrimitiveType::LINE) {
        double dx = prim.endPt.x - prim.startPt.x;
        double dy = prim.endPt.y - prim.startPt.y;
        double len = std::hypot(dx, dy);
        if (len < 1e-6) {
            res.radius = -1.0; // Mark invalid / collapsed
            return res;
        }

        // Left normal vector
        DxfPoint N(-dy / len, dx / len);

        res.startPt.x = prim.startPt.x + dist * N.x;
        res.startPt.y = prim.startPt.y + dist * N.y;
        res.endPt.x = prim.endPt.x + dist * N.x;
        res.endPt.y = prim.endPt.y + dist * N.y;
    } else {
        if (prim.radius <= 1e-6) {
            res.radius = -1.0;
            return res;
        }
        double deltaR = prim.isCCW ? -dist : dist;
        res.radius = prim.radius + deltaR;
        if (res.radius <= 1e-4) {
            // Arc offset radius collapsed to zero or negative! Mark invalid.
            res.radius = -1.0;
            return res;
        }

        double scale = res.radius / prim.radius;
        res.startPt.x = prim.center.x + (prim.startPt.x - prim.center.x) * scale;
        res.startPt.y = prim.center.y + (prim.startPt.y - prim.center.y) * scale;
        res.endPt.x = prim.center.x + (prim.endPt.x - prim.center.x) * scale;
        res.endPt.y = prim.center.y + (prim.endPt.y - prim.center.y) * scale;
    }
    return res;
}

bool checkCornerConvexity(const PathPrimitive& prim1, const PathPrimitive& prim2, double offsetDist) {
    DxfPoint t1 = getPrimitiveEndTangent(prim1);
    DxfPoint t2 = getPrimitiveStartTangent(prim2);

    double crossZ = t1.x * t2.y - t1.y * t2.x;
    double dot = t1.x * t2.x + t1.y * t2.y;
    double angleTurn = std::atan2(crossZ, dot);

    if (std::abs(angleTurn) < 1e-3) {
        return false;
    }

    // Roll-around arc needed if turn direction and offset direction have opposite signs
    return (angleTurn * offsetDist < -1e-3);
}

static bool isDegeneratePrimitive(const PathPrimitive& prim) {
    double len = getPrimitiveLength(prim);
    if (len < 0.01) return true;

    if (prim.type == PrimitiveType::ARC) {
        if (prim.radius <= 1e-4) return true;
        double a1 = std::atan2(prim.startPt.y - prim.center.y, prim.startPt.x - prim.center.x);
        double a2 = std::atan2(prim.endPt.y - prim.center.y, prim.endPt.x - prim.center.x);
        double da = a2 - a1;
        if (prim.isCCW) {
            while (da < 0.0) da += 2.0 * M_PI;
        } else {
            while (da > 0.0) da -= 2.0 * M_PI;
            da = std::abs(da);
        }
        double sweepDeg = da * 180.0 / M_PI;
        if (sweepDeg < 0.1) return true;
    }
    return false;
}

AnalyticalPath cleanDegeneratePrimitives(const AnalyticalPath& path) {
    if (path.empty()) return path;
    AnalyticalPath cleaned;
    cleaned.trace = path.trace;
    cleaned.reserve(path.size());

    for (const auto& prim : path) {
        if (prim.isSpur) {
            cleaned.push_back(prim);
            if (cleaned.size() > 1) {
                cleaned.back().startPt = cleaned[cleaned.size() - 2].endPt;
            }
        } else if (isDegeneratePrimitive(prim)) {
            if (!cleaned.empty()) {
                cleaned.back().endPt = prim.endPt;
                if (prim.hasEndSpur) {
                    cleaned.back().hasEndSpur = true;
                    cleaned.back().endSpurApex = prim.endSpurApex;
                }
            }
        } else {
            cleaned.push_back(prim);
            if (cleaned.size() > 1) {
                cleaned.back().startPt = cleaned[cleaned.size() - 2].endPt;
            }
        }
    }

    if (cleaned.size() >= 2) {
        double d = pointDist(cleaned.back().endPt, cleaned.front().startPt);
        if (d < 1e-1) {
            cleaned.front().startPt = cleaned.back().endPt;
        }
    }

    return cleaned;
}

PathPrimitive createRollAroundArc(const PathPrimitive& prim1, const PathPrimitive& prim2, const PathPrimitive& offsetPrim1, double dist) {
    DxfPoint cornerPt = prim1.endPt;
    DxfPoint sPt = offsetPrim1.endPt;
    PathPrimitive offsetPrim2 = offsetSinglePrimitive(prim2, dist);
    DxfPoint ePt = offsetPrim2.startPt;

    bool ccw = (dist > 0.0) ? false : true;
    return PathPrimitive::CreateArc(sPt, ePt, cornerPt, std::abs(dist), ccw);
}

static bool isPointOnLineSegment(const DxfPoint& pt, const DxfPoint& s, const DxfPoint& e, double tol = 1e-2) {
    double dx = e.x - s.x;
    double dy = e.y - s.y;
    double lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-12) return (pointDist(pt, s) < tol);

    double t = ((pt.x - s.x) * dx + (pt.y - s.y) * dy) / lenSq;
    if (t < -tol / std::sqrt(lenSq) || t > 1.0 + tol / std::sqrt(lenSq)) return false;

    double projX = s.x + t * dx;
    double projY = s.y + t * dy;
    return (std::hypot(pt.x - projX, pt.y - projY) < tol);
}

static bool isPointOnArcSweep(const DxfPoint& pt, const DxfPoint& center, double radius, const DxfPoint& sPt, const DxfPoint& ePt, bool isCCW, double tol = 1e-2) {
    if (std::abs(pointDist(pt, center) - radius) > tol) return false;

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

bool findPrimitiveIntersection(const PathPrimitive& p1, const PathPrimitive& p2, DxfPoint& outIntersect) {
    DxfPoint cornerPt((p1.endPt.x + p2.startPt.x) / 2.0, (p1.endPt.y + p2.startPt.y) / 2.0);

    if (p1.type == PrimitiveType::LINE && p2.type == PrimitiveType::LINE) {
        double dx1 = p1.endPt.x - p1.startPt.x;
        double dy1 = p1.endPt.y - p1.startPt.y;
        double dx2 = p2.endPt.x - p2.startPt.x;
        double dy2 = p2.endPt.y - p2.startPt.y;

        double det = dx1 * dy2 - dy1 * dx2;
        if (std::abs(det) < 1e-9) return false;

        double t1 = ((p2.startPt.x - p1.startPt.x) * dy2 - (p2.startPt.y - p1.startPt.y) * dx2) / det;
        outIntersect = DxfPoint(p1.startPt.x + t1 * dx1, p1.startPt.y + t1 * dy1);
        return true;
    } else if (p1.type == PrimitiveType::LINE && p2.type == PrimitiveType::ARC) {
        if (p2.radius <= 1e-6) return false;
        double dx = p1.endPt.x - p1.startPt.x;
        double dy = p1.endPt.y - p1.startPt.y;
        double len = std::hypot(dx, dy);
        if (len < 1e-9) return false;
        double ux = dx / len;
        double uy = dy / len;

        double delx = p1.startPt.x - p2.center.x;
        double dely = p1.startPt.y - p2.center.y;

        double B = 2.0 * (delx * ux + dely * uy);
        double C = (delx * delx + dely * dely) - p2.radius * p2.radius;
        double disc = B * B - 4.0 * C;
        if (disc < 0.0) return false;

        double sqD = std::sqrt(disc);
        double tA = (-B - sqD) / 2.0;
        double tB = (-B + sqD) / 2.0;

        DxfPoint ptA(p1.startPt.x + tA * ux, p1.startPt.y + tA * uy);
        DxfPoint ptB(p1.startPt.x + tB * ux, p1.startPt.y + tB * uy);

        double dA = pointDist(ptA, cornerPt);
        double dB = pointDist(ptB, cornerPt);
        outIntersect = (dA < dB) ? ptA : ptB;
        return true;
    } else if (p1.type == PrimitiveType::ARC && p2.type == PrimitiveType::LINE) {
        if (p1.radius <= 1e-6) return false;
        double dx = p2.endPt.x - p2.startPt.x;
        double dy = p2.endPt.y - p2.startPt.y;
        double len = std::hypot(dx, dy);
        if (len < 1e-9) return false;
        double ux = dx / len;
        double uy = dy / len;

        double delx = p2.startPt.x - p1.center.x;
        double dely = p2.startPt.y - p1.center.y;

        double B = 2.0 * (delx * ux + dely * uy);
        double C = (delx * delx + dely * dely) - p1.radius * p1.radius;
        double disc = B * B - 4.0 * C;
        if (disc < 0.0) return false;

        double sqD = std::sqrt(disc);
        double tA = (-B - sqD) / 2.0;
        double tB = (-B + sqD) / 2.0;

        DxfPoint ptA(p2.startPt.x + tA * ux, p2.startPt.y + tA * uy);
        DxfPoint ptB(p2.startPt.x + tB * ux, p2.startPt.y + tB * uy);

        double dA = pointDist(ptA, cornerPt);
        double dB = pointDist(ptB, cornerPt);
        outIntersect = (dA < dB) ? ptA : ptB;
        return true;
    } else { // ARC - ARC
        if (p1.radius <= 1e-6 || p2.radius <= 1e-6) return false;
        double d = pointDist(p1.center, p2.center);
        if (d < 1e-6) {
            outIntersect = DxfPoint((p1.endPt.x + p2.startPt.x) / 2.0, (p1.endPt.y + p2.startPt.y) / 2.0);
            return true;
        }
        if (d > p1.radius + p2.radius || d < std::abs(p1.radius - p2.radius)) {
            return false;
        }

        double a = (p1.radius * p1.radius - p2.radius * p2.radius + d * d) / (2.0 * d);
        double h2 = p1.radius * p1.radius - a * a;
        double h = (h2 > 0.0) ? std::sqrt(h2) : 0.0;

        double p0x = p1.center.x + a * (p2.center.x - p1.center.x) / d;
        double p0y = p1.center.y + a * (p2.center.y - p1.center.y) / d;

        double nx = -(p2.center.y - p1.center.y) / d;
        double ny = (p2.center.x - p1.center.x) / d;

        DxfPoint cand1(p0x + h * nx, p0y + h * ny);
        DxfPoint cand2(p0x - h * nx, p0y - h * ny);

        double d1 = pointDist(cand1, cornerPt);
        double d2 = pointDist(cand2, cornerPt);
        outIntersect = (d1 < d2) ? cand1 : cand2;
        return true;
    }
}

bool findPrimitiveIntersectionBounded(const PathPrimitive& p1, const PathPrimitive& p2, DxfPoint& outIntersect) {
    if (p1.type == PrimitiveType::LINE && p2.type == PrimitiveType::LINE) {
        DxfPoint inter;
        if (findPrimitiveIntersection(p1, p2, inter)) {
            if (isPointOnLineSegment(inter, p1.startPt, p1.endPt) &&
                isPointOnLineSegment(inter, p2.startPt, p2.endPt)) {
                outIntersect = inter;
                return true;
            }
        }
        return false;
    } else if (p1.type == PrimitiveType::LINE && p2.type == PrimitiveType::ARC) {
        if (p2.radius <= 1e-6) return false;
        double dx = p1.endPt.x - p1.startPt.x;
        double dy = p1.endPt.y - p1.startPt.y;
        double len = std::hypot(dx, dy);
        if (len < 1e-9) return false;
        double ux = dx / len;
        double uy = dy / len;

        double delx = p1.startPt.x - p2.center.x;
        double dely = p1.startPt.y - p2.center.y;

        double B = 2.0 * (delx * ux + dely * uy);
        double C = (delx * delx + dely * dely) - p2.radius * p2.radius;
        double disc = B * B - 4.0 * C;
        if (disc < 0.0) return false;

        double sqD = std::sqrt(disc);
        double tA = (-B - sqD) / 2.0;
        double tB = (-B + sqD) / 2.0;

        DxfPoint ptA(p1.startPt.x + tA * ux, p1.startPt.y + tA * uy);
        DxfPoint ptB(p1.startPt.x + tB * ux, p1.startPt.y + tB * uy);

        bool validA = isPointOnLineSegment(ptA, p1.startPt, p1.endPt) &&
                      isPointOnArcSweep(ptA, p2.center, p2.radius, p2.startPt, p2.endPt, p2.isCCW);
        bool validB = isPointOnLineSegment(ptB, p1.startPt, p1.endPt) &&
                      isPointOnArcSweep(ptB, p2.center, p2.radius, p2.startPt, p2.endPt, p2.isCCW);

        if (validA && validB) {
            double dA = pointDist(ptA, p1.endPt);
            double dB = pointDist(ptB, p1.endPt);
            outIntersect = (dA < dB) ? ptA : ptB;
            return true;
        } else if (validA) {
            outIntersect = ptA;
            return true;
        } else if (validB) {
            outIntersect = ptB;
            return true;
        }
        return false;
    } else if (p1.type == PrimitiveType::ARC && p2.type == PrimitiveType::LINE) {
        return findPrimitiveIntersectionBounded(p2, p1, outIntersect);
    } else { // ARC - ARC
        if (p1.radius <= 1e-6 || p2.radius <= 1e-6) return false;
        double d = pointDist(p1.center, p2.center);
        if (d < 1e-6) {
            DxfPoint cand = DxfPoint((p1.endPt.x + p2.startPt.x) / 2.0, (p1.endPt.y + p2.startPt.y) / 2.0);
            if (isPointOnArcSweep(cand, p1.center, p1.radius, p1.startPt, p1.endPt, p1.isCCW) &&
                isPointOnArcSweep(cand, p2.center, p2.radius, p2.startPt, p2.endPt, p2.isCCW)) {
                outIntersect = cand;
                return true;
            }
            return false;
        }
        if (d > p1.radius + p2.radius || d < std::abs(p1.radius - p2.radius)) {
            return false;
        }

        double a = (p1.radius * p1.radius - p2.radius * p2.radius + d * d) / (2.0 * d);
        double h2 = p1.radius * p1.radius - a * a;
        double h = (h2 > 0.0) ? std::sqrt(h2) : 0.0;

        double p0x = p1.center.x + a * (p2.center.x - p1.center.x) / d;
        double p0y = p1.center.y + a * (p2.center.y - p1.center.y) / d;

        double nx = -(p2.center.y - p1.center.y) / d;
        double ny = (p2.center.x - p1.center.x) / d;

        DxfPoint cand1(p0x + h * nx, p0y + h * ny);
        DxfPoint cand2(p0x - h * nx, p0y - h * ny);

        bool valid1 = isPointOnArcSweep(cand1, p1.center, p1.radius, p1.startPt, p1.endPt, p1.isCCW) &&
                      isPointOnArcSweep(cand1, p2.center, p2.radius, p2.startPt, p2.endPt, p2.isCCW);
        bool valid2 = isPointOnArcSweep(cand2, p1.center, p1.radius, p1.startPt, p1.endPt, p1.isCCW) &&
                      isPointOnArcSweep(cand2, p2.center, p2.radius, p2.startPt, p2.endPt, p2.isCCW);

        if (valid1 && valid2) {
            double d1 = pointDist(cand1, p1.endPt);
            double d2 = pointDist(cand2, p1.endPt);
            outIntersect = (d1 < d2) ? cand1 : cand2;
            return true;
        } else if (valid1) {
            outIntersect = cand1;
            return true;
        } else if (valid2) {
            outIntersect = cand2;
            return true;
        }
        return false;
    }
}

static bool isIntersectionWithinBounds(const PathPrimitive& p1, const PathPrimitive& p2, const DxfPoint& pt, double offsetDist) {
    double absDist = std::abs(offsetDist);
    double maxDistMargin = std::max(5.0 * absDist, 10.0);

    if (pointDist(pt, p1.endPt) > maxDistMargin || pointDist(pt, p2.startPt) > maxDistMargin) {
        return false;
    }

    double t1 = 0.0;
    if (p1.type == PrimitiveType::LINE) {
        double dx = p1.endPt.x - p1.startPt.x;
        double dy = p1.endPt.y - p1.startPt.y;
        double lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-12) return false;
        t1 = ((pt.x - p1.startPt.x) * dx + (pt.y - p1.startPt.y) * dy) / lenSq;
    } else {
        double a1 = std::atan2(p1.startPt.y - p1.center.y, p1.startPt.x - p1.center.x);
        double a2 = std::atan2(p1.endPt.y - p1.center.y, p1.endPt.x - p1.center.x);
        double a = std::atan2(pt.y - p1.center.y, pt.x - p1.center.x);
        double da = a - a1;
        double sweep = a2 - a1;
        if (p1.isCCW) {
            while (da < 0.0) da += 2.0 * M_PI;
            while (sweep <= 0.0) sweep += 2.0 * M_PI;
            t1 = (sweep > 1e-9) ? da / sweep : 1.0;
        } else {
            while (da > 0.0) da -= 2.0 * M_PI;
            while (sweep >= 0.0) sweep -= 2.0 * M_PI;
            t1 = (std::abs(sweep) > 1e-9) ? da / sweep : 1.0;
        }
    }

    double t2 = 0.0;
    if (p2.type == PrimitiveType::LINE) {
        double dx = p2.endPt.x - p2.startPt.x;
        double dy = p2.endPt.y - p2.startPt.y;
        double lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-12) return false;
        t2 = ((pt.x - p2.startPt.x) * dx + (pt.y - p2.startPt.y) * dy) / lenSq;
    } else {
        double a1 = std::atan2(p2.startPt.y - p2.center.y, p2.startPt.x - p2.center.x);
        double a2 = std::atan2(p2.endPt.y - p2.center.y, p2.endPt.x - p2.center.x);
        double a = std::atan2(pt.y - p2.center.y, pt.x - p2.center.x);
        double da = a - a1;
        double sweep = a2 - a1;
        if (p2.isCCW) {
            while (da < 0.0) da += 2.0 * M_PI;
            while (sweep <= 0.0) sweep += 2.0 * M_PI;
            t2 = (sweep > 1e-9) ? da / sweep : 0.0;
        } else {
            while (da > 0.0) da -= 2.0 * M_PI;
            while (sweep >= 0.0) sweep -= 2.0 * M_PI;
            t2 = (std::abs(sweep) > 1e-9) ? da / sweep : 0.0;
        }
    }

    if (t1 > 1.0 + 1e-3 && t2 < -1e-3) {
        return false;
    }

    if (t1 < -0.5 || t1 > 2.0 || t2 < -1.0 || t2 > 1.5) {
        return false;
    }

    return true;
}

AnalyticalPath offsetAnalyticalPathRaw(const AnalyticalPath& inPath, double dist) {
    AnalyticalPath rawOffset;
    rawOffset.trace = inPath.trace;
    rawOffset.trace.offsetDistance = inPath.trace.offsetDistance + dist;
    rawOffset.trace.entitySignature = computeEntitySignature(rawOffset.trace.entityHash, rawOffset.trace.offsetDistance);
    if (inPath.size() < 2) return rawOffset;

    AnalyticalPath path = inPath;
    ensureLoopOrientation(path, true); // Enforce CCW outer orientation

    AnalyticalPath validOffsetPrims;
    std::vector<size_t> origIndices;

    size_t n = path.size();
    for (size_t i = 0; i < n; ++i) {
        PathPrimitive offsetPrim = offsetSinglePrimitive(path[i], dist);
        if (offsetPrim.radius >= 0.0 && getPrimitiveLength(offsetPrim) > 1e-4) {
            validOffsetPrims.push_back(offsetPrim);
            origIndices.push_back(i);
        }
    }

    if (validOffsetPrims.size() < 2) return rawOffset;

    size_t m = validOffsetPrims.size();
    std::vector<bool> isRollAround(m, false);
    std::vector<PathPrimitive> rollAroundArcs(m);

    // Pass 1: Resolve all corner points (intersections first, then roll-around arcs if genuinely convex)
    for (size_t i = 0; i < m; ++i) {
        size_t nextIdx = (i + 1) % m;
        size_t origI = origIndices[i];
        size_t origNextI = origIndices[nextIdx];

        PathPrimitive prim = path[origI];
        PathPrimitive nextPrim = path[origNextI];

        DxfPoint intersectPt;
        bool foundIntersect = false;

        for (size_t fwd = 1; fwd < m - 1; ++fwd) {
            size_t candIdx = (i + fwd) % m;
            DxfPoint inter;
            if (findPrimitiveIntersection(validOffsetPrims[i], validOffsetPrims[candIdx], inter)) {
                if (isIntersectionWithinBounds(validOffsetPrims[i], validOffsetPrims[candIdx], inter, dist)) {
                    validOffsetPrims[i].endPt = inter;
                    validOffsetPrims[candIdx].startPt = inter;
                    foundIntersect = true;
                    break;
                }
            }
        }

        if (foundIntersect) {
            isRollAround[i] = false;
        } else {
            bool rollAround = checkCornerConvexity(prim, nextPrim, dist);
            if (rollAround) {
                isRollAround[i] = true;
                rollAroundArcs[i] = createRollAroundArc(prim, nextPrim, validOffsetPrims[i], dist);
            } else {
                isRollAround[i] = false;
                if (findPrimitiveIntersection(validOffsetPrims[i], validOffsetPrims[nextIdx], intersectPt)) {
                    validOffsetPrims[i].endPt = intersectPt;
                    validOffsetPrims[nextIdx].startPt = intersectPt;
                }
            }
        }
    }

    // Pass 2: Assemble raw offset loop with updated endpoints and roll-around arcs
    for (size_t i = 0; i < m; ++i) {
        rawOffset.push_back(validOffsetPrims[i]);
        if (isRollAround[i] && !isDegeneratePrimitive(rollAroundArcs[i])) {
            rawOffset.push_back(rollAroundArcs[i]);
        }
    }

    rawOffset = cleanDegeneratePrimitives(rawOffset);

    // Connect endpoints across consecutive primitives cleanly
    if (rawOffset.size() >= 3) {
        for (size_t i = 0; i < rawOffset.size(); ++i) {
            size_t nextIdx = (i + 1) % rawOffset.size();
            double d = pointDist(rawOffset[i].endPt, rawOffset[nextIdx].startPt);
            if (d < 1e-1 && d > 1e-6) {
                rawOffset[nextIdx].startPt = rawOffset[i].endPt;
            }
        }
    }

    rawOffset.trace.geometrySignature = computeGeometrySignature(rawOffset);
    return rawOffset;
}

std::vector<AnalyticalPath> offsetAnalyticalPath(const AnalyticalPath& inPath, double dist) {
    AnalyticalPath rawOffset = offsetAnalyticalPathRaw(inPath, dist);
    if (rawOffset.size() < 2) return std::vector<AnalyticalPath>();
    bool isOutward = (dist < 0.0);
    auto results = pruneSelfIntersectionsAndSplit(rawOffset, &inPath, std::abs(dist), isOutward);
    for (auto& r : results) {
        r.trace = rawOffset.trace;
        r.trace.geometrySignature = computeGeometrySignature(r);
    }
    return results;
}

std::vector<AnalyticalPath> pruneSelfIntersectionsAndSplit(const AnalyticalPath& path, const AnalyticalPath* parentBoundary, double distFromParent, bool isOutward) {
    std::vector<AnalyticalPath> resultLoops;
    AnalyticalPath cleanPath = cleanDegeneratePrimitives(path);
    if (cleanPath.size() < 2) return resultLoops;

    size_t n = cleanPath.size();
    bool foundSplit = false;
    size_t splitI = 0, splitJ = 0;
    DxfPoint splitPt;

    for (size_t i = 0; i < n && !foundSplit; ++i) {
        for (size_t j = i + 2; j < n && !foundSplit; ++j) {
            if (i == 0 && j == n - 1) continue;

            DxfPoint inter;
            if (findPrimitiveIntersectionBounded(cleanPath[i], cleanPath[j], inter)) {
                foundSplit = true;
                splitI = i;
                splitJ = j;
                splitPt = inter;
            }
        }
    }

    if (!foundSplit) {
        double area = getAnalyticalPathArea(cleanPath);
        // Valid interior loop must have strictly positive CCW area
        if (area > 1e-3) {
            // Verify all control points lie inside parent boundary (if provided and inward offset)
            bool validLocation = true;
            if (!isOutward && parentBoundary && !parentBoundary->empty()) {
                auto samplePts = sampleAnalyticalPath(cleanPath, 0.5);
                int insideCount = 0;
                for (const auto& pt : samplePts) {
                    if (isPointInsideAnalyticalPath(pt, *parentBoundary)) {
                        insideCount++;
                    }
                }
                if (samplePts.size() > 0 && (double)insideCount / samplePts.size() < 0.75) {
                    validLocation = false;
                }
            }
            if (validLocation) {
                cleanPath.trace.geometrySignature = computeGeometrySignature(cleanPath);
                resultLoops.push_back(cleanPath);
            }
        }
        return resultLoops;
    }

    // Split into 2 sub-loops
    AnalyticalPath loop1, loop2;
    loop1.trace = cleanPath.trace;
    loop2.trace = cleanPath.trace;

    for (size_t k = 0; k <= splitI; ++k) {
        loop1.push_back(cleanPath[k]);
    }
    loop1.back().endPt = splitPt;

    for (size_t k = splitJ + 1; k < n; ++k) {
        loop1.push_back(cleanPath[k]);
    }
    if (loop1.size() > splitI + 1) {
        loop1[splitI + 1].startPt = splitPt;
    }

    for (size_t k = splitI + 1; k <= splitJ; ++k) {
        loop2.push_back(cleanPath[k]);
    }
    if (!loop2.empty()) {
        loop2.front().startPt = splitPt;
        loop2.back().endPt = splitPt;
    }

    auto subClean1 = cleanDegeneratePrimitives(loop1);
    auto subClean2 = cleanDegeneratePrimitives(loop2);

    auto res1 = pruneSelfIntersectionsAndSplit(subClean1, parentBoundary, distFromParent, isOutward);
    auto res2 = pruneSelfIntersectionsAndSplit(subClean2, parentBoundary, distFromParent, isOutward);

    resultLoops.insert(resultLoops.end(), res1.begin(), res1.end());
    resultLoops.insert(resultLoops.end(), res2.begin(), res2.end());

    return resultLoops;
}

std::vector<AnalyticalPath> generatePocketRings(const AnalyticalPath& outerBoundary, double toolRad, double stepoverDist) {
    std::vector<AnalyticalPath> allRings;
    std::vector<AnalyticalPath> activeLoops = offsetAnalyticalPath(outerBoundary, +toolRad);

    int maxPasses = 200;
    int pass = 0;

    while (!activeLoops.empty() && pass < maxPasses) {
        pass++;
        std::vector<AnalyticalPath> nextActiveLoops;

        for (auto& loop : activeLoops) {
            loop.trace.role = PathRole::POCKET_RING;
            loop.trace.geometrySignature = computeGeometrySignature(loop);
            allRings.push_back(loop);

            std::vector<AnalyticalPath> offsetRings = offsetAnalyticalPath(loop, +stepoverDist);

            for (const auto& offsetRing : offsetRings) {
                if (getAnalyticalPathArea(offsetRing) > 1e-3 && offsetRing.size() >= 3) {
                    nextActiveLoops.push_back(offsetRing);
                }
            }
        }

        activeLoops = nextActiveLoops;
    }

    return allRings;
}

void emitPrimitiveGCode(std::ofstream& out, const PathPrimitive& prim, double z, double feed) {
    char buf[256];
    if (prim.type == PrimitiveType::LINE) {
        if (pointDist(prim.startPt, prim.endPt) < 1e-6) return; // Skip zero-length line
        snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n",
                 prim.endPt.x, prim.endPt.y, z, feed);
        out << buf;
    } else if (prim.type == PrimitiveType::ARC) {
        if (prim.radius <= 1e-6) return; // Skip zero-radius arc

        double chord = pointDist(prim.startPt, prim.endPt);
        double a1 = std::atan2(prim.startPt.y - prim.center.y, prim.startPt.x - prim.center.x);
        double a2 = std::atan2(prim.endPt.y - prim.center.y, prim.endPt.x - prim.center.x);
        double da = a2 - a1;
        if (prim.isCCW) {
            while (da <= 0.0) da += 2.0 * M_PI;
        } else {
            while (da >= 0.0) da -= 2.0 * M_PI;
        }

        // Micro-arc protection: If chord is tiny and sweep angle is near 0, do not emit G2/G3!
        if (chord < 1e-4 && std::abs(da) < 1e-3) {
            snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n",
                     prim.endPt.x, prim.endPt.y, z, feed);
            out << buf;
            return;
        }

        double I = prim.center.x - prim.startPt.x;
        double J = prim.center.y - prim.startPt.y;
        const char* cmd = prim.isCCW ? "G3" : "G2";
        snprintf(buf, sizeof(buf), "%s X%.3f Y%.3f Z%.3f I%.3f J%.3f F%.3f\n",
                 cmd, prim.endPt.x, prim.endPt.y, z, I, J, feed);
        out << buf;
    }
}

void emitAnalyticalLoopGCode(std::ofstream& out, const AnalyticalPath& path,
                            double prevZ, double curZ, double feedZ, double feedXY, double retractZ,
                            double rampAngleDeg, bool isRamping, DxfPoint& currentToolPos, bool& isRetracted)
{
    if (path.empty()) return;
    char buf[256];
    DxfPoint startPt = path[0].startPt;

    if (isRetracted) {
        snprintf(buf, sizeof(buf), "G0 Z%.3f\n", retractZ); out << buf;
        snprintf(buf, sizeof(buf), "G0 X%.3f Y%.3f\n", startPt.x, startPt.y); out << buf;
        isRetracted = false;
        double initialZ = isRamping ? prevZ : curZ;
        snprintf(buf, sizeof(buf), "G1 Z%.3f F%.3f\n", initialZ, feedZ); out << buf;
        currentToolPos = startPt;
    } else {
        double distToStart = pointDist(startPt, currentToolPos);
        if (distToStart > 1e-4) {
            snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f F%.3f\n", startPt.x, startPt.y, feedXY); out << buf;
            currentToolPos = startPt;
        }
    }

    double totLen = getPathLength(path);

    if (isRamping && std::abs(prevZ - curZ) > 1e-4 && totLen > 1e-6) {
        double cumLen = 0.0;
        for (const auto& prim : path) {
            cumLen += getPrimitiveLength(prim);
            double t = cumLen / totLen;
            double pz = prevZ + (curZ - prevZ) * t;
            emitPrimitiveGCode(out, prim, pz, feedZ);
        }

        snprintf(buf, sizeof(buf), "G1 F%.3f\n", feedXY); out << buf;
        for (const auto& prim : path) {
            emitPrimitiveGCode(out, prim, curZ, feedXY);
            if (prim.hasEndSpur) {
                snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n",
                         prim.endSpurApex.x, prim.endSpurApex.y, curZ, feedXY);
                out << buf;
                snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n",
                         prim.endPt.x, prim.endPt.y, curZ, feedXY);
                out << buf;
            }
        }
    } else {
        snprintf(buf, sizeof(buf), "G1 F%.3f\n", feedXY); out << buf;
        for (const auto& prim : path) {
            emitPrimitiveGCode(out, prim, curZ, feedXY);
            if (prim.hasEndSpur) {
                snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n",
                         prim.endSpurApex.x, prim.endSpurApex.y, curZ, feedXY);
                out << buf;
                snprintf(buf, sizeof(buf), "G1 X%.3f Y%.3f Z%.3f F%.3f\n",
                         prim.endPt.x, prim.endPt.y, curZ, feedXY);
                out << buf;
            }
        }
    }

    currentToolPos = path.back().endPt;
}

HierarchyValidationResult validatePartHierarchy(
    const DxfModel& model,
    const std::vector<CamShape>& shapes,
    double topHeight)
{
    HierarchyValidationResult result;
    if (shapes.size() < 2) return result;

    for (size_t i = 0; i < shapes.size(); ++i) {
        for (size_t j = i + 1; j < shapes.size(); ++j) {
            const auto& s1 = shapes[i];
            const auto& s2 = shapes[j];
            if (std::abs(s1.depth - s2.depth) < 1e-4) continue;

            const CamShape& taller = (s1.depth > s2.depth) ? s1 : s2;
            const CamShape& shorter = (s1.depth > s2.depth) ? s2 : s1;

            std::vector<size_t> tallerIndices(taller.entityIndices.begin(), taller.entityIndices.end());
            std::vector<size_t> shorterIndices(shorter.entityIndices.begin(), shorter.entityIndices.end());

            auto tallerPaths = getShapeAnalyticalPaths(model, tallerIndices);
            auto shorterPaths = getShapeAnalyticalPaths(model, shorterIndices);

            if (tallerPaths.empty() || shorterPaths.empty()) continue;

            const auto& tallerBoundary = tallerPaths[0];
            const auto& shorterBoundary = shorterPaths[0];

            auto samplePts = sampleAnalyticalPath(tallerBoundary, 0.5);
            if (samplePts.empty()) continue;

            int insideCount = 0;
            for (const auto& pt : samplePts) {
                if (isPointInsideAnalyticalPath(pt, shorterBoundary)) {
                    insideCount++;
                }
            }

            double ratio = (double)insideCount / samplePts.size();

            auto shorterPts = sampleAnalyticalPath(shorterBoundary, 0.5);
            int shorterInTallerCount = 0;
            for (const auto& pt : shorterPts) {
                if (isPointInsideAnalyticalPath(pt, tallerBoundary)) {
                    shorterInTallerCount++;
                }
            }
            double shorterRatio = shorterPts.empty() ? 0.0 : (double)shorterInTallerCount / shorterPts.size();

            if (ratio < 0.95 && shorterRatio < 0.95) {
                result.hasViolation = true;
                double maxExtDist = 0.0;
                for (const auto& pt : samplePts) {
                    if (!isPointInsideAnalyticalPath(pt, shorterBoundary)) {
                        double d = pointToAnalyticalPathDistance(pt, shorterBoundary);
                        if (d > maxExtDist) {
                            maxExtDist = d;
                        }
                    }
                }
                char msg[512];
                snprintf(msg, sizeof(msg),
                    "Shape %d (h=%.1fmm) extends outside base Shape %d (h=%.1fmm) by %.2fmm",
                    taller.shapeId, taller.depth, shorter.shapeId, shorter.depth, maxExtDist);
                result.warnings.push_back(msg);
            }
        }
    }
    return result;
}

std::vector<HeightBand> computePartHeightBands(
    const std::vector<CamShape>& shapes,
    double topHeight)
{
    std::vector<double> heights;
    heights.push_back(topHeight);

    for (const auto& shape : shapes) {
        if (shape.depth > 0.0 && shape.depth < topHeight - 1e-4) {
            heights.push_back(shape.depth);
        }
    }

    std::sort(heights.begin(), heights.end(), std::greater<double>());
    heights.erase(std::unique(heights.begin(), heights.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-4;
    }), heights.end());

    std::vector<HeightBand> bands;
    for (size_t i = 0; i + 1 < heights.size(); ++i) {
        HeightBand band;
        band.zUpper = heights[i];
        band.zFloor = heights[i + 1];
        bands.push_back(band);
    }
    return bands;
}

std::vector<AnalyticalPath> generateBandClearingLoops(
    const DxfModel& model,
    const std::vector<CamShape>& shapes,
    double zUpper,
    double zFloor,
    double topHeight,
    double toolDiameter,
    double stepoverPercent)
{
    std::vector<AnalyticalPath> clearingLoops;
    double toolRad = toolDiameter / 2.0;
    double stepoverDist = toolDiameter * (stepoverPercent / 100.0);
    if (stepoverDist <= 1e-3) stepoverDist = toolDiameter * 0.7;
    double overrunDist = toolDiameter * (1.0 - stepoverPercent / 100.0);
    if (overrunDist < 0.0) overrunDist = 0.0;

    double openOffset = toolRad - overrunDist;

    std::vector<AnalyticalPath> allUpperObstaclePaths;
    for (const auto& shape : shapes) {
        if (shape.depth >= zUpper - 1e-4) {
            std::vector<size_t> indices(shape.entityIndices.begin(), shape.entityIndices.end());
            auto paths = getShapeAnalyticalPaths(model, indices);
            for (auto& p : paths) {
                ensureLoopOrientation(p, true);
                allUpperObstaclePaths.push_back(p);
            }
        }
    }

    for (const auto& shape : shapes) {
        if (std::abs(shape.depth - zFloor) < 1e-4) {
            std::vector<size_t> indices(shape.entityIndices.begin(), shape.entityIndices.end());
            auto floorPaths = getShapeAnalyticalPaths(model, indices);

            for (auto& fPath : floorPaths) {
                ensureLoopOrientation(fPath, true);

                std::vector<AnalyticalPath> floorObstacles;
                std::vector<AnalyticalPath> floorExpandedObstacles;

                for (const auto& obsPath : allUpperObstaclePaths) {
                    auto obsSample = sampleAnalyticalPath(obsPath, 0.5);
                    if (obsSample.empty()) continue;

                    int insideCount = 0;
                    for (const auto& pt : obsSample) {
                        if (isPointInsideAnalyticalPath(pt, fPath)) {
                            insideCount++;
                        }
                    }

                    if ((double)insideCount / obsSample.size() > 0.5) {
                        floorObstacles.push_back(obsPath);
                        auto exp = offsetAnalyticalPath(obsPath, -toolRad);
                        for (auto& ep : exp) {
                            ensureLoopOrientation(ep, true);
                            floorExpandedObstacles.push_back(ep);
                        }
                    }
                }

                bool isRecessedPocket = false;
                for (const auto& obsPath : allUpperObstaclePaths) {
                    auto fSample = sampleAnalyticalPath(fPath, 0.5);
                    int insideObs = 0;
                    for (const auto& pt : fSample) {
                        if (isPointInsideAnalyticalPath(pt, obsPath)) {
                            insideObs++;
                        }
                    }
                    if (!fSample.empty() && (double)insideObs / fSample.size() > 0.8) {
                        isRecessedPocket = true;
                        break;
                    }
                }

                std::vector<AnalyticalPath> rawFloorLoops;

                if (isRecessedPocket) {
                    std::vector<AnalyticalPath> pocketRings = generatePocketRings(fPath, toolRad, stepoverDist);
                    for (const auto& ring : pocketRings) {
                        if (getAnalyticalPathArea(ring) > 1e-3 && ring.size() >= 2) {
                            rawFloorLoops.push_back(ring);
                        }
                    }
                } else {
                    std::vector<AnalyticalPath> activeOuter = offsetAnalyticalPath(fPath, +openOffset);
                    int pass = 0;
                    int maxPasses = 100;

                    while (!activeOuter.empty() && pass < maxPasses) {
                        pass++;
                        std::vector<AnalyticalPath> nextOuter;
                        for (const auto& loop : activeOuter) {
                            if (getAnalyticalPathArea(loop) > 1e-3 && loop.size() >= 2) {
                                rawFloorLoops.push_back(loop);
                                auto innerRings = offsetAnalyticalPath(loop, +stepoverDist);
                                for (const auto& ir : innerRings) {
                                    if (getAnalyticalPathArea(ir) > 1e-3 && ir.size() >= 2) {
                                        nextOuter.push_back(ir);
                                    }
                                }
                            }
                        }
                        activeOuter = nextOuter;
                    }

                    for (const auto& obsPath : floorObstacles) {
                        std::vector<AnalyticalPath> activeObs = offsetAnalyticalPath(obsPath, -toolRad);
                        int obsPass = 0;
                        while (!activeObs.empty() && obsPass < maxPasses) {
                            obsPass++;
                            std::vector<AnalyticalPath> nextObs;
                            for (const auto& loop : activeObs) {
                                if (getAnalyticalPathArea(loop) > 1e-3 && loop.size() >= 2) {
                                    auto samplePts = sampleAnalyticalPath(loop, 0.5);
                                    if (!samplePts.empty()) {
                                        int insideCount = 0;
                                        for (const auto& pt : samplePts) {
                                            if (isPointInsideAnalyticalPath(pt, fPath)) {
                                                insideCount++;
                                            }
                                        }
                                        if ((double)insideCount / samplePts.size() > 0.5) {
                                            rawFloorLoops.push_back(loop);
                                            auto outerRings = offsetAnalyticalPath(loop, -stepoverDist);
                                            for (const auto& oring : outerRings) {
                                                if (getAnalyticalPathArea(oring) > 1e-3 && oring.size() >= 2) {
                                                    nextObs.push_back(oring);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            activeObs = nextObs;
                        }
                    }
                }

                for (const auto& loop : rawFloorLoops) {
                    if (loop.size() < 2) continue;

                    AnalyticalPath validPrims;
                    for (const auto& prim : loop) {
                        auto samplePts = sampleAnalyticalPath({prim}, 0.2);
                        bool isValid = true;
                        for (const auto& pt : samplePts) {
                            if (!isPointInsideAnalyticalPath(pt, fPath)) {
                                isValid = false;
                                break;
                            }
                            for (const auto& obsPath : floorObstacles) {
                                if (isPointInsideAnalyticalPath(pt, obsPath) || pointToAnalyticalPathDistance(pt, obsPath) < toolRad - 1e-2) {
                                    isValid = false;
                                    break;
                                }
                            }
                            if (!isValid) break;
                        }
                        if (isValid) {
                            validPrims.push_back(prim);
                        }
                    }

                    if (validPrims.empty()) continue;

                    AnalyticalPath sanitized = validPrims;
                    sanitized.trace = loop.trace;
                    if (validPrims.size() < loop.size()) {
                        sanitized.push_back(PathPrimitive::CreateLine(validPrims.back().endPt, validPrims.front().startPt));
                        sanitized = cleanDegeneratePrimitives(sanitized);
                    }

                    if (sanitized.size() >= 2 && getAnalyticalPathArea(sanitized) > 1e-3) {
                        sanitized.trace.role = PathRole::BAND_CLEARING;
                        sanitized.trace.geometrySignature = computeGeometrySignature(sanitized);
                        clearingLoops.push_back(sanitized);
                    }
                }
            }
        }
    }

    return clearingLoops;
}

std::vector<AnalyticalPath> generateOuterCutoutPaths(
    const DxfModel& model,
    const std::vector<CamShape>& shapes,
    double toolDiameter)
{
    std::vector<AnalyticalPath> cutoutPaths;
    if (shapes.empty()) return cutoutPaths;

    double toolRad = toolDiameter / 2.0;

    const CamShape* baseShape = &shapes[0];
    for (const auto& shape : shapes) {
        if (shape.depth < baseShape->depth) {
            baseShape = &shape;
        }
    }

    std::vector<size_t> indices(baseShape->entityIndices.begin(), baseShape->entityIndices.end());
    auto baseAnalyticalPaths = getShapeAnalyticalPaths(model, indices);

    for (auto& path : baseAnalyticalPaths) {
        ensureLoopOrientation(path, true);
        auto outerContours = offsetAnalyticalPath(path, -toolRad);
        for (auto& c : outerContours) {
            ensureLoopOrientation(c, true);
            c.trace.sourceShapeId = baseShape->shapeId;
            c.trace.role = PathRole::OUTER_CUTOUT;
            c.trace.offsetDistance = -toolRad;
            c.trace.entitySignature = computeEntitySignature(c.trace.entityHash, -toolRad);
            c.trace.geometrySignature = computeGeometrySignature(c);
            cutoutPaths.push_back(c);
        }
    }

    return cutoutPaths;
}

std::vector<AnalyticalPath> getShapeAnalyticalPaths(const DxfModel& model, const CamShape& shape) {
    std::vector<size_t> indices(shape.entityIndices.begin(), shape.entityIndices.end());
    auto paths = getShapeAnalyticalPaths(model, indices);
    for (auto& p : paths) {
        p.trace.sourceShapeId = shape.shapeId;
    }
    return paths;
}

const CamShape* findShapeBelow(
    const DxfModel& model,
    const CamPart& part,
    const CamShape& targetShape)
{
    const CamShape* resultShape = nullptr;
    double maxBelowDepth = -1.0;
    double targetDepth = targetShape.depth;

    auto targetAnalyticalPaths = getShapeAnalyticalPaths(model, targetShape);
    std::vector<DxfPoint> targetSamplePts;
    for (const auto& sp : targetAnalyticalPaths) {
        auto pts = sampleAnalyticalPath(sp, 0.5);
        targetSamplePts.insert(targetSamplePts.end(), pts.begin(), pts.end());
    }

    for (size_t sIdx = 0; sIdx < part.shapes.size(); ++sIdx) {
        const auto& candidateShape = part.shapes[sIdx];
        if (candidateShape.shapeId == targetShape.shapeId) continue;
        if (candidateShape.depth < targetDepth - 1e-4) {
            auto candPaths = getShapeAnalyticalPaths(model, candidateShape);
            bool surrounds = false;
            if (!targetSamplePts.empty() && !candPaths.empty()) {
                int insideCount = 0;
                for (const auto& pt : targetSamplePts) {
                    for (const auto& cPath : candPaths) {
                        if (isPointInsideAnalyticalPath(pt, cPath)) { insideCount++; break; }
                    }
                }
                if ((double)insideCount / targetSamplePts.size() > 0.5) surrounds = true;
            }
            if (surrounds) {
                if (resultShape == nullptr || candidateShape.depth > maxBelowDepth) {
                    resultShape = &candidateShape;
                    maxBelowDepth = candidateShape.depth;
                }
            }
        }
    }

    if (!resultShape) {
        for (size_t sIdx = 0; sIdx < part.shapes.size(); ++sIdx) {
            const auto& candidateShape = part.shapes[sIdx];
            if (candidateShape.shapeId == targetShape.shapeId) continue;
            if (candidateShape.depth < targetDepth - 1e-4) {
                if (resultShape == nullptr || candidateShape.depth > maxBelowDepth) {
                    resultShape = &candidateShape;
                    maxBelowDepth = candidateShape.depth;
                }
            }
        }
    }

    return resultShape;
}

std::vector<MachiningLevel> computePartMachiningLevels(
    const DxfModel& model,
    const CamPart& part)
{
    std::vector<MachiningLevel> levels;
    if (part.shapes.empty()) return levels;

    for (int sIdx = 0; sIdx < (int)part.shapes.size(); ++sIdx) {
        const auto& shape = part.shapes[sIdx];
        const CamShape* shapeBelow = findShapeBelow(model, part, shape);
        int belowId = shapeBelow ? shapeBelow->shapeId : -1;
        double depth = shape.depth;

        bool found = false;
        for (auto& lvl : levels) {
            if (std::abs(lvl.depth - depth) < 1e-4 && lvl.belowShapeId == belowId) {
                lvl.shapeIndices.push_back(sIdx);
                found = true;
                break;
            }
        }
        if (!found) {
            MachiningLevel newLvl;
            newLvl.depth = depth;
            newLvl.belowShapeId = belowId;
            newLvl.shapeIndices.push_back(sIdx);
            levels.push_back(newLvl);
        }
    }

    std::sort(levels.begin(), levels.end(), [](const MachiningLevel& a, const MachiningLevel& b) {
        if (std::abs(a.depth - b.depth) > 1e-4) {
            return a.depth > b.depth;
        }
        return a.belowShapeId < b.belowShapeId;
    });

    for (size_t i = 0; i < levels.size(); ++i) {
        levels[i].levelIndex = (int)(i + 1);
    }

    return levels;
}

ShapePaths computeShapePaths(
    const DxfModel& model,
    const CamPart& part,
    const CamShape& shape,
    double toolDiameter,
    double overcutPercent)
{
    ShapePaths sp;
    sp.shapeId = shape.shapeId;
    sp.shape = &shape;

    if (toolDiameter <= 1e-4) toolDiameter = 2.0;
    if (overcutPercent < 0.0) overcutPercent = 30.0;
    double toolRadius = toolDiameter * 0.5;

    auto analyticalPaths = getShapeAnalyticalPaths(model, shape);
    if (analyticalPaths.empty()) return sp;

    // 1. Role A: Island / Boss (3 paths: outline, toolPath, cutZone)
    for (const auto& path : analyticalPaths) {
        AnalyticalPath loop = path;
        ensureLoopOrientation(loop, true);
        loop.trace.sourcePartId = part.id;
        loop.trace.sourceShapeId = shape.shapeId;
        loop.trace.role = PathRole::SHAPE_OUTLINE;
        loop.trace.offsetDistance = 0.0;
        loop.trace.entitySignature = computeEntitySignature(loop.trace.entityHash, 0.0);
        loop.trace.geometrySignature = computeGeometrySignature(loop);
        sp.island.outline.push_back(loop);

        auto tPaths = offsetAnalyticalPath(loop, -toolRadius);
        for (auto tp : tPaths) {
            ensureLoopOrientation(tp, true);
            tp.trace.sourcePartId = part.id;
            tp.trace.sourceShapeId = shape.shapeId;
            tp.trace.role = PathRole::SHAPE_TOOLPATH;
            tp.trace.offsetDistance = -toolRadius;
            tp.trace.entitySignature = computeEntitySignature(tp.trace.entityHash, -toolRadius);
            tp.trace.geometrySignature = computeGeometrySignature(tp);
            sp.island.toolPath.push_back(tp);
        }

        auto cPaths = offsetAnalyticalPath(loop, -toolDiameter);
        for (auto cp : cPaths) {
            ensureLoopOrientation(cp, true);
            cp.trace.sourcePartId = part.id;
            cp.trace.sourceShapeId = shape.shapeId;
            cp.trace.role = PathRole::SHAPE_CUTZONE;
            cp.trace.offsetDistance = -toolDiameter;
            cp.trace.entitySignature = computeEntitySignature(cp.trace.entityHash, -toolDiameter);
            cp.trace.geometrySignature = computeGeometrySignature(cp);
            sp.island.cutZone.push_back(cp);
        }
    }

    // 2. Role B: Material / Floor (4 paths: outline, toolPath, cutZoneInner, cutZoneOuter)
    double expansionFactor = (overcutPercent / 100.0) - 0.5;
    double expansionDist = expansionFactor * toolDiameter;
    double offsetDist = -expansionDist;

    for (const auto& bPath : analyticalPaths) {
        AnalyticalPath bLoop = bPath;
        ensureLoopOrientation(bLoop, true);
        bLoop.trace.sourcePartId = part.id;
        bLoop.trace.sourceShapeId = shape.shapeId;
        bLoop.trace.role = PathRole::MATERIAL_OUTLINE;
        bLoop.trace.offsetDistance = 0.0;
        bLoop.trace.entitySignature = computeEntitySignature(bLoop.trace.entityHash, 0.0);
        bLoop.trace.geometrySignature = computeGeometrySignature(bLoop);
        sp.material.outline.push_back(bLoop);

        AnalyticalPath rawToolPath = offsetAnalyticalPathRaw(bLoop, offsetDist);
        if (rawToolPath.size() >= 2) {
            AnalyticalPath cleanTpUnspurred = cleanDegeneratePrimitives(rawToolPath);
            cleanTpUnspurred.trace = rawToolPath.trace;
            cleanTpUnspurred.trace.sourcePartId = part.id;
            cleanTpUnspurred.trace.sourceShapeId = shape.shapeId;
            cleanTpUnspurred.trace.role = PathRole::MATERIAL_TOOLPATH;
            cleanTpUnspurred.trace.offsetDistance = offsetDist;
            cleanTpUnspurred.trace.entitySignature = computeEntitySignature(cleanTpUnspurred.trace.entityHash, offsetDist);
            cleanTpUnspurred.trace.geometrySignature = computeGeometrySignature(cleanTpUnspurred);

            if (cleanTpUnspurred.size() >= 2) {
                AnalyticalPath cleanTp = cleanTpUnspurred;
                std::vector<std::pair<DxfPoint, DxfPoint>> spursAtCorners;

                if (overcutPercent < 50.0) {
                    double spurDist = (0.5 - (overcutPercent / 100.0)) * toolDiameter;
                    if (spurDist > 1e-4) {
                        size_t numPrims = cleanTp.size();
                        for (size_t i = 0; i < numPrims; ++i) {
                            const auto& prim1 = cleanTp[i];
                            const auto& prim2 = cleanTp[(i + 1) % numPrims];

                            DxfPoint t1 = getPrimitiveEndTangent(prim1);
                            DxfPoint t2 = getPrimitiveStartTangent(prim2);
                            double crossZ = t1.x * t2.y - t1.y * t2.x;
                            double dot = t1.x * t2.x + t1.y * t2.y;
                            double angleTurn = std::atan2(crossZ, dot);

                            if (angleTurn > 1e-3) {
                                DxfPoint vertexPt = prim1.endPt;
                                DxfPoint n1(-t1.y, t1.x);
                                DxfPoint n2(-t2.y, t2.x);

                                double nx = -n1.x - n2.x;
                                double ny = -n1.y - n2.y;
                                double nLen = std::hypot(nx, ny);
                                if (nLen > 1e-6) {
                                    nx /= nLen;
                                    ny /= nLen;

                                    double halfAngle = angleTurn * 0.5;
                                    double cosHalf = std::cos(halfAngle);

                                    if (cosHalf > 0.05) {
                                        double spurTravelLen = spurDist * ((1.0 / cosHalf) - 1.0);

                                        if (spurTravelLen > 1e-4) {
                                            DxfPoint apexPt(vertexPt.x + nx * spurTravelLen,
                                                           vertexPt.y + ny * spurTravelLen);

                                            cleanTp[i].hasEndSpur = true;
                                            cleanTp[i].endSpurApex = apexPt;
                                            spursAtCorners.push_back({vertexPt, apexPt});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ensureLoopOrientation(cleanTp, true);
                cleanTp.trace = cleanTpUnspurred.trace;
                cleanTp.trace.geometrySignature = computeGeometrySignature(cleanTp);
                sp.material.toolPath.push_back(cleanTp);

                auto innerPaths = offsetAnalyticalPath(cleanTpUnspurred, toolRadius);
                for (auto inPath : innerPaths) {
                    ensureLoopOrientation(inPath, true);
                    inPath.trace.sourcePartId = part.id;
                    inPath.trace.sourceShapeId = shape.shapeId;
                    inPath.trace.role = PathRole::MATERIAL_CUTZONE_INNER;
                    inPath.trace.offsetDistance = offsetDist + toolRadius;
                    inPath.trace.entitySignature = computeEntitySignature(inPath.trace.entityHash, inPath.trace.offsetDistance);
                    inPath.trace.geometrySignature = computeGeometrySignature(inPath);
                    sp.material.cutZoneInner.push_back(inPath);
                }

                auto baseOuterPaths = offsetAnalyticalPath(cleanTpUnspurred, -toolRadius);

                if (overcutPercent < 50.0 && !spursAtCorners.empty() && !baseOuterPaths.empty()) {
                    for (auto& outPath : baseOuterPaths) {
                        for (auto& prim : outPath) {
                            if (prim.type == PrimitiveType::ARC && prim.radius > 1e-6) {
                                for (const auto& sp : spursAtCorners) {
                                    const DxfPoint& vPt = sp.first;
                                    const DxfPoint& aPt = sp.second;
                                    double dCenter = std::hypot(prim.center.x - vPt.x, prim.center.y - vPt.y);
                                    if (dCenter < 1.0) {
                                        prim.center = aPt;
                                        prim.radius = toolRadius;
                                    }
                                }
                            }
                        }
                    }
                }

                for (auto outPath : baseOuterPaths) {
                    ensureLoopOrientation(outPath, true);
                    outPath.trace.sourcePartId = part.id;
                    outPath.trace.sourceShapeId = shape.shapeId;
                    outPath.trace.role = PathRole::MATERIAL_CUTZONE_OUTER;
                    outPath.trace.offsetDistance = offsetDist - toolRadius;
                    outPath.trace.entitySignature = computeEntitySignature(outPath.trace.entityHash, outPath.trace.offsetDistance);
                    outPath.trace.geometrySignature = computeGeometrySignature(outPath);
                    sp.material.cutZoneOuter.push_back(outPath);
                }
            }
        }
    }

    return sp;
}

std::vector<ShapePaths> computeAllPartShapePaths(
    const DxfModel& model,
    const CamPart& part,
    double toolDiameter,
    double overcutPercent)
{
    std::vector<ShapePaths> result;
    result.reserve(part.shapes.size());
    for (const auto& shape : part.shapes) {
        result.push_back(computeShapePaths(model, part, shape, toolDiameter, overcutPercent));
    }
    return result;
}

SelectedShapeProcessingPaths computeSelectedShapeProcessingPaths(
    const DxfModel& model,
    const CamPart& part,
    int selectedShapeIdx,
    const std::vector<ShapePaths>& allShapePaths)
{
    SelectedShapeProcessingPaths procPaths;
    if (selectedShapeIdx < 0 || selectedShapeIdx >= (int)part.shapes.size() ||
        selectedShapeIdx >= (int)allShapePaths.size()) {
        return procPaths;
    }

    procPaths.shape = &allShapePaths[selectedShapeIdx];
    const auto& selShape = part.shapes[selectedShapeIdx];
    const CamShape* shapeBelow = findShapeBelow(model, part, selShape);

    if (shapeBelow != nullptr) {
        for (const auto& sp : allShapePaths) {
            if (sp.shapeId == shapeBelow->shapeId) {
                procPaths.material = &sp;
                break;
            }
        }

        // Find sibling islands on the same material base
        for (size_t sIdx = 0; sIdx < part.shapes.size() && sIdx < allShapePaths.size(); ++sIdx) {
            if ((int)sIdx == selectedShapeIdx) continue;
            const auto& candidateShape = part.shapes[sIdx];
            if (candidateShape.shapeId == shapeBelow->shapeId) continue;
            if (candidateShape.depth <= shapeBelow->depth + 1e-4) continue;

            const CamShape* candBelow = findShapeBelow(model, part, candidateShape);
            if (candBelow != nullptr && candBelow->shapeId == shapeBelow->shapeId) {
                procPaths.islandShapes.push_back(&allShapePaths[sIdx]);
            }
        }
    }

    return procPaths;
}

MachiningLevelPaths computeMachiningLevelPaths(
    const DxfModel& model,
    const CamPart& part,
    const MachiningLevel& level,
    const std::vector<ShapePaths>& allShapePaths)
{
    MachiningLevelPaths machPaths;
    machPaths.levelIndex = level.levelIndex;
    machPaths.depth = level.depth;
    machPaths.belowShapeId = level.belowShapeId;

    if (part.shapes.empty() || allShapePaths.empty()) {
        return machPaths;
    }

    // 1. Resolve material floor shape
    if (level.belowShapeId != -1) {
        for (const auto& sp : allShapePaths) {
            if (sp.shapeId == level.belowShapeId) {
                machPaths.material = &sp;
                break;
            }
        }
    }

    // 2. Resolve islands on this level
    std::set<int> addedShapeIds;

    for (int sIdx : level.shapeIndices) {
        if (sIdx >= 0 && sIdx < (int)allShapePaths.size()) {
            machPaths.islands.push_back(&allShapePaths[sIdx]);
            addedShapeIds.insert(allShapePaths[sIdx].shapeId);
        }
    }

    // Also include any sibling obstacles standing on this same floor
    if (level.belowShapeId != -1) {
        const CamShape* shapeBelow = nullptr;
        for (const auto& s : part.shapes) {
            if (s.shapeId == level.belowShapeId) {
                shapeBelow = &s;
                break;
            }
        }

        if (shapeBelow != nullptr) {
            for (size_t sIdx = 0; sIdx < part.shapes.size() && sIdx < allShapePaths.size(); ++sIdx) {
                const auto& candidateShape = part.shapes[sIdx];
                if (addedShapeIds.count(candidateShape.shapeId)) continue;
                if (candidateShape.shapeId == shapeBelow->shapeId) continue;
                if (candidateShape.depth <= shapeBelow->depth + 1e-4) continue;

                const CamShape* candBelow = findShapeBelow(model, part, candidateShape);
                if (candBelow != nullptr && candBelow->shapeId == shapeBelow->shapeId) {
                    machPaths.islands.push_back(&allShapePaths[sIdx]);
                    addedShapeIds.insert(candidateShape.shapeId);
                }
            }
        }
    }

    return machPaths;
}
