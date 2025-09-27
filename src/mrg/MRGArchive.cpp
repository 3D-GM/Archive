#include "MRGArchive.h"
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iostream>
#include <functional>

namespace mrg {

bool MRGArchive::load(const std::vector<uint8_t>& buffer) {
    folders_.clear();
    if (buffer.size() < 8) {
        std::cerr << "[MRG] Buffer too small: " << buffer.size() << " bytes\n";
        return false;
    }
    size_t pos = 0;
    auto print_raw_bytes = [&](size_t p) {
        std::cerr << "[MRG]   Raw bytes at pos " << p << ": ";
        for (int i = 0; i < 4 && p + i < buffer.size(); ++i) {
            std::cerr << std::hex << std::uppercase << (int)buffer[p + i] << " ";
        }
        std::cerr << std::dec << std::nouppercase << std::endl;
    };
    std::function<uint32_t(size_t&)> read_u32_le = [&](size_t& p) -> uint32_t {
        if (p + 4 > buffer.size()) {
            std::cerr << "[MRG] Unexpected EOF while reading u32 at pos " << p << "\n";
            return 0;
        }
        print_raw_bytes(p);
        uint32_t v = buffer[p] | (buffer[p+1] << 8) | (buffer[p+2] << 16) | (buffer[p+3] << 24);
        p += 4;
        return v;
    };
    std::function<uint32_t(size_t&)> read_u32_be = [&](size_t& p) -> uint32_t {
        if (p + 4 > buffer.size()) {
            std::cerr << "[MRG] Unexpected EOF while reading u32 at pos " << p << "\n";
            return 0;
        }
        print_raw_bytes(p);
        uint32_t v = (buffer[p] << 24) | (buffer[p+1] << 16) | (buffer[p+2] << 8) | buffer[p+3];
        p += 4;
        return v;
    };
    // Try little-endian first, fallback to big-endian if values look wrong
    std::function<uint32_t(size_t&)> read_u32 = read_u32_le;
    std::cerr << "[MRG] Reading fileDataOffset at pos " << pos << std::endl;
    uint32_t fileDataOffset = read_u32(pos);
    std::cerr << "[MRG] Reading numFolders at pos " << pos << std::endl;
    uint32_t numFolders = read_u32(pos);
    std::cerr << "[MRG] fileDataOffset=" << fileDataOffset << ", numFolders=" << numFolders << " (LE)\n";
    // Heuristik: Wenn Werte unplausibel, versuche Big-Endian
    if (fileDataOffset > buffer.size() || numFolders > 1000) {
        std::cerr << "[MRG] Plausibilitätsprüfung fehlgeschlagen, versuche Big-Endian!\n";
        pos = 0;
        read_u32 = read_u32_be;
        std::cerr << "[MRG] Reading fileDataOffset (BE) at pos " << pos << std::endl;
        fileDataOffset = read_u32(pos);
        std::cerr << "[MRG] Reading numFolders (BE) at pos " << pos << std::endl;
        numFolders = read_u32(pos);
        std::cerr << "[MRG] fileDataOffset=" << fileDataOffset << ", numFolders=" << numFolders << " (BE)\n";
    }
    for (uint32_t d = 0; d < numFolders; ++d) {
        if (pos + 4 > buffer.size()) {
            std::cerr << "[MRG] Unexpected EOF before folderLen at pos " << pos << "\n";
            return false;
        }
        size_t folderBlockStart = pos;
    std::cerr << "[MRG] Reading folderLen at pos " << pos << std::endl;
    uint32_t folderLen = read_u32(pos);
        std::string folderName;
        size_t folderNameStart = pos;
        while (pos < buffer.size() && buffer[pos] != 0) {
            if (pos - folderNameStart > 256) {
                std::cerr << "[MRG] Folder name too long or missing null terminator at pos " << pos << "\n";
                return false;
            }
            folderName += (char)buffer[pos++];
        }
        if (pos >= buffer.size()) {
            std::cerr << "[MRG] Unexpected EOF while reading folderName at pos " << pos << "\n";
            return false;
        }
        ++pos;
    std::cerr << "[MRG] Reading numFiles at pos " << pos << std::endl;
    uint32_t numFiles = read_u32(pos);
        std::cerr << "[MRG] Folder " << d << ": '" << folderName << "' (" << numFiles << " files, len=" << folderLen << ")\n";
        size_t folderEntriesStart = pos;
        MRGFolderEntry folder{folderName, {}};
        for (uint32_t f = 0; f < numFiles; ++f) {
            std::cerr << "[MRG] Reading fileId at pos " << pos << std::endl;
            uint32_t fileId = read_u32(pos);
            std::cerr << "[MRG] Reading fileOffset at pos " << pos << std::endl;
            uint32_t fileOffset = read_u32(pos);
            std::cerr << "[MRG] Reading fileLength at pos " << pos << std::endl;
            uint32_t fileLength = read_u32(pos);
            std::string fileName;
            size_t fileNameStart = pos;
            while (pos < buffer.size() && buffer[pos] != 0) {
                if (pos - fileNameStart > 256) {
                    std::cerr << "[MRG] File name too long or missing null terminator at pos " << pos << "\n";
                    return false;
                }
                fileName += (char)buffer[pos++];
            }
            if (pos >= buffer.size()) {
                std::cerr << "[MRG] Unexpected EOF while reading fileName at pos " << pos << "\n";
                return false;
            }
            ++pos;
            std::cerr << "[MRG]   File " << f << ": id=" << fileId << ", offset=" << fileOffset << ", len=" << fileLength << ", name='" << fileName << "'\n";
            MRGFileEntry entry{fileId, fileOffset, fileLength, fileName, {}};
            folder.files.push_back(entry);
        }
        folders_.push_back(folder);
        size_t parsed = pos - folderEntriesStart + (folderEntriesStart - folderBlockStart);
        if (parsed < folderLen) {
            std::cerr << "[MRG]   Padding detected: " << (folderLen - parsed) << " bytes skipped at pos " << pos << "\n";
            pos += (folderLen - parsed);
        } else if (parsed > folderLen) {
            std::cerr << "[MRG]   ERROR: Folder parsed more bytes (" << parsed << ") than folderLen (" << folderLen << ")!\n";
            return false;
        }
        std::cerr << "[MRG]   Folder parsed: expected len=" << folderLen << ", actual parsed=" << parsed << ", buffer pos=" << pos << "/" << buffer.size() << "\n";
    }
    // Read file data
    // File offsets are relative to fileDataOffset!
    for (auto& folder : folders_) {
        // Konsistenzprüfung: Offsets/Längen sortiert, keine Überlappung, keine Lücken (optional)
        struct FileRange { size_t start, end; std::string name; };
        std::vector<FileRange> ranges;
        for (auto& file : folder.files) {
            size_t absOffset = static_cast<size_t>(fileDataOffset) + static_cast<size_t>(file.offset);
            size_t absEnd = absOffset + file.length;
            ranges.push_back({absOffset, absEnd, file.name});
        }
        // Sortieren nach Startoffset
        std::sort(ranges.begin(), ranges.end(), [](const FileRange& a, const FileRange& b) { return a.start < b.start; });
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (ranges[i].end > buffer.size()) {
                std::cerr << "[MRG] Konsistenzfehler: Datei '" << ranges[i].name << "' im Ordner '" << folder.name << "' geht über das Archivende hinaus (" << ranges[i].end << "/" << buffer.size() << ")\n";
                return false;
            }
            if (i > 0 && ranges[i].start < ranges[i-1].end) {
                std::cerr << "[MRG] Konsistenzfehler: Datei '" << ranges[i].name << "' im Ordner '" << folder.name << "' überschneidet sich mit vorheriger Datei '" << ranges[i-1].name << "' (" << ranges[i-1].end << " > " << ranges[i].start << ")\n";
                return false;
            }
            // Optional: Warnung bei Lücken
            if (i > 0 && ranges[i].start > ranges[i-1].end) {
                std::cerr << "[MRG] Warnung: Lücke zwischen Datei '" << ranges[i-1].name << "' und '" << ranges[i].name << "' im Ordner '" << folder.name << "' (" << ranges[i-1].end << " -> " << ranges[i].start << ")\n";
            }
        }
        // Wenn alles ok, extrahiere Daten
        for (auto& file : folder.files) {
            size_t absOffset = static_cast<size_t>(fileDataOffset) + static_cast<size_t>(file.offset);
            file.data.assign(buffer.begin() + absOffset, buffer.begin() + absOffset + file.length);
        }
    }
    std::cerr << "[MRG] Load OK\n";
    return true;
}

