#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

namespace vmfpropmerger {

static constexpr double PI = 3.1415926535897932384626433832795;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& a, double s);
double dotProduct(const Vec3& a, const Vec3& b);
double lengthVec(const Vec3& v);
Vec3 normalize(const Vec3& v);
double degToRad(double degrees);

std::string lowerString(std::string s);
std::string trim(const std::string& value);
std::string normalizeSlashes(std::string value);
std::string removeLeadingSlash(std::string value);
std::string removeExtension(const std::string& value);
bool endsWithInsensitive(const std::string& value, const std::string& ending);
std::string quoteArg(const std::string& value);
bool wildcardMatch(const std::string& pattern, const std::string& value);

bool readFileBytes(const fs::path& path, std::vector<uint8_t>& data);
bool writeFileBytes(const fs::path& path, const std::vector<uint8_t>& data);
bool readWholeFile(const fs::path& path, std::string& output);

double parseDouble(const std::string& text, double fallback);
int parseInt(const std::string& text, int fallback);
Vec3 parseVector(const std::string& text, const Vec3& fallback);
bool parseVectorArguments(int argc, char** argv, int& index, Vec3& result);

std::vector<fs::path> findFiles(const fs::path& root, const std::string& extension);

#ifdef _WIN32
std::wstring widenString(const std::string& value);
#endif

int runCommand(const fs::path& executable, const std::string& arguments,
               const fs::path& workingDirectory, bool verbose);

fs::path findCrowbar(const fs::path& gameDir);
fs::path findStudioMDL(const fs::path& gameDir);

} // namespace vmfpropmerger