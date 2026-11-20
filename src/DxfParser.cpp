#include "DxfParser.h"
#include <iostream>
#include <cctype>
#include <functional>

static std::string trimString(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
            return false;
    }
    return true;
}

static const unsigned char ACI_PALETTE[256][3] = {
    {0,0,0},       // 0: BYBLOCK
    {255,0,0},     // 1: Red
    {255,255,0},   // 2: Yellow
    {0,255,0},     // 3: Green
    {0,255,255},   // 4: Cyan
    {0,0,255},     // 5: Blue
    {255,0,255},   // 6: Magenta
    {0,0,0},       // 7: White/Black -> Black on white canvas
    {128,128,128}, // 8: Dark Grey
    {192,192,192}, // 9: Light Grey
    // 10-19: Red family
    {255,0,0}, {255,127,127}, {204,0,0}, {204,102,102}, {153,0,0}, {153,76,76}, {102,0,0}, {102,51,51}, {51,0,0}, {51,25,25},
    // 20-29: Red-Orange family
    {255,63,0}, {255,159,127}, {204,51,0}, {204,127,102}, {153,38,0}, {153,95,76}, {102,25,0}, {102,63,51}, {51,12,0}, {51,31,25},
    // 30-39: Orange family
    {255,127,0}, {255,191,127}, {204,102,0}, {204,153,102}, {153,76,0}, {153,114,76}, {102,51,0}, {102,76,51}, {51,25,0}, {51,38,25},
    // 40-49: Yellow-Orange family
    {255,191,0}, {255,223,127}, {204,153,0}, {204,178,102}, {153,114,0}, {153,133,76}, {102,76,0}, {102,89,51}, {51,38,0}, {51,44,25},
    // 50-59: Yellow family
    {255,255,0}, {255,255,127}, {204,204,0}, {204,204,102}, {153,153,0}, {153,153,76}, {102,102,0}, {102,102,51}, {51,51,0}, {51,51,25},
    // 60-69: Yellow-Green family
    {191,255,0}, {223,255,127}, {153,204,0}, {178,204,102}, {114,153,0}, {133,153,76}, {76,102,0}, {89,102,51}, {38,51,0}, {44,51,25},
    // 70-79: Green-Yellow family
    {127,255,0}, {191,255,127}, {102,204,0}, {153,204,102}, {76,153,0}, {114,153,76}, {51,102,0}, {76,102,51}, {25,51,0}, {38,51,25},
    // 80-89: Green family
    {63,255,0}, {159,255,127}, {51,204,0}, {127,204,102}, {38,153,0}, {95,153,76}, {25,102,0}, {63,102,51}, {12,51,0}, {31,51,25},
    // 90-99: Green family
    {0,255,0}, {127,255,127}, {0,204,0}, {102,204,102}, {0,153,0}, {76,153,76}, {0,102,0}, {51,102,51}, {0,51,0}, {25,51,25},
    // 100-109: Green-Cyan family
    {0,255,63}, {127,255,159}, {0,204,51}, {102,204,127}, {0,153,38}, {76,153,95}, {0,102,25}, {51,102,63}, {0,51,12}, {25,51,31},
    // 110-119: Cyan-Green family
    {0,255,127}, {127,255,191}, {0,204,102}, {102,204,153}, {0,153,76}, {76,153,114}, {0,102,51}, {51,102,76}, {0,51,25}, {25,51,38},
    // 120-129: Cyan family
    {0,255,191}, {127,255,223}, {0,204,153}, {102,204,178}, {0,153,114}, {76,153,133}, {0,102,76}, {51,102,89}, {0,51,38}, {25,51,44},
    // 130-139: Cyan family
    {0,255,255}, {127,255,255}, {0,204,204}, {102,204,204}, {0,153,153}, {76,153,153}, {0,102,102}, {51,102,102}, {0,51,51}, {25,51,51},
    // 140-149: Cyan-Blue family
    {0,191,255}, {127,223,255}, {0,153,204}, {102,178,204}, {0,114,153}, {76,133,153}, {0,76,102}, {51,89,102}, {0,38,51}, {25,44,51},
    // 150-159: Blue-Cyan family
    {0,127,255}, {127,191,255}, {0,102,204}, {102,153,204}, {0,76,153}, {76,114,153}, {0,51,102}, {51,76,102}, {0,25,51}, {25,38,51},
    // 160-169: Blue family
    {0,63,255}, {127,159,255}, {0,51,204}, {102,127,204}, {0,38,153}, {76,95,153}, {0,25,102}, {51,63,102}, {0,12,51}, {25,31,51},
    // 170-179: Blue family
    {0,0,255}, {127,127,255}, {0,0,204}, {102,102,204}, {0,0,153}, {76,76,153}, {0,0,102}, {51,51,102}, {0,0,51}, {25,25,51},
    // 180-189: Blue-Magenta family
    {63,0,255}, {159,127,255}, {51,0,204}, {127,102,204}, {38,0,153}, {95,76,153}, {25,0,102}, {63,51,102}, {12,0,51}, {31,25,51},
    // 190-199: Magenta-Blue family
    {127,0,255}, {191,127,255}, {102,0,204}, {153,102,204}, {76,0,153}, {114,76,153}, {51,0,102}, {76,51,102}, {25,0,51}, {38,25,51},
    // 200-209: Magenta family
    {191,0,255}, {223,127,255}, {153,0,204}, {178,102,204}, {114,0,153}, {133,76,153}, {76,0,102}, {89,51,102}, {38,0,51}, {44,25,51},
    // 210-219: Magenta family
    {255,0,255}, {255,127,255}, {204,0,204}, {204,102,204}, {153,0,153}, {153,76,153}, {102,0,102}, {102,51,102}, {51,0,51}, {51,25,51},
    // 220-229: Magenta-Red family
    {255,0,191}, {255,127,223}, {204,0,153}, {204,102,178}, {153,0,114}, {153,76,133}, {102,0,76}, {102,51,89}, {51,0,38}, {51,25,44},
    // 230-239: Red-Magenta family
    {255,0,127}, {255,127,191}, {204,0,102}, {204,102,153}, {153,0,76}, {153,76,114}, {102,0,51}, {102,51,76}, {51,0,25}, {51,25,38},
    // 240-249: Red family
    {255,0,63}, {255,127,159}, {204,0,51}, {204,102,127}, {153,0,38}, {153,76,95}, {102,0,25}, {102,51,63}, {51,0,12}, {51,25,31},
    // 250-255: Greyscale
    {51,51,51}, {91,91,91}, {132,132,132}, {173,173,173}, {214,214,214}, {0,0,0}
};