bool MRGArchive::save(std::vector<uint8_t>& buffer) const {
    buffer.clear();
    // Header: 4 - File Data Offset, 4 - Number Of Sub-Folders
    std::vector<uint8_t> dirBlock;
    std::vector<uint8_t> fileBlock;
    uint32_t numFolders = static_cast<uint32_t>(folders_.size());
    // Reserve space for header
    buffer.resize(8, 0);
    // Directory
    uint32_t fileDataOffset = 8; // header size
    for (const auto& folder : folders_) {
        size_t folderStart = dirBlock.size();
        dirBlock.resize(dirBlock.size() + 4, 0); // Placeholder for folder length
        // Folder name
        dirBlock.insert(dirBlock.end(), folder.name.begin(), folder.name.end());
        dirBlock.push_back(0);
        // Number of files
        uint32_t numFiles = static_cast<uint32_t>(folder.files.size());
        for (int i = 0; i < 4; ++i) dirBlock.push_back((numFiles >> (i*8)) & 0xFF);
        // Files
        for (const auto& file : folder.files) {
            // File ID
            for (int i = 0; i < 4; ++i) dirBlock.push_back((file.id >> (i*8)) & 0xFF);
            // File Offset (relative to file data start, will fill later)
            size_t offsetPos = dirBlock.size();
            dirBlock.resize(dirBlock.size() + 4, 0);
            // File Length
            for (int i = 0; i < 4; ++i) dirBlock.push_back((file.data.size() >> (i*8)) & 0xFF);
            // Filename
            dirBlock.insert(dirBlock.end(), file.name.begin(), file.name.end());
            dirBlock.push_back(0);
            // Store file data for later
            size_t fileOffset = fileBlock.size();
            fileBlock.insert(fileBlock.end(), file.data.begin(), file.data.end());
            // Write offset
            uint32_t absOffset = static_cast<uint32_t>(fileDataOffset + dirBlock.size() + fileOffset);
            for (int i = 0; i < 4; ++i) dirBlock[offsetPos + i] = (absOffset >> (i*8)) & 0xFF;
        }
        // Write folder length
        uint32_t folderLen = static_cast<uint32_t>(dirBlock.size() - folderStart);
        for (int i = 0; i < 4; ++i) dirBlock[folderStart + i] = (folderLen >> (i*8)) & 0xFF;
    }
    fileDataOffset += static_cast<uint32_t>(dirBlock.size());
    // Write header
    for (int i = 0; i < 4; ++i) buffer[i] = (fileDataOffset >> (i*8)) & 0xFF;
    for (int i = 0; i < 4; ++i) buffer[4+i] = (numFolders >> (i*8)) & 0xFF;
    // Write directory and file data
    buffer.insert(buffer.end(), dirBlock.begin(), dirBlock.end());
    buffer.insert(buffer.end(), fileBlock.begin(), fileBlock.end());
    return true;
}

