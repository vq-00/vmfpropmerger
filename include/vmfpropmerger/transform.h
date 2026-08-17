#pragma once

#include "vmfpropmerger/utils.h"

namespace vmfpropmerger {

// Forward declarations (now inside namespace)
struct Options;
struct PropDynamic;
struct SMDMesh;

struct RotationMatrix {
    double m[3][3] = {};
};

RotationMatrix sourceAngleMatrix(const Vec3& angles);
Vec3 rotateVectorSource(const Vec3& value, const RotationMatrix& matrix);
RotationMatrix multiplyRotationMatrices(const RotationMatrix& a, const RotationMatrix& b);

enum class AxisComponent {
    X, Y, Z, NegX, NegY, NegZ
};

struct CoordinateMap {
    AxisComponent x = AxisComponent::NegY;
    AxisComponent y = AxisComponent::X;
    AxisComponent z = AxisComponent::Z;
};

bool parseAxisComponent(std::string text, AxisComponent& output);
bool parseCoordinateMap(const std::string& text, CoordinateMap& map);
double getAxisValue(const Vec3& value, AxisComponent component);
Vec3 applyCoordinateMap(const Vec3& value, const CoordinateMap& map);
RotationMatrix coordinateMapMatrix(const CoordinateMap& map);
Vec3 transformByMatrix(const RotationMatrix& matrix, const Vec3& value);

Vec3 transformPosition(const Vec3& local, const PropDynamic& prop,
                       const Options& options, const CoordinateMap& coordinateMap);
Vec3 transformNormal(const Vec3& local, const PropDynamic& prop,
                     const Options& options, const CoordinateMap& coordinateMap);
void transformCollisionMesh(SMDMesh& mesh, const PropDynamic& prop,
                            const Options& options, const CoordinateMap& coordinateMap);

} // namespace vmfpropmerger