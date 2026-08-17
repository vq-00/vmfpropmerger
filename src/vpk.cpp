#include "vmfpropmerger/vpk.h"

namespace vmfpropmerger {

static uint16_t readU16(const std::vector<uint8_t>& data, size_t position) {
    if (position + 2 > data.size()) return 0;
    return static_cast<uint16_t>(data[position] | (data[position + 1] << 8));
}
static uint32_t readU32(const std::vector<uint8_t>& data, size_t position) {
    if (position + 4 > data.size()) return 0;
    return static_cast<uint32_t>(data[position]) |
           (static_cast<uint32_t>(data[position + 1]) << 8) |
           (static_cast<uint32_t>(data[position + 2]) << 16) |
           (static_cast<uint32_t>(data[position + 3]) << 24);
}
static std::string readCString(const std::vector<uint8_t>& data, size_t& position) {
    std::string result;
    while (position < data.size() && data[position] != '\0') {
        result.push_back(static_cast<char>(data[position++]));
    }
    if (position < data.size()) ++position;
    return result;
}

VpkFile::VpkFile(const fs::path& path) : m_path(path) {}

bool VpkFile::load() {
    std::vector<uint8_t> header;
    if (!readFileBytes(m_path, header)) return false;
    if (header.size() < 12) return false;
    if (readU32(header, 0) != 0x55AA1234) return false;
    m_version = readU32(header, 4);
    m_treeSize = readU32(header, 8);
    size_t headerSize = 12;
    if (m_version == 2) headerSize = 28;
    else if (m_version != 1) return false;
    if (header.size() < headerSize) return false;
    std::ifstream file(m_path, std::ios::binary);
    if (!file) return false;
    file.seekg(static_cast<std::streamoff>(headerSize), std::ios::beg);
    std::vector<uint8_t> tree(m_treeSize);
    if (m_treeSize > 0) {
        file.read(reinterpret_cast<char*>(tree.data()), static_cast<std::streamsize>(m_treeSize));
        if (!file) return false;
    }
    return parseTree(tree);
}

bool VpkFile::find(const std::string& filename, VpkEntry& entry) const {
    std::string normalized = lowerString(normalizeSlashes(filename));
    auto it = m_lookup.find(normalized);
    if (it == m_lookup.end()) return false;
    entry = m_entries[it->second];
    return true;
}

bool VpkFile::extract(const VpkEntry& entry, std::vector<uint8_t>& result) const {
    result = entry.preload;
    if (entry.entryLength == 0) return true;
    const fs::path archivePath = getArchivePath(entry.archiveIndex);
    if (archivePath.empty()) return false;
    std::ifstream file(archivePath, std::ios::binary);
    if (!file) return false;
    uint64_t absoluteOffset = entry.entryOffset;
    if (entry.archiveIndex == 0x7fff) {
        const size_t headerSize = (m_version == 2) ? 28 : 12;
        absoluteOffset = static_cast<uint64_t>(headerSize) + static_cast<uint64_t>(m_treeSize) +
                         static_cast<uint64_t>(entry.entryOffset);
    }
    file.seekg(static_cast<std::streamoff>(absoluteOffset), std::ios::beg);
    if (!file) return false;
    const size_t oldSize = result.size();
    result.resize(oldSize + entry.entryLength);
    file.read(reinterpret_cast<char*>(result.data() + oldSize), static_cast<std::streamsize>(entry.entryLength));
    return !!file;
}

bool VpkFile::parseTree(const std::vector<uint8_t>& data) {
    size_t p = 0;
    while (p < data.size()) {
        const std::string extension = readCString(data, p);
        if (extension.empty()) break;
        while (p < data.size()) {
            const std::string path = readCString(data, p);
            if (path.empty()) break;
            while (p < data.size()) {
                const std::string filename = readCString(data, p);
                if (filename.empty()) break;
                if (p + 18 > data.size()) return false;
                VpkEntry entry;
                entry.crc = readU32(data, p); p += 4;
                entry.preloadBytes = readU16(data, p); p += 2;
                entry.archiveIndex = readU16(data, p); p += 2;
                entry.entryOffset = readU32(data, p); p += 4;
                entry.entryLength = readU32(data, p); p += 4;
                p += 2;
                if (p + entry.preloadBytes > data.size()) return false;
                if (entry.preloadBytes > 0) {
                    entry.preload.assign(data.begin() + p, data.begin() + p + entry.preloadBytes);
                    p += entry.preloadBytes;
                }
                std::string fullPath;
                if (path.empty() || path == " ") fullPath = filename;
                else fullPath = path + "/" + filename;
                if (!extension.empty()) fullPath += "." + extension;
                fullPath = lowerString(normalizeSlashes(fullPath));
                m_lookup[fullPath] = m_entries.size();
                m_entries.push_back(std::move(entry));
            }
        }
    }
    return true;
}

fs::path VpkFile::getArchivePath(uint16_t index) const {
    if (index == 0x7fff) return m_path;
    const std::string filename = m_path.filename().string();
    const std::string lowerName = lowerString(filename);
    const std::string marker = "_dir.vpk";
    const size_t position = lowerName.rfind(marker);
    if (position == std::string::npos) return {};
    const std::string prefix = filename.substr(0, position);
    std::ostringstream stream;
    stream << prefix << "_" << std::setw(3) << std::setfill('0') << index << ".vpk";
    return m_path.parent_path() / stream.str();
}

void VpkDatabase::initialize(const fs::path& gameRoot, bool enabled, bool verbose) {
    m_gameRoot = gameRoot;
    if (!enabled) return;
    std::error_code ec;
    if (!fs::exists(gameRoot, ec)) return;
    for (auto& entry : fs::directory_iterator(gameRoot, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        const std::string filename = lowerString(entry.path().filename().string());
        if (!endsWithInsensitive(filename, "_dir.vpk")) continue;
        if (verbose) std::cout << "Loading VPK: " << entry.path() << "\n";
        VpkFile vpk(entry.path());
        if (vpk.load()) {
            m_vpks.push_back(std::move(vpk));
        } else if (verbose) {
            std::cerr << " Warning: failed to load VPK.\n";
        }
    }
    if (verbose) std::cout << "Loaded " << m_vpks.size() << " VPK archive(s).\n\n";
}

bool VpkDatabase::extractModelSet(const std::string& modelPath, const fs::path& destinationRoot,
                                  fs::path& outMdl) {
    std::string normalizedInput = normalizeSlashes(modelPath);
    std::string cleanedPath = removeLeadingSlash(normalizedInput);
    std::string base = removeExtension(cleanedPath);
    const std::vector<std::string> suffixes = {
        ".mdl", ".vvd", ".dx90.vtx", ".dx80.vtx", ".sw.vtx", ".vtx", ".phy", ".ani"
    };
    bool foundMdl = false;
    for (const std::string& suffix : suffixes) {
        const std::string wanted = base + suffix;
        const fs::path destination = destinationRoot / wanted;
        std::error_code ec;
        fs::create_directories(destination.parent_path(), ec);
        if (ec) {
            std::cerr << " Failed to create directory: " << destination.parent_path() << "\n";
            if (suffix == ".mdl") return false;
            continue;
        }
        const fs::path loose = m_gameRoot / wanted;
        if (fs::exists(loose, ec) && fs::is_regular_file(loose, ec)) {
            fs::copy_file(loose, destination, fs::copy_options::overwrite_existing, ec);
            if (!ec) {
                std::cout << " Found loose: " << wanted << "\n";
                if (suffix == ".mdl") { foundMdl = true; outMdl = fs::absolute(destination); }
                continue;
            }
        }
        bool found = false;
        for (const VpkFile& vpk : m_vpks) {
            VpkEntry entry;
            if (!vpk.find(wanted, entry)) continue;
            std::vector<uint8_t> data;
            if (!vpk.extract(entry, data)) {
                std::cerr << " Failed to extract VPK entry: " << wanted << "\n";
                continue;
            }
            if (!writeFileBytes(destination, data)) {
                std::cerr << " Failed to write: " << destination << "\n";
                continue;
            }
            std::cout << " Extracted VPK: " << wanted << "\n";
            if (suffix == ".mdl") { foundMdl = true; outMdl = fs::absolute(destination); }
            found = true;
            break;
        }
        if (!found && suffix == ".mdl") {
            std::cout << " MISSING: " << wanted << "\n";
        }
    }
    return foundMdl;
}

} // namespace vmfpropmerger