DxfColor DxfModel::aciToRgb(int colorIndex) {
    if (colorIndex < 0) colorIndex = -colorIndex;
    if (colorIndex > 255) colorIndex = 7;

    unsigned char r = ACI_PALETTE[colorIndex][0];
    unsigned char g = ACI_PALETTE[colorIndex][1];
    unsigned char b = ACI_PALETTE[colorIndex][2];

    if (r > 240 && g > 240 && b > 240) {
        r = g = b = 0;
    }
    return DxfColor{r, g, b};
}

DxfLayer* DxfModel::findLayer(const std::string& name) {
    for (auto& layer : layers) {
        if (iequals(layer.name, name)) return &layer;
    }
    return nullptr;
}

const DxfLayer* DxfModel::findLayer(const std::string& name) const {
    for (const auto& layer : layers) {
        if (iequals(layer.name, name)) return &layer;
    }
    return nullptr;
}

DxfBlock* DxfModel::findBlock(const std::string& name) {
    for (auto& blk : blocks) {
        if (iequals(blk.name, name)) return &blk;
    }
    return nullptr;
}

const DxfBlock* DxfModel::findBlock(const std::string& name) const {
    for (const auto& blk : blocks) {
        if (iequals(blk.name, name)) return &blk;
    }
    return nullptr;
}

DxfColor DxfModel::getEntityColor(const DxfEntity& entity) const {
    if (entity.hasTrueColor && entity.rgbColor > 0) {
        unsigned char r = (entity.rgbColor >> 16) & 0xFF;
        unsigned char g = (entity.rgbColor >> 8) & 0xFF;
        unsigned char b = entity.rgbColor & 0xFF;
        if (r > 240 && g > 240 && b > 240) return DxfColor{0, 0, 0};
        return DxfColor{r, g, b};
    }

    if (entity.colorIndex != 256 && entity.colorIndex != 0) {
        return aciToRgb(entity.colorIndex);
    }

    const DxfLayer* layer = findLayer(entity.layerName);
    if (layer) {
        if (layer->hasTrueColor && layer->rgbColor > 0) {
            unsigned char r = (layer->rgbColor >> 16) & 0xFF;
            unsigned char g = (layer->rgbColor >> 8) & 0xFF;
            unsigned char b = layer->rgbColor & 0xFF;
            if (r > 240 && g > 240 && b > 240) return DxfColor{0, 0, 0};
            return DxfColor{r, g, b};
        }
        return aciToRgb(layer->colorIndex);
    }

    return DxfColor{0, 0, 0};
}

