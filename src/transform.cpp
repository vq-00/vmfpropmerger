#include "vmfpropmerger/transform.h"
#include "vmfpropmerger/options.h"    // for Options
#include "vmfpropmerger/vmf.h"        // for PropDynamic
#include "vmfpropmerger/smd.h"        // for SMDMesh

namespace vmfpropmerger {

RotationMatrix sourceAngleMatrix(const Vec3& angles) {
    const double yaw = degToRad(angles.y);
    const double pitch = degToRad(angles.x);
    const double roll = degToRad(angles.z);
    const double sy = std::sin(yaw), cy = std::cos(yaw);
    const double sp = std::sin(pitch), cp = std::cos(pitch);
    const double sr = std::sin(roll), cr = std::cos(roll);
    RotationMatrix matrix{};
    matrix.m[0][0] = cp * cy;
    matrix.m[0][1] = sr * sp * cy - cr * sy;
    matrix.m[0][2] = cr * sp * cy + sr * sy;
    matrix.m[1][0] = cp * sy;
    matrix.m[1][1] = sr * sp * sy + cr * cy;
    matrix.m[1][2] = cr * sp * sy - sr * cy;
    matrix.m[2][0] = -sp;
    matrix.m[2][1] = sr * cp;
    matrix.m[2][2] = cr * cp;
    return matrix;
}

Vec3 rotateVectorSource(const Vec3& value, const RotationMatrix& matrix) {
    Vec3 result;
    result.x = matrix.m[0][0] * value.x + matrix.m[0][1] * value.y + matrix.m[0][2] * value.z;
    result.y = matrix.m[1][0] * value.x + matrix.m[1][1] * value.y + matrix.m[1][2] * value.z;
    result.z = matrix.m[2][0] * value.x + matrix.m[2][1] * value.y + matrix.m[2][2] * value.z;
    return result;
}

RotationMatrix multiplyRotationMatrices(const RotationMatrix& a, const RotationMatrix& b) {
    RotationMatrix result{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            result.m[row][col] = a.m[row][0] * b.m[0][col] +
                                 a.m[row][1] * b.m[1][col] +
                                 a.m[row][2] * b.m[2][col];
        }
    }
    return result;
}

bool parseAxisComponent(std::string text, AxisComponent& output) {
    text = lowerString(trim(text));
    if (text == "x") { output = AxisComponent::X; return true; }
    if (text == "y") { output = AxisComponent::Y; return true; }
    if (text == "z") { output = AxisComponent::Z; return true; }
    if (text == "-x") { output = AxisComponent::NegX; return true; }
    if (text == "-y") { output = AxisComponent::NegY; return true; }
    if (text == "-z") { output = AxisComponent::NegZ; return true; }
    return false;
}

bool parseCoordinateMap(const std::string& text, CoordinateMap& map) {
    std::vector<std::string> components;
    std::string current;
    for (char c : text) {
        if (c == ',') { components.push_back(trim(current)); current.clear(); }
        else current += c;
    }
    components.push_back(trim(current));
    if (components.size() != 3) return false;
    return parseAxisComponent(components[0], map.x) &&
           parseAxisComponent(components[1], map.y) &&
           parseAxisComponent(components[2], map.z);
}

double getAxisValue(const Vec3& value, AxisComponent component) {
    switch (component) {
        case AxisComponent::X: return value.x;
        case AxisComponent::Y: return value.y;
        case AxisComponent::Z: return value.z;
        case AxisComponent::NegX: return -value.x;
        case AxisComponent::NegY: return -value.y;
        case AxisComponent::NegZ: return -value.z;
    }
    return 0.0;
}

Vec3 applyCoordinateMap(const Vec3& value, const CoordinateMap& map) {
    return {getAxisValue(value, map.x), getAxisValue(value, map.y), getAxisValue(value, map.z)};
}

RotationMatrix coordinateMapMatrix(const CoordinateMap& map) {
    RotationMatrix matrix{};
    auto setRow = [&](int row, AxisComponent component) {
        switch (component) {
            case AxisComponent::X: matrix.m[row][0] = 1.0; break;
            case AxisComponent::Y: matrix.m[row][1] = 1.0; break;
            case AxisComponent::Z: matrix.m[row][2] = 1.0; break;
            case AxisComponent::NegX: matrix.m[row][0] = -1.0; break;
            case AxisComponent::NegY: matrix.m[row][1] = -1.0; break;
            case AxisComponent::NegZ: matrix.m[row][2] = -1.0; break;
        }
    };
    setRow(0, map.x);
    setRow(1, map.y);
    setRow(2, map.z);
    return matrix;
}

Vec3 transformByMatrix(const RotationMatrix& matrix, const Vec3& value) {
    return rotateVectorSource(value, matrix);
}

Vec3 transformPosition(const Vec3& local, const PropDynamic& prop,
                       const Options& options, const CoordinateMap& coordinateMap) {
    const double sx = prop.modelScale * prop.modelScaleAxis.x * options.globalScale;
    const double sy = prop.modelScale * prop.modelScaleAxis.y * options.globalScale;
    const double sz = prop.modelScale * prop.modelScaleAxis.z * options.globalScale;
    Vec3 localSource = {local.x * sx, local.y * sy, local.z * sz};
    Vec3 localWorld = applyCoordinateMap(localSource, coordinateMap);
    if (options.applyRotation) {
        localWorld = rotateVectorSource(localWorld, prop.worldRotation);
    }
    return localWorld + prop.worldOrigin + options.globalOffset;
}

Vec3 transformNormal(const Vec3& local, const PropDynamic& prop,
                     const Options& options, const CoordinateMap& coordinateMap) {
    if (!options.transformNormals) {
        Vec3 normal = applyCoordinateMap(local, coordinateMap);
        if (options.applyRotation) normal = rotateVectorSource(normal, prop.worldRotation);
        return normalize(normal);
    }
    const double sx = prop.modelScale * prop.modelScaleAxis.x * options.globalScale;
    const double sy = prop.modelScale * prop.modelScaleAxis.y * options.globalScale;
    const double sz = prop.modelScale * prop.modelScaleAxis.z * options.globalScale;
    const double safeSx = std::abs(sx) <= 1e-12 ? 1e-12 : sx;
    const double safeSy = std::abs(sy) <= 1e-12 ? 1e-12 : sy;
    const double safeSz = std::abs(sz) <= 1e-12 ? 1e-12 : sz;
    Vec3 normal = {local.x / safeSx, local.y / safeSy, local.z / safeSz};
    normal = applyCoordinateMap(normal, coordinateMap);
    if (options.applyRotation) normal = rotateVectorSource(normal, prop.worldRotation);
    return normalize(normal);
}

void transformCollisionMesh(SMDMesh& mesh, const PropDynamic& prop,
                            const Options& options, const CoordinateMap& coordinateMap) {
    for (auto& triangle : mesh.triangles) {
        SMDVertex* vertices[] = {&triangle.a, &triangle.b, &triangle.c};
        for (SMDVertex* vertex : vertices) {
            vertex->position = transformPosition(vertex->position, prop, options, coordinateMap);
            vertex->normal = transformNormal(vertex->normal, prop, options, coordinateMap);
            vertex->bone = 0;
        }
    }
}

} // namespace vmfpropmerger