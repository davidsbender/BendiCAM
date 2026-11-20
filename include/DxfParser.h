#ifndef DXFPARSER_H
#define DXFPARSER_H

#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <queue>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class DxfEntityType {
    LINE,
    CIRCLE,
    ARC,
    POLYLINE,
    ELLIPSE,
    POINT,
    SPLINE,
    INSERT,
    TEXT,
    SOLID
};

struct DxfColor {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

struct DxfLayer {
    std::string name = "0";
    int colorIndex = 7;
    uint32_t rgbColor = 0;
    bool hasTrueColor = false;
    bool visible = true;
};

struct DxfPoint {
    double x = 0.0;
    double y = 0.0;
    DxfPoint() = default;
    DxfPoint(double _x, double _y) : x(_x), y(_y) {}
};

struct DxfVertex {
    double x = 0.0;
    double y = 0.0;
    double bulge = 0.0;
    DxfVertex() = default;
    DxfVertex(double _x, double _y, double _b = 0.0) : x(_x), y(_y), bulge(_b) {}
};

struct DxfEntity {
    DxfEntityType type = DxfEntityType::LINE;
    std::string layerName = "0";
    int colorIndex = 256; // 256 = BYLAYER
    uint32_t rgbColor = 0;
    bool hasTrueColor = false;
    std::vector<DxfPoint> points;
    std::vector<DxfVertex> polylineVertices;
    DxfPoint center;
    double radius = 0.0;
    double startAngle = 0.0;
    double endAngle = 360.0;
    bool isClosed = false;
    DxfPoint majorAxis;
    double ratio = 1.0;
    double startParam = 0.0;
    double endParam = 2.0 * M_PI;

    // For INSERT
    std::string blockName;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotationAngle = 0.0; // degrees

    // For TEXT
    std::string textString;
    double textHeight = 1.0;
};

struct DxfBlock {
    std::string name;
    DxfPoint basePoint;
    std::vector<DxfEntity> entities;
};

struct DxfBoundingBox {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    bool valid = false;

    void update(double x, double y) {
        if (!valid) {
            minX = maxX = x;
            minY = maxY = y;
            valid = true;
        } else {
            minX = std::min(minX, x);
            maxX = std::max(maxX, x);
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
};

class DxfModel {
public:
    std::vector<DxfEntity> entities;
    std::vector<DxfLayer> layers;
    std::vector<DxfBlock> blocks;
    DxfBoundingBox bbox;

    void clear() {
        entities.clear();
        layers.clear();
        blocks.clear();
        bbox = DxfBoundingBox();
    }

    bool loadFromFile(const std::string& filename);
    void calculateBoundingBox();
    DxfColor getEntityColor(const DxfEntity& entity) const;
    DxfLayer* findLayer(const std::string& name);
    const DxfLayer* findLayer(const std::string& name) const;
    DxfBlock* findBlock(const std::string& name);
    const DxfBlock* findBlock(const std::string& name) const;
    static DxfColor aciToRgb(int colorIndex);

    static std::vector<DxfPoint> tessellatePolyline(const std::vector<DxfVertex>& vertices, bool isClosed);
    static std::vector<DxfPoint> sampleArc(const DxfPoint& center, double radius, double startAngleDeg, double endAngleDeg, int numSteps = 16);
    static std::vector<DxfPoint> sampleEllipse(const DxfPoint& center, const DxfPoint& majorAxis, double ratio, double startParam, double endParam, int numSteps = 36);

    void flattenBlockInsertions();

    int hitTest(double x, double y, double tolerance) const;
    std::set<size_t> findConnectedEntities(size_t startIndex, double tolerance, bool closeCoincidingPolylines = false) const;
    static double pointToSegmentDistance(double px, double py, double x1, double y1, double x2, double y2);
    static double pointToEntityDistance(double px, double py, const DxfEntity& entity);
    static std::vector<DxfPoint> getOpenEndpoints(const DxfEntity& entity, bool closeCoincidingPolylines = false, double tolerance = 1e-4);
};

#endif // DXFPARSER_H
