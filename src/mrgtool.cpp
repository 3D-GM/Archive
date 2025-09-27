#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include "mrg/MRGArchive.h"

void print_usage() {
    std::cout << "Usage:\n  mrgtool extract <archive.mrg> <output_dir>\n  mrgtool pack <input_dir> <archive.mrg>\n";
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(); return 1; }
    std::string cmd = argv[1];
    if (cmd == "extract" && argc == 4) {
        std::string archivePath = argv[2];
        std::string outDir = argv[3];
        auto buffer = read_file(archivePath);
        mrg::MRGArchive archive;
        if (!archive.load(buffer)) {
            std::cerr << "Failed to load archive!\n";
            return 2;
        }
        if (!archive.extractAll(outDir)) {
            std::cerr << "Failed to extract files!\n";
            return 3;
        }
        std::cout << "Extraction complete.\n";
        return 0;
    } else if (cmd == "pack" && argc == 4) {
        std::string inDir = argv[2];
        std::string archivePath = argv[3];
        mrg::MRGArchive archive;
        if (!archive.pack(inDir)) {
            std::cerr << "Failed to pack directory!\n";
            return 4;
        }
        std::vector<uint8_t> buffer;
        if (!archive.save(buffer)) {
            std::cerr << "Failed to save archive!\n";
            return 5;
        }
        std::ofstream ofs(archivePath, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        std::cout << "Packing complete.\n";
        return 0;
    } else {
        print_usage();
        return 1;
    }
}