std::vector<DxfPoint> DxfModel::tessellatePolyline(const std::vector<DxfVertex>& vertices, bool isClosed) {
    std::vector<DxfPoint> pts;
    if (vertices.empty()) return pts;

    size_t count = vertices.size();
    size_t numSegments = isClosed ? count : (count - 1);

    for (size_t i = 0; i < numSegments; ++i) {
        DxfVertex v1 = vertices[i];
        DxfVertex v2 = vertices[(i + 1) % count];

        pts.push_back(DxfPoint(v1.x, v1.y));

        double b = v1.bulge;
        if (std::abs(b) > 1e-6) {
            double dx = v2.x - v1.x;
            double dy = v2.y - v1.y;
            double d = std::sqrt(dx * dx + dy * dy);

            if (d > 1e-6) {
                double r = (d * (1.0 + b * b)) / (4.0 * std::abs(b));
                double s = (d * (1.0 - b * b)) / (4.0 * b);

                double mx = (v1.x + v2.x) / 2.0;
                double my = (v1.y + v2.y) / 2.0;

                double cx = mx - s * (dy / d);
                double cy = my + s * (dx / d);

                double startRad = std::atan2(v1.y - cy, v1.x - cx);
                double endRad = std::atan2(v2.y - cy, v2.x - cx);

                double sweep = 0.0;
                if (b > 0) {
                    if (endRad <= startRad) endRad += 2.0 * M_PI;
                    sweep = endRad - startRad;
                } else {
                    if (endRad >= startRad) endRad -= 2.0 * M_PI;
                    sweep = endRad - startRad;
                }

                int steps = std::max(6, (int)(std::abs(sweep) / (M_PI / 18.0)));
                for (int k = 1; k < steps; ++k) {
                    double phi = startRad + sweep * k / steps;
                    double ax = cx + r * std::cos(phi);
                    double ay = cy + r * std::sin(phi);
                    pts.push_back(DxfPoint(ax, ay));
                }
            }
        }
    }

    if (!isClosed && count > 0) {
        pts.push_back(DxfPoint(vertices.back().x, vertices.back().y));
    }

    return pts;
}

static DxfPoint transformPoint(const DxfPoint& pt, const DxfPoint& basePt, const DxfPoint& insPt, double sx, double sy, double rotDeg) {
    double rad = rotDeg * M_PI / 180.0;
    double dx = (pt.x - basePt.x) * sx;
    double dy = (pt.y - basePt.y) * sy;
    double rx = insPt.x + dx * std::cos(rad) - dy * std::sin(rad);
    double ry = insPt.y + dx * std::sin(rad) + dy * std::cos(rad);
    return DxfPoint(rx, ry);
}

void DxfModel::flattenBlockInsertions() {
    std::vector<DxfEntity> flattened;
    flattened.reserve(entities.size());

    std::function<void(const DxfEntity&, const DxfPoint&, double, double, double, const std::string&, int)> expandEntity;

    expandEntity = [&](const DxfEntity& ent, const DxfPoint& insPt, double sx, double sy, double rotDeg, const std::string& parentLayer, int depth) {
        if (depth > 10) return;

        if (ent.type == DxfEntityType::INSERT) {
            const DxfBlock* blk = findBlock(ent.blockName);
            if (!blk) return;

            DxfPoint combinedIns = transformPoint(ent.center, DxfPoint(0,0), insPt, sx, sy, rotDeg);
            double combinedSx = sx * ent.scaleX;
            double combinedSy = sy * ent.scaleY;
            double combinedRot = rotDeg + ent.rotationAngle;
            std::string effectiveLayer = ent.layerName.empty() || ent.layerName == "0" ? parentLayer : ent.layerName;

            for (const auto& childEnt : blk->entities) {
                expandEntity(childEnt, combinedIns, combinedSx, combinedSy, combinedRot, effectiveLayer, depth + 1);
            }
        } else {
            DxfEntity transformed = ent;
            if (transformed.layerName.empty() || transformed.layerName == "0") {
                transformed.layerName = parentLayer;
            }

            if (transformed.type == DxfEntityType::LINE) {
                for (auto& pt : transformed.points) {
                    pt = transformPoint(pt, DxfPoint(0,0), insPt, sx, sy, rotDeg);
                }
            } else if (transformed.type == DxfEntityType::POLYLINE) {
                for (auto& v : transformed.polylineVertices) {
                    DxfPoint p = transformPoint(DxfPoint(v.x, v.y), DxfPoint(0,0), insPt, sx, sy, rotDeg);
                    v.x = p.x;
                    v.y = p.y;
                }
                transformed.points = tessellatePolyline(transformed.polylineVertices, transformed.isClosed);
            } else if (transformed.type == DxfEntityType::CIRCLE || transformed.type == DxfEntityType::ARC) {
                transformed.center = transformPoint(transformed.center, DxfPoint(0,0), insPt, sx, sy, rotDeg);
                transformed.radius *= (std::abs(sx) + std::abs(sy)) / 2.0;
                transformed.startAngle += rotDeg;
                transformed.endAngle += rotDeg;
            } else if (transformed.type == DxfEntityType::ELLIPSE) {
                transformed.center = transformPoint(transformed.center, DxfPoint(0,0), insPt, sx, sy, rotDeg);
                DxfPoint m = transformPoint(transformed.majorAxis, DxfPoint(0,0), DxfPoint(0,0), sx, sy, rotDeg);
                transformed.majorAxis = m;
            } else if (transformed.type == DxfEntityType::POINT || transformed.type == DxfEntityType::SPLINE || transformed.type == DxfEntityType::TEXT || transformed.type == DxfEntityType::SOLID) {
                for (auto& pt : transformed.points) {
                    pt = transformPoint(pt, DxfPoint(0,0), insPt, sx, sy, rotDeg);
                }
            }
            flattened.push_back(transformed);
        }
    };

    for (const auto& ent : entities) {
        if (ent.type == DxfEntityType::INSERT) {
            expandEntity(ent, DxfPoint(0,0), 1.0, 1.0, 0.0, ent.layerName, 0);
        } else {
            DxfEntity processed = ent;
            if (processed.type == DxfEntityType::POLYLINE && !processed.polylineVertices.empty()) {
                processed.points = tessellatePolyline(processed.polylineVertices, processed.isClosed);
            }
            flattened.push_back(processed);
        }
    }

    entities = std::move(flattened);
}

