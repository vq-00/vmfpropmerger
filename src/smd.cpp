#include "vmfpropmerger/smd.h"

namespace vmfpropmerger {

bool loadReferenceSMD(const fs::path& path, SMDMesh& mesh) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::string line;
    bool inTriangles = false;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        if (trimmed == "triangles") { inTriangles = true; continue; }
        if (!inTriangles) continue;
        if (trimmed.empty()) continue;
        if (trimmed == "end") break;
        const std::string material = trimmed;
        SMDVertex vertices[3];
        bool validTriangle = true;
        for (int i = 0; i < 3; ++i) {
            if (!std::getline(input, line)) { validTriangle = false; break; }
            std::istringstream stream(line);
            if (!(stream >> vertices[i].bone
                  >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z
                  >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z
                  >> vertices[i].u >> vertices[i].v)) {
                validTriangle = false;
                break;
            }
        }
        if (validTriangle) {
            mesh.triangles.push_back({material, vertices[0], vertices[1], vertices[2]});
        }
    }
    return !mesh.triangles.empty();
}

void writeSMDVertex(std::ostream& out, const SMDVertex& vertex) {
    out << "0 "
        << std::setprecision(10)
        << vertex.position.x << " " << vertex.position.y << " " << vertex.position.z << " "
        << vertex.normal.x << " " << vertex.normal.y << " " << vertex.normal.z << " "
        << vertex.u << " " << vertex.v << "\n";
}

bool writeMergedSMD(const fs::path& path, const std::vector<SMDMesh>& meshes) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "version 1\n\n"
        << "nodes\n"
        << "0 \"root\" -1\n"
        << "end\n\n"
        << "skeleton\n"
        << "time 0\n"
        << "0 0 0 0 0 0 0\n"
        << "end\n\n"
        << "triangles\n";
    for (const auto& mesh : meshes) {
        for (const auto& triangle : mesh.triangles) {
            out << triangle.material << "\n";
            writeSMDVertex(out, triangle.a);
            writeSMDVertex(out, triangle.b);
            writeSMDVertex(out, triangle.c);
        }
    }
    out << "end\n";
    return !!out;
}

std::vector<std::string> extractCDMaterials(const fs::path& qc) {
    std::vector<std::string> result;
    std::ifstream input(qc);
    if (!input) return result;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        const std::string lower = lowerString(trimmed);
        if (lower.find("$cdmaterials") != 0) continue;
        const size_t p1 = line.find('"');
        if (p1 == std::string::npos) continue;
        const size_t p2 = line.find('"', p1 + 1);
        if (p2 == std::string::npos) continue;
        std::string material = trim(line.substr(p1 + 1, p2 - p1 - 1));
        material = normalizeSlashes(material);
        material = removeLeadingSlash(material);
        if (!material.empty() && std::find(result.begin(), result.end(), material) == result.end())
            result.push_back(material);
    }
    return result;
}

std::vector<std::string> extractQCBodyFiles(const fs::path& qc) {
    std::vector<std::string> result;
    std::ifstream input(qc);
    if (!input) return result;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        const std::string lower = lowerString(trimmed);
        if (lower.rfind("$body", 0) != 0) continue;
        std::vector<std::string> quoted;
        bool inQuote = false;
        std::string current;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            const char c = trimmed[i];
            if (c == '"') {
                if (inQuote) { quoted.push_back(current); current.clear(); inQuote = false; }
                else inQuote = true;
                continue;
            }
            if (inQuote) current += c;
        }
        if (quoted.size() >= 2) {
            std::string smd = normalizeSlashes(quoted.back());
            result.push_back(smd);
        }
    }
    return result;
}

std::vector<std::string> extractQCCollisionFiles(const fs::path& qc) {
    std::vector<std::string> result;
    std::ifstream input(qc);
    if (!input) return result;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        const std::string lower = lowerString(trimmed);
        if (lower.rfind("$collisionmodel", 0) != 0) continue;
        std::vector<std::string> quoted;
        bool inQuote = false;
        std::string current;
        for (size_t i = 0; i < trimmed.size(); ++i) {
            const char c = trimmed[i];
            if (c == '"') {
                if (inQuote) { quoted.push_back(current); current.clear(); inQuote = false; }
                else inQuote = true;
                continue;
            }
            if (inQuote) current += c;
        }
        if (!quoted.empty()) {
            std::string smd = normalizeSlashes(quoted.front());
            if (!smd.empty()) result.push_back(smd);
        }
    }
    return result;
}

fs::path findSMDByRelativeName(const fs::path& root, const std::string& relativeName) {
    const std::string wanted = lowerString(normalizeSlashes(relativeName));
    const fs::path direct = root / relativeName;
    std::error_code ec;
    if (fs::exists(direct, ec) && fs::is_regular_file(direct, ec)) return direct;
    const std::vector<fs::path> smds = findFiles(root, ".smd");
    for (const fs::path& smd : smds) {
        std::string relative = fs::relative(smd, root, ec).string();
        if (ec) continue;
        relative = lowerString(normalizeSlashes(relative));
        if (relative == wanted) return smd;
        if (lowerString(smd.filename().string()) == lowerString(fs::path(relativeName).filename().string()))
            return smd;
    }
    return {};
}

