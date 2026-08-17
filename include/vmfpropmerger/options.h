#pragma once

#include "vmfpropmerger/utils.h"

namespace vmfpropmerger {

// Forward declaration (now inside namespace)
struct PropDynamic;

struct Options {
    fs::path vmfPath;
    fs::path gameRoot;
    fs::path crowbar;
    fs::path studiomdl;
    bool crowbarExplicit = false;
    bool studiomdlExplicit = false;
    fs::path workDir;
    bool workDirExplicit = false;
    bool keepWork = false;
    bool noVpk = false;
    bool noVmf = false;
    bool keepOriginalProps = false;
    bool allowFailures = false;
    bool dryRun = false;
    bool quiet = false;
    bool verbose = false;
    bool applyRotation = true;
    bool transformNormals = true;
    double globalScale = 1.0;
    Vec3 globalOffset = {};
    std::string coordMap = "-y,x,z";
    std::vector<std::string> excludeModels;
    std::vector<std::string> includeModels;
    fs::path excludeFile;
    size_t trianglesPerBody = 12000;
    size_t maxConvexPieces = 4096;
    fs::path outputDirectory;
    bool outputDirectoryExplicit = false;
    std::string modelName;
    std::string targetName;
    fs::path outputVmf;
    bool outputVmfExplicit = false;
    std::string surfaceProp = "default";
    double sequenceFps = 1.0;
    bool showHelp = false;
    bool showVersion = false;
};

void printBanner();
void printUsage();
bool requireValue(int argc, char** argv, int& index, const char* option, std::string& value);
bool parseOptions(int argc, char** argv, Options& options);

void loadExcludeFile(const fs::path& path, std::vector<std::string>& patterns);
std::string normalizedModelName(const std::string& model);
bool modelMatchesPatterns(const std::string& model, const std::vector<std::string>& patterns);
bool shouldProcessProp(const PropDynamic& prop, const Options& options);

} // namespace vmfpropmerger