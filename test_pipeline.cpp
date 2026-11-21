#include "AnalyticalToolpath.h"
#include "DxfParser.h"
#include <iostream>
#include <cassert>
#include <cmath>

void testRoundedLarge() {
    std::cout << "[Test 1] Testing rounded-large.bcam..." << std::endl;
    DxfModel model;
    
    DxfLayer layer;
    layer.name = "0";
    layer.visible = true;
    model.layers.push_back(layer);

    // Shape 1 (Base platform 0..30, 0..30, depth=5)
    DxfEntity e0; e0.type = DxfEntityType::LINE; e0.layerName = "0"; e0.points = { {0, 0}, {30, 0} }; model.entities.push_back(e0);
    DxfEntity e1; e1.type = DxfEntityType::LINE; e1.layerName = "0"; e1.points = { {30, 0}, {30, 30} }; model.entities.push_back(e1);
    DxfEntity e2; e2.type = DxfEntityType::LINE; e2.layerName = "0"; e2.points = { {30, 30}, {0, 30} }; model.entities.push_back(e2);
    DxfEntity e3; e3.type = DxfEntityType::LINE; e3.layerName = "0"; e3.points = { {0, 30}, {0, 0} }; model.entities.push_back(e3);

    // Shape 2 (Standing tower 10..20, 10..20, depth=10)
    DxfEntity e4; e4.type = DxfEntityType::LINE; e4.layerName = "0"; e4.points = { {10, 10}, {20, 10} }; model.entities.push_back(e4);
    DxfEntity e5; e5.type = DxfEntityType::LINE; e5.layerName = "0"; e5.points = { {20, 10}, {20, 20} }; model.entities.push_back(e5);
    DxfEntity e6; e6.type = DxfEntityType::LINE; e6.layerName = "0"; e6.points = { {20, 20}, {10, 20} }; model.entities.push_back(e6);
    DxfEntity e7; e7.type = DxfEntityType::LINE; e7.layerName = "0"; e7.points = { {10, 20}, {10, 10} }; model.entities.push_back(e7);

    CamShape s1; s1.shapeId = 1; s1.depth = 5.0; s1.entityIndices = {0, 1, 2, 3};
    CamShape s2; s2.shapeId = 2; s2.depth = 10.0; s2.entityIndices = {4, 5, 6, 7};
    std::vector<CamShape> shapes = {s1, s2};

    double topHeight = 10.0;
    double toolDia = 2.0;
    double stepover = 70.0;

    auto valRes = validatePartHierarchy(model, shapes, topHeight);
    assert(!valRes.hasViolation);
    std::cout << "  -> Hierarchy Validation: Passed (no false violation)" << std::endl;

    auto bands = computePartHeightBands(shapes, topHeight);
    assert(bands.size() == 1);
    assert(std::abs(bands[0].zUpper - 10.0) < 1e-4);
    assert(std::abs(bands[0].zFloor - 5.0) < 1e-4);
    std::cout << "  -> Height Bands: Passed (Z upper 10.0 -> floor 5.0)" << std::endl;

    auto clearingLoops = generateBandClearingLoops(model, shapes, 10.0, 5.0, topHeight, toolDia, stepover);
    assert(!clearingLoops.empty());
    
    std::vector<size_t> s2Indices = {4, 5, 6, 7};
    auto s2Paths = getShapeAnalyticalPaths(model, s2Indices);
    assert(!s2Paths.empty());

    for (const auto& loop : clearingLoops) {
        auto samplePts = sampleAnalyticalPath(loop, 0.5);
        for (const auto& pt : samplePts) {
            double dist = pointToAnalyticalPathDistance(pt, s2Paths[0]);
            assert(dist >= 1.0 - 1e-2);
        }
    }
    std::cout << "  -> Clearing Loops Zero-Gouging Clearance: Passed (all points >= 1.0mm from Shape 2)" << std::endl;

    auto cutoutPaths = generateOuterCutoutPaths(model, shapes, toolDia);
    assert(!cutoutPaths.empty());
    std::cout << "  -> Cutout Paths: Passed" << std::endl;
}

