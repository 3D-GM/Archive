#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace mrg {

struct MRGFileEntry {
    uint32_t id;
    uint32_t offset;
    uint32_t length;
    std::string name;
    std::vector<uint8_t> data;
};

struct MRGFolderEntry {
    std::string name;
    std::vector<MRGFileEntry> files;
};

class MRGArchive {
public:
    bool load(const std::vector<uint8_t>& buffer);
    bool save(std::vector<uint8_t>& buffer) const;
    bool extractAll(const std::string& outDir) const;
    bool pack(const std::string& folderPath);
    const std::vector<MRGFolderEntry>& getFolders() const { return folders_; }
private:
    std::vector<MRGFolderEntry> folders_;
};

} // namespace mrg