bool DxfModel::loadFromFile(const std::string& filename) {
    clear();
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string lineCodeStr;
    std::string lineValStr;

    bool readingLayer = false;
    bool readingEntity = false;
    bool readingBlock = false;

    DxfLayer curLayer;
    DxfEntity curEnt;
    DxfBlock curBlock;
    bool hasPolyline = false;
    DxfEntity polylineEnt;

    auto finishLayer = [&]() {
        if (readingLayer) {
            if (!curLayer.name.empty() && findLayer(curLayer.name) == nullptr) {
                layers.push_back(curLayer);
            }
            readingLayer = false;
        }
    };

    auto finishEntity = [&]() {
        if (readingEntity) {
            if (curEnt.type == DxfEntityType::POLYLINE && !curEnt.polylineVertices.empty()) {
                curEnt.points = tessellatePolyline(curEnt.polylineVertices, curEnt.isClosed);
            }

            if (readingBlock) {
                curBlock.entities.push_back(curEnt);
            } else {
                entities.push_back(curEnt);
            }
            readingEntity = false;
        }
    };

    while (std::getline(file, lineCodeStr)) {
        if (!std::getline(file, lineValStr)) break;

        int code = 0;
        try {
            code = std::stoi(trimString(lineCodeStr));
        } catch (...) {
            continue;
        }

        std::string val = trimString(lineValStr);

        if (code == 0) {
            finishLayer();
            finishEntity();

            if (val == "LAYER") {
                curLayer = DxfLayer();
                readingLayer = true;
            } else if (val == "BLOCK") {
                curBlock = DxfBlock();
                readingBlock = true;
            } else if (val == "ENDBLK") {
                if (readingBlock && !curBlock.name.empty()) {
                    blocks.push_back(curBlock);
                }
                readingBlock = false;
            } else if (val == "LINE") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::LINE;
                curEnt.points.resize(2);
                readingEntity = true;
            } else if (val == "CIRCLE") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::CIRCLE;
                readingEntity = true;
            } else if (val == "ARC") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::ARC;
                readingEntity = true;
            } else if (val == "LWPOLYLINE") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::POLYLINE;
                readingEntity = true;
            } else if (val == "POLYLINE") {
                hasPolyline = true;
                polylineEnt = DxfEntity();
                polylineEnt.type = DxfEntityType::POLYLINE;
            } else if (val == "VERTEX" && hasPolyline) {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::POINT;
                readingEntity = true;
            } else if (val == "SEQEND" && hasPolyline) {
                polylineEnt.points = tessellatePolyline(polylineEnt.polylineVertices, polylineEnt.isClosed);
                if (readingBlock) {
                    curBlock.entities.push_back(polylineEnt);
                } else {
                    entities.push_back(polylineEnt);
                }
                hasPolyline = false;
                readingEntity = false;
            } else if (val == "ELLIPSE") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::ELLIPSE;
                readingEntity = true;
            } else if (val == "SPLINE") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::SPLINE;
                readingEntity = true;
            } else if (val == "INSERT") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::INSERT;
                readingEntity = true;
            } else if (val == "TEXT" || val == "MTEXT") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::TEXT;
                curEnt.points.resize(1);
                readingEntity = true;
            } else if (val == "SOLID" || val == "TRACE" || val == "3DFACE") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::SOLID;
                curEnt.points.resize(4);
                readingEntity = true;
            } else if (val == "POINT") {
                curEnt = DxfEntity();
                curEnt.type = DxfEntityType::POINT;
                curEnt.points.resize(1);
                readingEntity = true;
            }
        } else if (readingLayer) {
            if (code == 2) {
                curLayer.name = val;
            } else if (code == 62) {
                try {
                    int c = std::stoi(val);
                    if (c < 0) {
                        curLayer.visible = false;
                        curLayer.colorIndex = -c;
                    } else {
                        curLayer.colorIndex = c;
                    }
                } catch(...) {}
            } else if (code == 420) {
                try {
                    uint32_t rgb = (uint32_t)std::stoul(val);
                    if (rgb > 0) {
                        curLayer.rgbColor = rgb;
                        curLayer.hasTrueColor = true;
                    }
                } catch(...) {}
            }
        } else if (readingBlock && !readingEntity && !hasPolyline) {
            if (code == 2) {
                curBlock.name = val;
            } else if (code == 10) {
                try { curBlock.basePoint.x = std::stod(val); } catch(...) {}
            } else if (code == 20) {
                try { curBlock.basePoint.y = std::stod(val); } catch(...) {}
            }
        } else if (readingEntity || hasPolyline) {
            double numVal = 0.0;
            try {
                numVal = std::stod(val);
            } catch (...) {}

            if (code == 8) {
                if (readingEntity) curEnt.layerName = val;
                if (hasPolyline) polylineEnt.layerName = val;
            } else if (code == 62) {
                try {
                    int cIdx = std::stoi(val);
                    if (readingEntity) curEnt.colorIndex = cIdx;
                    if (hasPolyline) polylineEnt.colorIndex = cIdx;
                } catch(...) {}
            } else if (code == 420) {
                try {
                    uint32_t rgb = (uint32_t)std::stoul(val);
                    if (rgb > 0) {
                        if (readingEntity) { curEnt.rgbColor = rgb; curEnt.hasTrueColor = true; }
                        if (hasPolyline) { polylineEnt.rgbColor = rgb; polylineEnt.hasTrueColor = true; }
                    }
                } catch(...) {}
            } else if (code == 1) {
                if (readingEntity) curEnt.textString = val;
            } else if (code == 2) {
                if (readingEntity && curEnt.type == DxfEntityType::INSERT) curEnt.blockName = val;
            }

            if (curEnt.type == DxfEntityType::LINE) {
                if (code == 10) curEnt.points[0].x = numVal;
                else if (code == 20) curEnt.points[0].y = numVal;
                else if (code == 11) curEnt.points[1].x = numVal;
                else if (code == 21) curEnt.points[1].y = numVal;
            } else if (curEnt.type == DxfEntityType::CIRCLE) {
                if (code == 10) curEnt.center.x = numVal;
                else if (code == 20) curEnt.center.y = numVal;
                else if (code == 40) curEnt.radius = numVal;
            } else if (curEnt.type == DxfEntityType::ARC) {
                if (code == 10) curEnt.center.x = numVal;
                else if (code == 20) curEnt.center.y = numVal;
                else if (code == 40) curEnt.radius = numVal;
                else if (code == 50) curEnt.startAngle = numVal;
                else if (code == 51) curEnt.endAngle = numVal;
            } else if (curEnt.type == DxfEntityType::POLYLINE) {
                if (code == 70) {
                    int flags = std::stoi(val);
                    curEnt.isClosed = (flags & 1) != 0;
                } else if (code == 10) {
                    curEnt.polylineVertices.push_back(DxfVertex(numVal, 0.0, 0.0));
                } else if (code == 20 && !curEnt.polylineVertices.empty()) {
                    curEnt.polylineVertices.back().y = numVal;
                } else if (code == 42 && !curEnt.polylineVertices.empty()) {
                    curEnt.polylineVertices.back().bulge = numVal;
                }
            } else if (hasPolyline && code == 70 && !readingEntity) {
                int flags = std::stoi(val);
                polylineEnt.isClosed = (flags & 1) != 0;
            } else if (hasPolyline && curEnt.type == DxfEntityType::POINT) {
                if (code == 10) {
                    polylineEnt.polylineVertices.push_back(DxfVertex(numVal, 0.0, 0.0));
                } else if (code == 20 && !polylineEnt.polylineVertices.empty()) {
                    polylineEnt.polylineVertices.back().y = numVal;
                } else if (code == 42 && !polylineEnt.polylineVertices.empty()) {
                    polylineEnt.polylineVertices.back().bulge = numVal;
                }
            } else if (curEnt.type == DxfEntityType::ELLIPSE) {
                if (code == 10) curEnt.center.x = numVal;
                else if (code == 20) curEnt.center.y = numVal;
                else if (code == 11) curEnt.majorAxis.x = numVal;
                else if (code == 21) curEnt.majorAxis.y = numVal;
                else if (code == 40) curEnt.ratio = numVal;
                else if (code == 41) curEnt.startParam = numVal;
                else if (code == 42) curEnt.endParam = numVal;
            } else if (curEnt.type == DxfEntityType::SPLINE) {
                if (code == 10 || code == 11) {
                    curEnt.points.push_back(DxfPoint(numVal, 0.0));
                } else if ((code == 20 || code == 21) && !curEnt.points.empty()) {
                    curEnt.points.back().y = numVal;
                }
            } else if (curEnt.type == DxfEntityType::INSERT) {
                if (code == 10) curEnt.center.x = numVal;
                else if (code == 20) curEnt.center.y = numVal;
                else if (code == 41) curEnt.scaleX = numVal;
                else if (code == 42) curEnt.scaleY = numVal;
                else if (code == 50) curEnt.rotationAngle = numVal;
            } else if (curEnt.type == DxfEntityType::TEXT) {
                if (code == 10) curEnt.points[0].x = numVal;
                else if (code == 20) curEnt.points[0].y = numVal;
                else if (code == 40) curEnt.textHeight = numVal;
            } else if (curEnt.type == DxfEntityType::SOLID) {
                if (code == 10) curEnt.points[0].x = numVal;
                else if (code == 20) curEnt.points[0].y = numVal;
                else if (code == 11) curEnt.points[1].x = numVal;
                else if (code == 21) curEnt.points[1].y = numVal;
                else if (code == 12) curEnt.points[2].x = numVal;
                else if (code == 22) curEnt.points[2].y = numVal;
                else if (code == 13) curEnt.points[3].x = numVal;
                else if (code == 23) curEnt.points[3].y = numVal;
            } else if (curEnt.type == DxfEntityType::POINT) {
                if (code == 10) curEnt.points[0].x = numVal;
                else if (code == 20) curEnt.points[0].y = numVal;
            }
        }
    }

    finishLayer();
    finishEntity();

    flattenBlockInsertions();

    for (const auto& ent : entities) {
        if (!ent.layerName.empty() && findLayer(ent.layerName) == nullptr) {
            DxfLayer newLayer;
            newLayer.name = ent.layerName;
            newLayer.colorIndex = 7;
            newLayer.visible = true;
            layers.push_back(newLayer);
        }
    }

    calculateBoundingBox();
    return !entities.empty();
}