void testRecessedPocket() {
    std::cout << "\n[Test 2] Testing Recessed Pocket Case..." << std::endl;
    DxfModel model;
    DxfLayer layer; layer.name = "0"; layer.visible = true; model.layers.push_back(layer);

    // Shape 1 (Outer block 0..100, 0..100, depth=10)
    DxfEntity e0; e0.type = DxfEntityType::LINE; e0.layerName = "0"; e0.points = { {0, 0}, {100, 0} }; model.entities.push_back(e0);
    DxfEntity e1; e1.type = DxfEntityType::LINE; e1.layerName = "0"; e1.points = { {100, 0}, {100, 100} }; model.entities.push_back(e1);
    DxfEntity e2; e2.type = DxfEntityType::LINE; e2.layerName = "0"; e2.points = { {100, 100}, {0, 100} }; model.entities.push_back(e2);
    DxfEntity e3; e3.type = DxfEntityType::LINE; e3.layerName = "0"; e3.points = { {0, 100}, {0, 0} }; model.entities.push_back(e3);

    // Shape 2 (Inner pocket 20..80, 20..80, depth=4)
    DxfEntity e4; e4.type = DxfEntityType::LINE; e4.layerName = "0"; e4.points = { {20, 20}, {80, 20} }; model.entities.push_back(e4);
    DxfEntity e5; e5.type = DxfEntityType::LINE; e5.layerName = "0"; e5.points = { {80, 20}, {80, 80} }; model.entities.push_back(e5);
    DxfEntity e6; e6.type = DxfEntityType::LINE; e6.layerName = "0"; e6.points = { {80, 80}, {20, 80} }; model.entities.push_back(e6);
    DxfEntity e7; e7.type = DxfEntityType::LINE; e7.layerName = "0"; e7.points = { {20, 80}, {20, 20} }; model.entities.push_back(e7);

    CamShape s1; s1.shapeId = 1; s1.depth = 10.0; s1.entityIndices = {0, 1, 2, 3};
    CamShape s2; s2.shapeId = 2; s2.depth = 4.0; s2.entityIndices = {4, 5, 6, 7};
    std::vector<CamShape> shapes = {s1, s2};

    double topHeight = 10.0;
    double toolDia = 2.0;
    double stepover = 70.0;

    auto valRes = validatePartHierarchy(model, shapes, topHeight);
    assert(!valRes.hasViolation);

    auto bands = computePartHeightBands(shapes, topHeight);
    assert(bands.size() == 1);
    assert(std::abs(bands[0].zUpper - 10.0) < 1e-4);
    assert(std::abs(bands[0].zFloor - 4.0) < 1e-4);

    auto clearingLoops = generateBandClearingLoops(model, shapes, 10.0, 4.0, topHeight, toolDia, stepover);
    assert(!clearingLoops.empty());

    for (const auto& loop : clearingLoops) {
        auto samplePts = sampleAnalyticalPath(loop, 0.5);
        for (const auto& pt : samplePts) {
            assert(pt.x >= 20.0 + 1.0 - 1e-2 && pt.x <= 80.0 - 1.0 + 1e-2);
            assert(pt.y >= 20.0 + 1.0 - 1e-2 && pt.y <= 80.0 - 1.0 + 1e-2);
        }
    }
    std::cout << "  -> Recessed Pocket Containment & No-Overrun: Passed (all toolpaths strictly inside pocket)" << std::endl;
}

void testMultiBossMultiStep() {
    std::cout << "\n[Test 3] Testing Multi-Boss & Multi-Step Case..." << std::endl;
    DxfModel model;
    DxfLayer layer; layer.name = "0"; layer.visible = true; model.layers.push_back(layer);

    // Base 0..100, 0..100, depth=3
    DxfEntity e0; e0.type = DxfEntityType::LINE; e0.layerName = "0"; e0.points = { {0, 0}, {100, 0} }; model.entities.push_back(e0);
    DxfEntity e1; e1.type = DxfEntityType::LINE; e1.layerName = "0"; e1.points = { {100, 0}, {100, 100} }; model.entities.push_back(e1);
    DxfEntity e2; e2.type = DxfEntityType::LINE; e2.layerName = "0"; e2.points = { {100, 100}, {0, 100} }; model.entities.push_back(e2);
    DxfEntity e3; e3.type = DxfEntityType::LINE; e3.layerName = "0"; e3.points = { {0, 100}, {0, 0} }; model.entities.push_back(e3);

    // Shelf 10..90, 10..90, depth=6
    DxfEntity e4; e4.type = DxfEntityType::LINE; e4.layerName = "0"; e4.points = { {10, 10}, {90, 10} }; model.entities.push_back(e4);
    DxfEntity e5; e5.type = DxfEntityType::LINE; e5.layerName = "0"; e5.points = { {90, 10}, {90, 90} }; model.entities.push_back(e5);
    DxfEntity e6; e6.type = DxfEntityType::LINE; e6.layerName = "0"; e6.points = { {90, 90}, {10, 90} }; model.entities.push_back(e6);
    DxfEntity e7; e7.type = DxfEntityType::LINE; e7.layerName = "0"; e7.points = { {10, 90}, {10, 10} }; model.entities.push_back(e7);

    // Boss 1 20..40, 20..40, depth=10
    DxfEntity e8; e8.type = DxfEntityType::LINE; e8.layerName = "0"; e8.points = { {20, 20}, {40, 20} }; model.entities.push_back(e8);
    DxfEntity e9; e9.type = DxfEntityType::LINE; e9.layerName = "0"; e9.points = { {40, 20}, {40, 40} }; model.entities.push_back(e9);
    DxfEntity e10; e10.type = DxfEntityType::LINE; e10.layerName = "0"; e10.points = { {40, 40}, {20, 40} }; model.entities.push_back(e10);
    DxfEntity e11; e11.type = DxfEntityType::LINE; e11.layerName = "0"; e11.points = { {20, 40}, {20, 20} }; model.entities.push_back(e11);

    // Boss 2 60..80, 60..80, depth=10
    DxfEntity e12; e12.type = DxfEntityType::LINE; e12.layerName = "0"; e12.points = { {60, 60}, {80, 60} }; model.entities.push_back(e12);
    DxfEntity e13; e13.type = DxfEntityType::LINE; e13.layerName = "0"; e13.points = { {80, 60}, {80, 80} }; model.entities.push_back(e13);
    DxfEntity e14; e14.type = DxfEntityType::LINE; e14.layerName = "0"; e14.points = { {80, 80}, {60, 80} }; model.entities.push_back(e14);
    DxfEntity e15; e15.type = DxfEntityType::LINE; e15.layerName = "0"; e15.points = { {60, 80}, {60, 60} }; model.entities.push_back(e15);

    CamShape sBase; sBase.shapeId = 1; sBase.depth = 3.0; sBase.entityIndices = {0, 1, 2, 3};
    CamShape sShelf; sShelf.shapeId = 2; sShelf.depth = 6.0; sShelf.entityIndices = {4, 5, 6, 7};
    CamShape sBoss1; sBoss1.shapeId = 3; sBoss1.depth = 10.0; sBoss1.entityIndices = {8, 9, 10, 11};
    CamShape sBoss2; sBoss2.shapeId = 4; sBoss2.depth = 10.0; sBoss2.entityIndices = {12, 13, 14, 15};

    std::vector<CamShape> shapes = {sBase, sShelf, sBoss1, sBoss2};
    double topHeight = 10.0, toolDia = 2.0, stepover = 70.0;

    auto bands = computePartHeightBands(shapes, topHeight);
    assert(bands.size() == 2);
    assert(std::abs(bands[0].zUpper - 10.0) < 1e-4 && std::abs(bands[0].zFloor - 6.0) < 1e-4);
    assert(std::abs(bands[1].zUpper - 6.0) < 1e-4 && std::abs(bands[1].zFloor - 3.0) < 1e-4);

    auto loopsBand0 = generateBandClearingLoops(model, shapes, 10.0, 6.0, topHeight, toolDia, stepover);
    assert(!loopsBand0.empty());

    auto loopsBand1 = generateBandClearingLoops(model, shapes, 6.0, 3.0, topHeight, toolDia, stepover);
    assert(!loopsBand1.empty());

    std::cout << "  -> Multi-Boss Multi-Step Height Bands & Loops: Passed" << std::endl;
}

