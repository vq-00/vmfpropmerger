#include "vmfpropmerger/options.h"
#include "vmfpropmerger/vmf.h"      // for PropDynamic

namespace vmfpropmerger {

void printBanner() {
    std::cout << "====================================================\n"
              << " Reactive Drop VMF Prop Merger\n"
              << "====================================================\n\n";
}

void printUsage() {
    std::cout
        << "Usage:\n"
        << " vmfpropmerger.exe [options]\n\n"
        << "Input:\n"
        << " --vmf <path> VMF file\n"
        << " --game <path> Reactive Drop game directory\n\n"
        << "Model filtering:\n"
        << " --exclude-model <pattern> Exclude model (repeatable)\n"
        << " --exclude-file <path> File containing excluded models/patterns\n"
        << " --include-model <pattern> Only process matching models (repeatable)\n\n"
        << "Transforms:\n"
        << " --coord-map <map> Coordinate conversion, default: -y,x,z\n"
        << " Examples: -y,x,z x,y,z y,x,z\n"
        << " --rotation Apply prop angles (default)\n"
        << " --no-rotation Ignore prop angles\n"
        << " --transform-normals Transform normals (default)\n"
        << " --no-transform-normals Leave normals in source orientation\n"
        << " --global-scale <number> Additional global scale\n"
        << " --offset <x> <y> <z> Global XYZ offset\n\n"
        << "Output:\n"
        << " --output-dir <path> Output model directory\n"
        << " --model-name <name> Final model filename/path\n"
        << " --targetname <name> Targetname for merged VMF entity\n"
        << " --output-vmf <path> Output VMF path\n"
        << " --max-triangles <number> Triangles per generated SMD body\n"
        << " --max-convex-pieces <number> Maximum collision convex pieces\n"
        << " --surfaceprop <name> QC surfaceprop\n"
        << " --sequence-fps <number> QC sequence FPS\n\n"
        << "Tools:\n"
        << " --crowbar <path> Crowbar executable\n"
        << " --studiomdl <path> studiomdl.exe\n"
        << " --no-vpk Do not search VPK archives\n\n"
        << "Work files:\n"
        << " --work-dir <path> Work directory\n"
        << " --keep-work Do not delete work directory\n\n"
        << "VMF behavior:\n"
        << " --no-vmf Do not create a modified VMF\n"
        << " --keep-original-props Keep successfully merged props\n"
        << " --allow-failures Compile even when some props fail\n\n"
        << "Diagnostics:\n"
        << " --dry-run Parse/filter only; no external tools\n"
        << " --quiet Minimal output\n"
        << " --verbose More diagnostic output\n"
        << " --help Show this help\n"
        << " --version Show version\n\n"
        << "Examples:\n"
        << " vmfpropmerger.exe --vmf map.vmf "
        << "--game \"D:\\\\Games\\\\Reactive Drop\\\\reactivedrop\"\n\n"
        << " vmfpropmerger.exe --vmf map.vmf "
        << "--game \"D:\\\\Games\\\\Reactive Drop\\\\reactivedrop\" \\\n"
        << " --exclude-model \"props/*\" \\\n"
        << " --exclude-model \"models/characters/*\" \\\n"
        << " --max-triangles 10000 \\\n"
        << " --keep-work\n\n";
}

bool requireValue(int argc, char** argv, int& index, const char* option, std::string& value) {
    if (index + 1 >= argc) {
        std::cerr << "Missing value for " << option << "\n";
        return false;
    }
    value = argv[++index];
    return true;
}

bool parseOptions(int argc, char** argv, Options& options) {
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            options.showHelp = true; continue;
        }
        if (arg == "--version" || arg == "-v") {
            options.showVersion = true; continue;
        }
        if (arg == "--vmf") {
            std::string value;
            if (!requireValue(argc, argv, i, "--vmf", value)) return false;
            options.vmfPath = value; continue;
        }
        if (arg == "--game") {
            std::string value;
            if (!requireValue(argc, argv, i, "--game", value)) return false;
            options.gameRoot = value; continue;
        }
        if (arg == "--crowbar") {
            std::string value;
            if (!requireValue(argc, argv, i, "--crowbar", value)) return false;
            options.crowbar = value; options.crowbarExplicit = true; continue;
        }
        if (arg == "--studiomdl") {
            std::string value;
            if (!requireValue(argc, argv, i, "--studiomdl", value)) return false;
            options.studiomdl = value; options.studiomdlExplicit = true; continue;
        }
        if (arg == "--work-dir") {
            std::string value;
            if (!requireValue(argc, argv, i, "--work-dir", value)) return false;
            options.workDir = value; options.workDirExplicit = true; continue;
        }
        if (arg == "--exclude-model") {
            std::string value;
            if (!requireValue(argc, argv, i, "--exclude-model", value)) return false;
            options.excludeModels.push_back(value); continue;
        }
        if (arg == "--include-model") {
            std::string value;
            if (!requireValue(argc, argv, i, "--include-model", value)) return false;
            options.includeModels.push_back(value); continue;
        }
        if (arg == "--exclude-file") {
            std::string value;
            if (!requireValue(argc, argv, i, "--exclude-file", value)) return false;
            options.excludeFile = value; continue;
        }
        if (arg == "--coord-map") {
            std::string value;
            if (!requireValue(argc, argv, i, "--coord-map", value)) return false;
            options.coordMap = value; continue;
        }
        if (arg == "--global-scale") {
            std::string value;
            if (!requireValue(argc, argv, i, "--global-scale", value)) return false;
            options.globalScale = parseDouble(value, 1.0); continue;
        }
        if (arg == "--offset") {
            if (!parseVectorArguments(argc, argv, i, options.globalOffset)) {
                std::cerr << "--offset requires three numbers.\n"; return false;
            }
            continue;
        }
        if (arg == "--max-triangles") {
            std::string value;
            if (!requireValue(argc, argv, i, "--max-triangles", value)) return false;
            int parsed = parseInt(value, 12000);
            if (parsed <= 0) { std::cerr << "--max-triangles must be greater than zero.\n"; return false; }
            options.trianglesPerBody = static_cast<size_t>(parsed);
            continue;
        }
        if (arg == "--max-convex-pieces") {
            if (i + 1 >= argc) { std::cerr << "Missing value for --max-convex-pieces\n"; return false; }
            int value = parseInt(argv[++i], 1024);
            if (value <= 0) { std::cerr << "--max-convex-pieces must be greater than zero.\n"; return false; }
            options.maxConvexPieces = static_cast<size_t>(value);
            continue;
        }
        if (arg == "--output-dir") {
            std::string value;
            if (!requireValue(argc, argv, i, "--output-dir", value)) return false;
            options.outputDirectory = value; options.outputDirectoryExplicit = true; continue;
        }
        if (arg == "--model-name") {
            std::string value;
            if (!requireValue(argc, argv, i, "--model-name", value)) return false;
            options.modelName = value; continue;
        }
        if (arg == "--targetname") {
            std::string value;
            if (!requireValue(argc, argv, i, "--targetname", value)) return false;
            options.targetName = value; continue;
        }
        if (arg == "--output-vmf") {
            std::string value;
            if (!requireValue(argc, argv, i, "--output-vmf", value)) return false;
            options.outputVmf = value; options.outputVmfExplicit = true; continue;
        }
        if (arg == "--surfaceprop") {
            std::string value;
            if (!requireValue(argc, argv, i, "--surfaceprop", value)) return false;
            options.surfaceProp = value; continue;
        }
        if (arg == "--sequence-fps") {
            std::string value;
            if (!requireValue(argc, argv, i, "--sequence-fps", value)) return false;
            options.sequenceFps = parseDouble(value, 1.0); continue;
        }
        if (arg == "--keep-work") { options.keepWork = true; continue; }
        if (arg == "--no-vpk") { options.noVpk = true; continue; }
        if (arg == "--no-vmf") { options.noVmf = true; continue; }
        if (arg == "--keep-original-props") { options.keepOriginalProps = true; continue; }
        if (arg == "--allow-failures") { options.allowFailures = true; continue; }
        if (arg == "--dry-run") { options.dryRun = true; continue; }
        if (arg == "--quiet") { options.quiet = true; continue; }
        if (arg == "--verbose") { options.verbose = true; continue; }
        if (arg == "--rotation") { options.applyRotation = true; continue; }
        if (arg == "--no-rotation") { options.applyRotation = false; continue; }
        if (arg == "--transform-normals") { options.transformNormals = true; continue; }
        if (arg == "--no-transform-normals") { options.transformNormals = false; continue; }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
        positional.push_back(arg);
    }
    if (options.showHelp || options.showVersion) return true;
    if (options.vmfPath.empty() && !positional.empty()) options.vmfPath = positional[0];
    if (options.gameRoot.empty() && positional.size() >= 2) options.gameRoot = positional[1];
    if (options.vmfPath.empty() || options.gameRoot.empty()) {
        std::cerr << "A VMF file and game directory are required.\n\n";
        printUsage();
        return false;
    }
    return true;
}

void loadExcludeFile(const fs::path& path, std::vector<std::string>& patterns) {
    if (path.empty()) return;
    std::ifstream input(path);
    if (!input) {
        std::cerr << "Warning: could not open exclude file:\n" << path << "\n";
        return;
    }
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line[0] == '#' || (line.size() >= 2 && line[0] == '/' && line[1] == '/')) continue;
        patterns.push_back(line);
    }
}

std::string normalizedModelName(const std::string& model) {
    return lowerString(removeLeadingSlash(normalizeSlashes(model)));
}

bool modelMatchesPatterns(const std::string& model, const std::vector<std::string>& patterns) {
    const std::string normalized = normalizedModelName(model);
    for (const std::string& pattern : patterns) {
        const std::string normalizedPattern = normalizedModelName(pattern);
        if (normalized == normalizedPattern) return true;
        if (wildcardMatch(normalizedPattern, normalized)) return true;
    }
    return false;
}

bool shouldProcessProp(const PropDynamic& prop, const Options& options) {
    if (!options.includeModels.empty() && !modelMatchesPatterns(prop.model, options.includeModels))
        return false;
    if (modelMatchesPatterns(prop.model, options.excludeModels))
        return false;
    return true;
}

} // namespace vmfpropmerger