bool MRGArchive::extractAll(const std::string& outDir) const {
    namespace fs = std::filesystem;
    for (const auto& folder : folders_) {
        fs::path folderPath = fs::path(outDir) / folder.name;
        fs::create_directories(folderPath);
        for (const auto& file : folder.files) {
            fs::path filePath = folderPath / file.name;
            std::ofstream ofs(filePath, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(file.data.data()), file.data.size());
        }
    }
    return true;
}

bool MRGArchive::pack(const std::string& folderPath) {
    namespace fs = std::filesystem;
    folders_.clear();
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) return false;
    for (const auto& dirEntry : fs::directory_iterator(folderPath)) {
        if (!dirEntry.is_directory()) continue;
        MRGFolderEntry folder;
        folder.name = dirEntry.path().filename().string();
        uint32_t fileId = 0;
        for (const auto& fileEntry : fs::directory_iterator(dirEntry.path())) {
            if (!fileEntry.is_regular_file()) continue;
            MRGFileEntry file;
            file.id = fileId++;
            file.name = fileEntry.path().filename().string();
            std::ifstream ifs(fileEntry.path(), std::ios::binary);
            file.data = std::vector<uint8_t>((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            file.length = static_cast<uint32_t>(file.data.size());
            file.offset = 0; // will be set during save
            folder.files.push_back(file);
        }
        folders_.push_back(folder);
    }
    return true;
}

} // namespace mrg
