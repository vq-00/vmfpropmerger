#pragma once

#include "vmfpropmerger/utils.h"

namespace vmfpropmerger {

struct SMDVertex {
    int bone = 0;
    Vec3 position;
    Vec3 normal;
    double u = 0.0;
    double v = 0.0;
};

struct SMDTriangle {
    std::string material;
    SMDVertex a;
    SMDVertex b;
    SMDVertex c;
};

struct SMDMesh {
    std::vector<SMDTriangle> triangles;
};

struct CachedModel {
    SMDMesh renderMesh;
    SMDMesh collisionMesh;
    std::vector<std::string> materials;
    bool hasCollision = false;
};

bool loadReferenceSMD(const fs::path& path, SMDMesh& mesh);
void writeSMDVertex(std::ostream& out, const SMDVertex& vertex);
bool writeMergedSMD(const fs::path& path, const std::vector<SMDMesh>& meshes);

std::vector<std::string> extractCDMaterials(const fs::path& qc);
std::vector<std::string> extractQCBodyFiles(const fs::path& qc);
std::vector<std::string> extractQCCollisionFiles(const fs::path& qc);

fs::path findSMDByRelativeName(const fs::path& root, const std::string& relativeName);
std::vector<fs::path> findReferenceSMDs(const fs::path& decompiled, const fs::path& mdlPath);
fs::path findCollisionSMD(const fs::path& decompiled, const fs::path& mdlPath);

} // namespace vmfpropmerger