std::vector<fs::path> findReferenceSMDs(const fs::path& decompiled, const fs::path& mdlPath) {
    std::vector<fs::path> result;
    const std::vector<fs::path> qcs = findFiles(decompiled, ".qc");
    for (const fs::path& qc : qcs) {
        const std::vector<std::string> bodies = extractQCBodyFiles(qc);
        for (const std::string& body : bodies) {
            const fs::path smd = findSMDByRelativeName(decompiled, body);
            if (!smd.empty() && std::find(result.begin(), result.end(), smd) == result.end())
                result.push_back(smd);
        }
    }
    if (!result.empty()) return result;

    const std::string modelStem = lowerString(mdlPath.stem().string());
    const std::vector<fs::path> smds = findFiles(decompiled, ".smd");
    auto findExact = [&](const std::string& filename) -> fs::path {
        const std::string wanted = lowerString(filename);
        for (const fs::path& smd : smds) {
            if (lowerString(smd.filename().string()) == wanted) return smd;
        }
        return {};
    };
    fs::path reference = findExact(modelStem + "_reference.smd");
    if (!reference.empty()) { result.push_back(reference); return result; }
    reference = findExact(modelStem + "_model.smd");
    if (!reference.empty()) { result.push_back(reference); return result; }
    reference = findExact(modelStem + ".smd");
    if (!reference.empty()) { result.push_back(reference); return result; }

    std::vector<fs::path> candidates;
    for (const fs::path& smd : smds) {
        const std::string name = lowerString(smd.filename().string());
        if (name.find("_lod") != std::string::npos) continue;
        if (name.find("lod") != std::string::npos) continue;
        if (name.find("physics") != std::string::npos) continue;
        if (name.find("collision") != std::string::npos) continue;
        if (name == "idle.smd") continue;
        if (name == "ref.smd") continue;
        if (name == "bindpose.smd") continue;
        if (name.find("sequence") != std::string::npos) continue;
        if (name.find("anim") != std::string::npos) continue;
        if (name.find("pose") != std::string::npos) continue;
        if (name.find(modelStem) != std::string::npos) candidates.push_back(smd);
    }
    if (!candidates.empty()) {
        std::sort(candidates.begin(), candidates.end(),
                  [](const fs::path& a, const fs::path& b) {
                      const std::string aName = lowerString(a.filename().string());
                      const std::string bName = lowerString(b.filename().string());
                      auto score = [](const std::string& name) {
                          if (name.find("_reference.smd") != std::string::npos) return 0;
                          if (name.find("_model.smd") != std::string::npos) return 1;
                          return 2;
                      };
                      int aScore = score(aName), bScore = score(bName);
                      if (aScore != bScore) return aScore < bScore;
                      return aName < bName;
                  });
        result.push_back(candidates.front());
        return result;
    }
    for (const fs::path& smd : smds) {
        const std::string name = lowerString(smd.filename().string());
        if (name.find("_lod") != std::string::npos) continue;
        if (name.find("physics") != std::string::npos) continue;
        if (name.find("collision") != std::string::npos) continue;
        if (name == "idle.smd") continue;
        if (name == "ref.smd") continue;
        if (name == "bindpose.smd") continue;
        result.push_back(smd);
        break;
    }
    return result;
}

fs::path findCollisionSMD(const fs::path& decompiled, const fs::path& mdlPath) {
    const std::vector<fs::path> qcs = findFiles(decompiled, ".qc");
    for (const fs::path& qc : qcs) {
        const std::vector<std::string> collisionFiles = extractQCCollisionFiles(qc);
        if (!collisionFiles.empty()) {
            const fs::path smd = findSMDByRelativeName(decompiled, collisionFiles.front());
            if (!smd.empty()) return smd;
        }
    }
    const std::vector<fs::path> smds = findFiles(decompiled, ".smd");
    const std::string modelStem = lowerString(mdlPath.stem().string());
    auto score = [&](const fs::path& smd) -> int {
        const std::string name = lowerString(smd.filename().string());
        if (name == modelStem + "_physics.smd") return 0;
        if (name == modelStem + "_collision.smd") return 1;
        if (name.find("physics") != std::string::npos) return 2;
        if (name.find("collision") != std::string::npos) return 3;
        return 100;
    };
    fs::path best;
    int bestScore = 100;
    for (const fs::path& smd : smds) {
        int currentScore = score(smd);
        if (currentScore < bestScore) { bestScore = currentScore; best = smd; }
    }
    return best;
}

} // namespace vmfpropmerger