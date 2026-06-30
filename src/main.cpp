// =============================================================================
// main.cpp — CLI entry point for the Huffman File Compressor
// =============================================================================

#include "Huffman.hpp"
#include <iostream>

static void printUsage(const char* prog) {
    std::cout << "\nHuffman File Compressor\n"
              << "Usage:\n"
              << "  " << prog << " -c <input_file>  <output.bin>   compress\n"
              << "  " << prog << " -d <input.bin>   <output_file>  decompress\n\n"
              << "Examples:\n"
              << "  " << prog << " -c examples/sample.txt compressed.bin\n"
              << "  " << prog << " -d compressed.bin restored.txt\n\n";
}

int main(int argc, char* argv[]) {
    if (argc != 4) { printUsage(argv[0]); return 1; }

    std::string mode(argv[1]);
    try {
        if      (mode == "-c") Encoder::encode(argv[2], argv[3]);
        else if (mode == "-d") Decoder::decode(argv[2], argv[3]);
        else { std::cerr << "Unknown flag: " << mode << "\n"; printUsage(argv[0]); return 1; }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