double DxfModel::pointToSegmentDistance(double px, double py, double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double l2 = dx * dx + dy * dy;
    if (l2 < 1e-12) {
        double rx = px - x1;
        double ry = py - y1;
        return std::sqrt(rx * rx + ry * ry);
    }
    double t = std::max(0.0, std::min(1.0, ((px - x1) * dx + (py - y1) * dy) / l2));
    double projX = x1 + t * dx;
    double projY = y1 + t * dy;
    double rx = px - projX;
    double ry = py - projY;
    return std::sqrt(rx * rx + ry * ry);
}

double DxfModel::pointToEntityDistance(double px, double py, const DxfEntity& entity) {
    if (entity.type == DxfEntityType::LINE && entity.points.size() >= 2) {
        return pointToSegmentDistance(px, py, entity.points[0].x, entity.points[0].y, entity.points[1].x, entity.points[1].y);
    }
    if (entity.type == DxfEntityType::POLYLINE || entity.type == DxfEntityType::SPLINE || entity.type == DxfEntityType::SOLID) {
        if (entity.points.empty()) return 1e9;
        double minDist = 1e9;
        size_t count = entity.points.size();
        size_t numSegs = entity.isClosed ? count : (count - 1);
        for (size_t i = 0; i < numSegs; ++i) {
            const auto& p1 = entity.points[i];
            const auto& p2 = entity.points[(i + 1) % count];
            minDist = std::min(minDist, pointToSegmentDistance(px, py, p1.x, p1.y, p2.x, p2.y));
        }
        return minDist;
    }
    if (entity.type == DxfEntityType::CIRCLE) {
        double distC = std::sqrt((px - entity.center.x) * (px - entity.center.x) + (py - entity.center.y) * (py - entity.center.y));
        return std::abs(distC - entity.radius);
    }
    if (entity.type == DxfEntityType::ARC) {
        double distC = std::sqrt((px - entity.center.x) * (px - entity.center.x) + (py - entity.center.y) * (py - entity.center.y));
        double angleDeg = std::atan2(py - entity.center.y, px - entity.center.x) * 180.0 / M_PI;
        if (angleDeg < 0) angleDeg += 360.0;

        double sDeg = entity.startAngle;
        double eDeg = entity.endAngle;
        while (sDeg < 0) sDeg += 360.0;
        while (eDeg < 0) eDeg += 360.0;
        while (sDeg >= 360.0) sDeg -= 360.0;
        while (eDeg >= 360.0) eDeg -= 360.0;

        bool insideArc = false;
        if (sDeg <= eDeg) {
            insideArc = (angleDeg >= sDeg && angleDeg <= eDeg);
        } else {
            insideArc = (angleDeg >= sDeg || angleDeg <= eDeg);
        }

        if (insideArc) {
            return std::abs(distC - entity.radius);
        } else {
            double sRad = entity.startAngle * M_PI / 180.0;
            double eRad = entity.endAngle * M_PI / 180.0;
            double ax1 = entity.center.x + entity.radius * std::cos(sRad);
            double ay1 = entity.center.y + entity.radius * std::sin(sRad);
            double ax2 = entity.center.x + entity.radius * std::cos(eRad);
            double ay2 = entity.center.y + entity.radius * std::sin(eRad);
            double d1 = std::hypot(px - ax1, py - ay1);
            double d2 = std::hypot(px - ax2, py - ay2);
            return std::min(d1, d2);
        }
    }
    if (entity.type == DxfEntityType::ELLIPSE) {
        int steps = 36;
        double startP = entity.startParam;
        double endP = entity.endParam;
        if (endP <= startP) endP += 2.0 * M_PI;
        double minDist = 1e9;
        DxfPoint prevPt;
        for (int i = 0; i <= steps; ++i) {
            double t = startP + (endP - startP) * i / steps;
            double ex = entity.center.x + entity.majorAxis.x * std::cos(t) - entity.majorAxis.y * entity.ratio * std::sin(t);
            double ey = entity.center.y + entity.majorAxis.y * std::cos(t) + entity.majorAxis.x * entity.ratio * std::sin(t);
            DxfPoint pt(ex, ey);
            if (i > 0) {
                minDist = std::min(minDist, pointToSegmentDistance(px, py, prevPt.x, prevPt.y, pt.x, pt.y));
            }
            prevPt = pt;
        }
        return minDist;
    }
    if ((entity.type == DxfEntityType::POINT || entity.type == DxfEntityType::TEXT) && !entity.points.empty()) {
        return std::hypot(px - entity.points[0].x, py - entity.points[0].y);
    }
    return 1e9;
}

