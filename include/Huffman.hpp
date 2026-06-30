// =============================================================================
// Huffman.hpp — Public interface for the Huffman File Compressor
// =============================================================================

#ifndef HUFFMAN_HPP
#define HUFFMAN_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <fstream>

// ----------------------------------------------------------------------
// HuffNode — a single node in the Huffman tree. Leaves hold a byte
// value; internal nodes hold only a combined frequency.
// ----------------------------------------------------------------------
struct HuffNode {
    uint8_t   byte;
    uint64_t  freq;
    HuffNode* left;
    HuffNode* right;

    HuffNode(uint8_t b, uint64_t f)
        : byte(b), freq(f), left(nullptr), right(nullptr) {}
    HuffNode(uint64_t f, HuffNode* l, HuffNode* r)
        : byte(0), freq(f), left(l), right(r) {}

    bool isLeaf() const { return !left && !right; }
};

void freeTree(HuffNode* node);

// ----------------------------------------------------------------------
// MinHeapCmp — inverts std::priority_queue's default max-heap behavior
// so the lowest-frequency node is always on top.
// ----------------------------------------------------------------------
struct MinHeapCmp {
    bool operator()(const HuffNode* a, const HuffNode* b) const;
};

// ----------------------------------------------------------------------
// TreeBuilder — builds the Huffman tree via the greedy min-heap
// algorithm, then derives a prefix-free binary code for every byte.
// ----------------------------------------------------------------------
class TreeBuilder {
public:
    static HuffNode* build(const std::unordered_map<uint8_t, uint64_t>& freqTable);

    static void buildCodeTable(HuffNode* node, const std::string& prefix,
                                std::unordered_map<uint8_t, std::string>& table);
};

// ----------------------------------------------------------------------
// BitBuffer — packs individual bits into bytes (MSB-first) for writing
// to disk, and unpacks them back during decoding. Holds the entire
// stream in memory so tree-shape bits and payload bits share one
// contiguous buffer, avoiding byte-alignment mismatches between them.
// ----------------------------------------------------------------------
class BitBuffer {
public:
    std::vector<uint8_t> data;
    uint8_t wBuf  = 0; int wBits = 0;
    size_t  rByte = 0; int rBits = 7;

    void writeBit(int bit);
    void writeBits(const std::string& bits);
    int  flushWrite();

    void initRead();
    int  readBit();

    void writeTo(std::ofstream& out);
    void readFrom(std::ifstream& in);
};

void serialiseTree(HuffNode* node, BitBuffer& bb);
HuffNode* deserialiseTree(BitBuffer& bb);

// ----------------------------------------------------------------------
// Encoder / Decoder — top-level operations exposed to main.cpp
// ----------------------------------------------------------------------
class Encoder {
public:
    static void encode(const std::string& inputPath, const std::string& outputPath);
};

class Decoder {
public:
    static void decode(const std::string& inputPath, const std::string& outputPath);
};

#endif // HUFFMAN_HPP
