#include "vmfpropmerger/utils.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace vmfpropmerger {

Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 operator*(const Vec3& a, double s) {
    return {a.x * s, a.y * s, a.z * s};
}
double dotProduct(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
double lengthVec(const Vec3& v) {
    return std::sqrt(dotProduct(v, v));
}
Vec3 normalize(const Vec3& v) {
    const double len = lengthVec(v);
    if (len <= 1e-12) return {0.0, 0.0, 1.0};
    return v * (1.0 / len);
}
double degToRad(double degrees) {
    return degrees * PI / 180.0;
}

std::string lowerString(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
std::string trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(begin, end - begin);
}
std::string normalizeSlashes(std::string value) {
    for (char& c : value) if (c == '\\') c = '/';
    return value;
}
std::string removeLeadingSlash(std::string value) {
    while (!value.empty() && (value.front() == '/' || value.front() == '\\')) value.erase(value.begin());
    return value;
}
std::string removeExtension(const std::string& value) {
    const std::size_t pos = value.find_last_of('.');
    if (pos == std::string::npos) return value;
    const std::size_t slash = value.find_last_of("/\\");
    if (slash != std::string::npos && pos < slash) return value;
    return value.substr(0, pos);
}
bool endsWithInsensitive(const std::string& value, const std::string& ending) {
    if (value.size() < ending.size()) return false;
    return lowerString(value.substr(value.size() - ending.size())) == lowerString(ending);
}
std::string quoteArg(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        if (c == '"') result += "\\\"";
        else result += c;
    }
    result += "\"";
    return result;
}
bool wildcardMatch(const std::string& pattern, const std::string& value) {
    const std::string p = lowerString(normalizeSlashes(pattern));
    const std::string v = lowerString(normalizeSlashes(value));
    size_t pIndex = 0, vIndex = 0;
    size_t star = std::string::npos, starMatch = 0;
    while (vIndex < v.size()) {
        if (pIndex < p.size() && (p[pIndex] == '?' || p[pIndex] == v[vIndex])) {
            ++pIndex; ++vIndex; continue;
        }
        if (pIndex < p.size() && p[pIndex] == '*') {
            star = pIndex++; starMatch = vIndex; continue;
        }
        if (star != std::string::npos) {
            pIndex = star + 1;
            vIndex = ++starMatch;
            continue;
        }
        return false;
    }
    while (pIndex < p.size() && p[pIndex] == '*') ++pIndex;
    return pIndex == p.size();
}

bool readFileBytes(const fs::path& path, std::vector<uint8_t>& data) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamoff size = f.tellg();
    if (size < 0) return false;
    f.seekg(0, std::ios::beg);
    data.resize(static_cast<size_t>(size));
    if (size > 0) {
        f.read(reinterpret_cast<char*>(data.data()), size);
    }
    return !!f;
}
bool writeFileBytes(const fs::path& path, const std::vector<uint8_t>& data) {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec) return false;
    }
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    if (!data.empty()) {
        f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    return !!f;
}
bool readWholeFile(const fs::path& path, std::string& output) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream stream;
    stream << input.rdbuf();
    output = stream.str();
    return true;
}

double parseDouble(const std::string& text, double fallback) {
    try {
        size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed == 0) return fallback;
        return value;
    } catch (...) { return fallback; }
}
int parseInt(const std::string& text, int fallback) {
    try {
        size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed == 0) return fallback;
        return value;
    } catch (...) { return fallback; }
}
Vec3 parseVector(const std::string& text, const Vec3& fallback) {
    std::istringstream stream(text);
    Vec3 result;
    if (!(stream >> result.x >> result.y >> result.z)) return fallback;
    return result;
}
bool parseVectorArguments(int argc, char** argv, int& index, Vec3& result) {
    if (index + 3 >= argc) return false;
    result.x = parseDouble(argv[++index], 0.0);
    result.y = parseDouble(argv[++index], 0.0);
    result.z = parseDouble(argv[++index], 0.0);
    return true;
}

std::vector<fs::path> findFiles(const fs::path& root, const std::string& extension) {
    std::vector<fs::path> result;
    std::error_code ec;
    if (!fs::exists(root, ec)) return result;
    const std::string wanted = lowerString(extension);
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec)) continue;
        const std::string actual = lowerString(it->path().extension().string());
        if (actual == wanted) result.push_back(it->path());
    }
    return result;
}

#ifdef _WIN32
std::wstring widenString(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return std::wstring(value.begin(), value.end());
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}
#endif

int runCommand(const fs::path& executable, const std::string& arguments,
               const fs::path& workingDirectory, bool verbose) {
    if (verbose) {
        std::cout << "\n--------------------------------------------------\n"
                  << quoteArg(executable.string()) << " " << arguments
                  << "\n--------------------------------------------------\n";
    }
#ifdef _WIN32
    std::wstring executableW = executable.wstring();
    std::wstring argumentsW = widenString(arguments);
    std::wstring commandLine = L"\"" + executableW + L"\"";
    if (!argumentsW.empty()) { commandLine += L" "; commandLine += argumentsW; }
    std::wstring workingDirectoryW = workingDirectory.wstring();
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');
    BOOL success = CreateProcessW(executableW.c_str(), mutableCommandLine.data(),
                                  nullptr, nullptr, FALSE, 0, nullptr,
                                  workingDirectoryW.empty() ? nullptr : workingDirectoryW.c_str(),
                                  &si, &pi);
    if (!success) {
        std::cerr << "CreateProcess failed. Windows error: " << GetLastError() << "\n";
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exitCode);
#else
    std::string fullCommand = "cd " + quoteArg(workingDirectory.string()) + " && " + quoteArg(executable.string());
    if (!arguments.empty()) { fullCommand += " "; fullCommand += arguments; }
    return std::system(fullCommand.c_str());
#endif
}

fs::path findCrowbar(const fs::path& gameDir) {
    const char* environmentNames[] = { "CROWBAR_EXE", "CROWBAR_DECOMPILER_EXE" };
    for (const char* name : environmentNames) {
        const char* value = std::getenv(name);
        if (value != nullptr && *value != '\0') {
            fs::path candidate(value);
            std::error_code ec;
            if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
                return fs::absolute(candidate);
            }
        }
    }
    const std::vector<fs::path> candidates = {
        "CrowbarCommandLineDecomp.exe",
        "CrowbarDecompiler.exe",
        "Crowbar.exe"
    };
    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec))
            return fs::absolute(candidate);
        if (fs::exists(gameDir / candidate, ec) && fs::is_regular_file(gameDir / candidate, ec))
            return fs::absolute(gameDir / candidate);
        if (fs::exists(gameDir / "bin" / candidate, ec) && fs::is_regular_file(gameDir / "bin" / candidate, ec))
            return fs::absolute(gameDir / "bin" / candidate);
        if (fs::exists(fs::current_path() / candidate, ec) && fs::is_regular_file(fs::current_path() / candidate, ec))
            return fs::absolute(fs::current_path() / candidate);
    }
    return {};
}

fs::path findStudioMDL(const fs::path& gameDir) {
    const fs::path parentRoot = gameDir.parent_path();
    const std::vector<fs::path> candidates = {
        gameDir / "bin" / "studiomdl.exe",
        gameDir / "studiomdl.exe",
        parentRoot / "bin" / "studiomdl.exe",
        fs::current_path() / "studiomdl.exe"
    };
    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
            return fs::absolute(candidate);
        }
    }
    return {};
}

} // namespace vmfpropmerger