int DxfModel::hitTest(double x, double y, double tolerance) const {
    int bestIdx = -1;
    double minDistance = tolerance;

    for (size_t i = 0; i < entities.size(); ++i) {
        const auto& ent = entities[i];
        const DxfLayer* layer = findLayer(ent.layerName);
        if (layer && !layer->visible) continue;

        double d = pointToEntityDistance(x, y, ent);
        if (d <= minDistance) {
            minDistance = d;
            bestIdx = (int)i;
        }
    }
    return bestIdx;
}

std::vector<DxfPoint> DxfModel::getOpenEndpoints(const DxfEntity& entity, bool closeCoincidingPolylines, double tolerance) {
    std::vector<DxfPoint> pts;

    if (entity.type == DxfEntityType::LINE && entity.points.size() >= 2) {
        pts.push_back(entity.points.front());
        pts.push_back(entity.points.back());
    }
    else if (entity.type == DxfEntityType::ARC) {
        double sRad = entity.startAngle * M_PI / 180.0;
        double eRad = entity.endAngle * M_PI / 180.0;
        pts.push_back(DxfPoint(entity.center.x + entity.radius * std::cos(sRad),
                               entity.center.y + entity.radius * std::sin(sRad)));
        pts.push_back(DxfPoint(entity.center.x + entity.radius * std::cos(eRad),
                               entity.center.y + entity.radius * std::sin(eRad)));
    }
    else if ((entity.type == DxfEntityType::POLYLINE || entity.type == DxfEntityType::SPLINE) && !entity.isClosed && entity.points.size() >= 2) {
        if (closeCoincidingPolylines) {
            double dx = entity.points.front().x - entity.points.back().x;
            double dy = entity.points.front().y - entity.points.back().y;
            double tol = std::max(tolerance, 1e-4);
            if (dx * dx + dy * dy <= tol * tol) {
                // Start and end coincide: treat like a closed polyline (no open endpoints)
                return pts;
            }
        }
        pts.push_back(entity.points.front());
        pts.push_back(entity.points.back());
    }
    else if (entity.type == DxfEntityType::ELLIPSE) {
        double diff = std::abs(entity.endParam - entity.startParam);
        if (diff < (2.0 * M_PI - 1e-4)) {
            double t1 = entity.startParam;
            double t2 = entity.endParam;
            double x1 = entity.center.x + entity.majorAxis.x * std::cos(t1) - entity.majorAxis.y * entity.ratio * std::sin(t1);
            double y1 = entity.center.y + entity.majorAxis.y * std::cos(t1) + entity.majorAxis.x * entity.ratio * std::sin(t1);
            double x2 = entity.center.x + entity.majorAxis.x * std::cos(t2) - entity.majorAxis.y * entity.ratio * std::sin(t2);
            double y2 = entity.center.y + entity.majorAxis.y * std::cos(t2) + entity.majorAxis.x * entity.ratio * std::sin(t2);
            pts.push_back(DxfPoint(x1, y1));
            pts.push_back(DxfPoint(x2, y2));
        }
    }
    // CIRCLE and closed POLYLINE/SPLINE/ELLIPSE have no open endpoints -> return empty pts

    return pts;
}