void testHierarchyViolation() {
    std::cout << "\n[Test 4] Testing Hierarchy Violation Case..." << std::endl;
    DxfModel model;
    DxfLayer layer; layer.name = "0"; layer.visible = true; model.layers.push_back(layer);

    // Base shape 0..30, 0..30, depth=5
    DxfEntity e0; e0.type = DxfEntityType::LINE; e0.layerName = "0"; e0.points = { {0, 0}, {30, 0} }; model.entities.push_back(e0);
    DxfEntity e1; e1.type = DxfEntityType::LINE; e1.layerName = "0"; e1.points = { {30, 0}, {30, 30} }; model.entities.push_back(e1);
    DxfEntity e2; e2.type = DxfEntityType::LINE; e2.layerName = "0"; e2.points = { {30, 30}, {0, 30} }; model.entities.push_back(e2);
    DxfEntity e3; e3.type = DxfEntityType::LINE; e3.layerName = "0"; e3.points = { {0, 30}, {0, 0} }; model.entities.push_back(e3);

    // Taller shape extending OUTSIDE base shape (20..50, 20..50, depth=10)
    DxfEntity e4; e4.type = DxfEntityType::LINE; e4.layerName = "0"; e4.points = { {20, 20}, {50, 20} }; model.entities.push_back(e4);
    DxfEntity e5; e5.type = DxfEntityType::LINE; e5.layerName = "0"; e5.points = { {50, 20}, {50, 50} }; model.entities.push_back(e5);
    DxfEntity e6; e6.type = DxfEntityType::LINE; e6.layerName = "0"; e6.points = { {50, 50}, {20, 50} }; model.entities.push_back(e6);
    DxfEntity e7; e7.type = DxfEntityType::LINE; e7.layerName = "0"; e7.points = { {20, 50}, {20, 20} }; model.entities.push_back(e7);

    CamShape s1; s1.shapeId = 1; s1.depth = 5.0; s1.entityIndices = {0, 1, 2, 3};
    CamShape s2; s2.shapeId = 2; s2.depth = 10.0; s2.entityIndices = {4, 5, 6, 7};
    std::vector<CamShape> shapes = {s1, s2};

    double topHeight = 10.0;

    auto valRes = validatePartHierarchy(model, shapes, topHeight);
    assert(valRes.hasViolation);
    assert(!valRes.warnings.empty());
    std::cout << "  -> Hierarchy Violation Warning Emission: Passed (" << valRes.warnings[0] << ")" << std::endl;
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "  BendiCAM Part Geometry Pipeline Tests  " << std::endl;
    std::cout << "=========================================" << std::endl;

    testRoundedLarge();
    testRecessedPocket();
    testMultiBossMultiStep();
    testHierarchyViolation();

    std::cout << "\nALL 4 STRESS TEST SCENARIOS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}