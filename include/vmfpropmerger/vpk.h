#pragma once

#include "vmfpropmerger/utils.h"

namespace vmfpropmerger {

struct VpkEntry {
    uint32_t crc = 0;
    uint16_t preloadBytes = 0;
    uint16_t archiveIndex = 0;
    uint32_t entryOffset = 0;
    uint32_t entryLength = 0;
    std::vector<uint8_t> preload;
};

class VpkFile {
public:
    explicit VpkFile(const fs::path& path);
    bool load();
    bool find(const std::string& filename, VpkEntry& entry) const;
    bool extract(const VpkEntry& entry, std::vector<uint8_t>& result) const;

private:
    fs::path m_path;
    uint32_t m_version = 0;
    uint32_t m_treeSize = 0;
    std::vector<VpkEntry> m_entries;
    std::map<std::string, size_t> m_lookup;

    bool parseTree(const std::vector<uint8_t>& data);
    fs::path getArchivePath(uint16_t index) const;
};

class VpkDatabase {
public:
    void initialize(const fs::path& gameRoot, bool enabled, bool verbose);
    bool extractModelSet(const std::string& modelPath, const fs::path& destinationRoot, fs::path& outMdl);

private:
    fs::path m_gameRoot;
    std::vector<VpkFile> m_vpks;
};

} // namespace vmfpropmerger