std::set<size_t> DxfModel::findConnectedEntities(size_t startIndex, double tolerance, bool closeCoincidingPolylines) const {
    std::set<size_t> connected;
    if (startIndex >= entities.size()) return connected;

    const std::string& startLayer = entities[startIndex].layerName;

    // Filter visible entity indices belonging to the SAME layer
    std::vector<size_t> sameLayerIndices;
    for (size_t i = 0; i < entities.size(); ++i) {
        const DxfLayer* layer = findLayer(entities[i].layerName);
        if ((!layer || layer->visible) && iequals(entities[i].layerName, startLayer)) {
            sameLayerIndices.push_back(i);
        }
    }

    // Extract open endpoints for each same-layer entity
    std::vector<std::vector<DxfPoint>> endPointsMap(entities.size());
    for (size_t idx : sameLayerIndices) {
        endPointsMap[idx] = getOpenEndpoints(entities[idx], closeCoincidingPolylines, tolerance);
    }

    // Build adjacency graph between entities whose open endpoints coincide
    std::vector<std::vector<size_t>> adj(entities.size());
    double tol2 = tolerance * tolerance;

    for (size_t i = 0; i < sameLayerIndices.size(); ++i) {
        size_t idx1 = sameLayerIndices[i];
        if (endPointsMap[idx1].empty()) continue;

        for (size_t j = i + 1; j < sameLayerIndices.size(); ++j) {
            size_t idx2 = sameLayerIndices[j];
            if (endPointsMap[idx2].empty()) continue;

            bool isConn = false;
            for (const auto& ep1 : endPointsMap[idx1]) {
                for (const auto& ep2 : endPointsMap[idx2]) {
                    double dx = ep1.x - ep2.x;
                    double dy = ep1.y - ep2.y;
                    if (dx * dx + dy * dy <= tol2) {
                        isConn = true;
                        break;
                    }
                }
                if (isConn) break;
            }

            if (isConn) {
                adj[idx1].push_back(idx2);
                adj[idx2].push_back(idx1);
            }
        }
    }

    // BFS traversal from startIndex
    std::queue<size_t> q;
    q.push(startIndex);
    connected.insert(startIndex);

    while (!q.empty()) {
        size_t curr = q.front();
        q.pop();

        for (size_t neighbor : adj[curr]) {
            if (connected.find(neighbor) == connected.end()) {
                connected.insert(neighbor);
                q.push(neighbor);
            }
        }
    }

    return connected;
}

void DxfModel::calculateBoundingBox() {
    bbox = DxfBoundingBox();
    for (const auto& entity : entities) {
        const DxfLayer* layer = findLayer(entity.layerName);
        if (layer && !layer->visible) {
            continue;
        }

        if (entity.type == DxfEntityType::LINE || entity.type == DxfEntityType::POLYLINE || entity.type == DxfEntityType::SPLINE || entity.type == DxfEntityType::SOLID) {
            for (const auto& pt : entity.points) {
                bbox.update(pt.x, pt.y);
            }
        } else if (entity.type == DxfEntityType::CIRCLE) {
            bbox.update(entity.center.x - entity.radius, entity.center.y - entity.radius);
            bbox.update(entity.center.x + entity.radius, entity.center.y + entity.radius);
        } else if (entity.type == DxfEntityType::ARC) {
            double startRad = entity.startAngle * M_PI / 180.0;
            double endRad = entity.endAngle * M_PI / 180.0;
            if (endRad <= startRad) endRad += 2.0 * M_PI;

            int steps = 16;
            for (int i = 0; i <= steps; ++i) {
                double t = startRad + (endRad - startRad) * i / steps;
                double ax = entity.center.x + entity.radius * std::cos(t);
                double ay = entity.center.y + entity.radius * std::sin(t);
                bbox.update(ax, ay);
            }
        } else if (entity.type == DxfEntityType::ELLIPSE) {
            int steps = 36;
            double startP = entity.startParam;
            double endP = entity.endParam;
            if (endP <= startP) endP += 2.0 * M_PI;

            for (int i = 0; i <= steps; ++i) {
                double t = startP + (endP - startP) * i / steps;
                double ex = entity.center.x + entity.majorAxis.x * std::cos(t) - entity.majorAxis.y * entity.ratio * std::sin(t);
                double ey = entity.center.y + entity.majorAxis.y * std::cos(t) + entity.majorAxis.x * entity.ratio * std::sin(t);
                bbox.update(ex, ey);
            }
        } else if ((entity.type == DxfEntityType::POINT || entity.type == DxfEntityType::TEXT) && !entity.points.empty()) {
            bbox.update(entity.points[0].x, entity.points[0].y);
        }
    }
}
