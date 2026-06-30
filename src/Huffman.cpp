// =============================================================================
// Huffman.cpp — Implementation of the Huffman File Compressor
// =============================================================================

#include "Huffman.hpp"
#include <queue>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <chrono>

// ----------------------------------------------------------------------
void freeTree(HuffNode* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

// ----------------------------------------------------------------------
bool MinHeapCmp::operator()(const HuffNode* a, const HuffNode* b) const {
    if (a->freq != b->freq) return a->freq > b->freq;
    return a->byte > b->byte;
}

// ----------------------------------------------------------------------
// TreeBuilder::build
//
// Greedy algorithm: always merge the two lowest-frequency nodes first.
// This guarantees frequent bytes end up closest to the root (shortest
// codes) and rare bytes end up deepest (longest codes).
// ----------------------------------------------------------------------
HuffNode* TreeBuilder::build(const std::unordered_map<uint8_t, uint64_t>& freqTable) {
    std::priority_queue<HuffNode*, std::vector<HuffNode*>, MinHeapCmp> pq;
    for (auto& [b, f] : freqTable)
        pq.push(new HuffNode(b, f));

    if (pq.size() == 1) {
        HuffNode* only = pq.top(); pq.pop();
        return new HuffNode(only->freq, only, nullptr);
    }

    while (pq.size() > 1) {
        HuffNode* l = pq.top(); pq.pop();
        HuffNode* r = pq.top(); pq.pop();
        pq.push(new HuffNode(l->freq + r->freq, l, r));
    }
    return pq.top();
}

// ----------------------------------------------------------------------
// TreeBuilder::buildCodeTable
// DFS traversal: left edge appends '0', right edge appends '1'.
// ----------------------------------------------------------------------
void TreeBuilder::buildCodeTable(HuffNode* node, const std::string& prefix,
                                  std::unordered_map<uint8_t, std::string>& table) {
    if (!node) return;
    if (node->isLeaf()) { table[node->byte] = prefix.empty() ? "0" : prefix; return; }
    buildCodeTable(node->left,  prefix + "0", table);
    buildCodeTable(node->right, prefix + "1", table);
}

// ----------------------------------------------------------------------
// BitBuffer — write side
// ----------------------------------------------------------------------
void BitBuffer::writeBit(int bit) {
    wBuf |= (static_cast<uint8_t>(bit & 1) << (7 - wBits));
    if (++wBits == 8) { data.push_back(wBuf); wBuf = 0; wBits = 0; }
}

void BitBuffer::writeBits(const std::string& bits) {
    for (char c : bits) writeBit(c == '1' ? 1 : 0);
}

int BitBuffer::flushWrite() {
    if (wBits == 0) return 0;
    int pad = 8 - wBits;
    data.push_back(wBuf); wBuf = 0; wBits = 0;
    return pad;
}

// ----------------------------------------------------------------------
// BitBuffer — read side
// ----------------------------------------------------------------------
void BitBuffer::initRead() { rByte = 0; rBits = 7; }

int BitBuffer::readBit() {
    if (rByte >= data.size()) return -1;
    int bit = (data[rByte] >> rBits) & 1;
    if (--rBits < 0) { ++rByte; rBits = 7; }
    return bit;
}

void BitBuffer::writeTo(std::ofstream& out) {
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
}

void BitBuffer::readFrom(std::ifstream& in) {
    data.assign(std::istreambuf_iterator<char>(in),
                std::istreambuf_iterator<char>());
}

// ----------------------------------------------------------------------
// Tree serialisation (preorder)
//   Internal node -> bit 0, recurse left, recurse right
//   Leaf node     -> bit 1, then 8 bits of byte value
// Writing the tree shape directly into the bit stream lets the decoder
// reconstruct an identical tree without needing a separate frequency table.
// ----------------------------------------------------------------------
void serialiseTree(HuffNode* node, BitBuffer& bb) {
    if (node->isLeaf()) {
        bb.writeBit(1);
        for (int i = 7; i >= 0; --i) bb.writeBit((node->byte >> i) & 1);
        return;
    }
    bb.writeBit(0);
    serialiseTree(node->left,  bb);
    serialiseTree(node->right, bb);
}

HuffNode* deserialiseTree(BitBuffer& bb) {
    int bit = bb.readBit();
    if (bit == 1) {
        uint8_t byte = 0;
        for (int i = 7; i >= 0; --i)
            byte |= static_cast<uint8_t>(bb.readBit()) << i;
        return new HuffNode(byte, 0);
    }
    HuffNode* l = deserialiseTree(bb);
    HuffNode* r = deserialiseTree(bb);
    return new HuffNode(0, l, r);
}

// ----------------------------------------------------------------------
// Big-endian helpers for the 8-byte symbol count header
// ----------------------------------------------------------------------
static void writeU64(std::ofstream& o, uint64_t v) {
    for (int i = 7; i >= 0; --i) o.put(static_cast<char>((v >> (8*i)) & 0xFF));
}
static uint64_t readU64(std::ifstream& i) {
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) v = (v << 8) | static_cast<uint8_t>(i.get());
    return v;
}

// ----------------------------------------------------------------------
// Display helpers
// ----------------------------------------------------------------------
static void printBox(uint64_t originalSize, uint64_t compressedSize, double elapsed) {
    double ratio = 100.0 * (1.0 - (double)compressedSize / (double)originalSize);
    std::cout << "\n";
    std::cout << "  +---------------------------------------+\n";
    std::cout << "  |        COMPRESSION COMPLETE           |\n";
    std::cout << "  +---------------------------------------+\n";
    std::cout << "  | Original size  : " << std::setw(10) << originalSize   << " bytes       |\n";
    std::cout << "  | Compressed size: " << std::setw(10) << compressedSize << " bytes       |\n";
    std::cout << "  | Compression    : " << std::setw(9)  << std::fixed << std::setprecision(1) << ratio << "%            |\n";
    std::cout << "  | Time taken     : " << std::setw(10) << std::fixed << std::setprecision(4) << elapsed << "s           |\n";
    std::cout << "  +---------------------------------------+\n\n";
}

static void printDecompBox(uint64_t outputSize, double elapsed) {
    std::cout << "\n";
    std::cout << "  +---------------------------------------+\n";
    std::cout << "  |       DECOMPRESSION COMPLETE          |\n";
    std::cout << "  +---------------------------------------+\n";
    std::cout << "  | Restored size  : " << std::setw(10) << outputSize << " bytes       |\n";
    std::cout << "  | Time taken     : " << std::setw(10) << std::fixed << std::setprecision(4) << elapsed << "s           |\n";
    std::cout << "  +---------------------------------------+\n\n";
}

// ----------------------------------------------------------------------
// Encoder::encode
//
// File layout:
//   [8 bytes] total symbol count
//   [1 byte]  padding bit count
//   [N bytes] serialised tree bits + encoded payload, packed together
// ----------------------------------------------------------------------
void Encoder::encode(const std::string& inputPath, const std::string& outputPath) {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + inputPath);

    std::unordered_map<uint8_t, uint64_t> freqTable;
    uint64_t totalBytes = 0;
    for (int c; (c = in.get()) != EOF; ++totalBytes)
        freqTable[static_cast<uint8_t>(c)]++;
    if (totalBytes == 0) throw std::runtime_error("Input file is empty.");

    HuffNode* root = TreeBuilder::build(freqTable);
    std::unordered_map<uint8_t, std::string> codeTable;
    TreeBuilder::buildCodeTable(root, "", codeTable);

    BitBuffer bb;
    serialiseTree(root, bb);

    in.clear(); in.seekg(0);
    for (int c; (c = in.get()) != EOF;)
        bb.writeBits(codeTable[static_cast<uint8_t>(c)]);

    int padding = bb.flushWrite();

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output: " + outputPath);

    writeU64(out, totalBytes);
    out.put(static_cast<char>(padding));
    bb.writeTo(out);
    out.close();

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    uint64_t compressedSize = static_cast<uint64_t>(
        std::ifstream(outputPath, std::ios::binary | std::ios::ate).tellg());

    freeTree(root);
    printBox(totalBytes, compressedSize, elapsed);
}

// ----------------------------------------------------------------------
// Decoder::decode
// ----------------------------------------------------------------------
void Decoder::decode(const std::string& inputPath, const std::string& outputPath) {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::ifstream in(inputPath, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input: " + inputPath);

    uint64_t totalBytes = readU64(in);
    int      padding    = static_cast<uint8_t>(in.get());
    (void)padding; // padding is implicitly handled by totalBytes as the stop condition

    BitBuffer bb;
    bb.readFrom(in);
    bb.initRead();

    HuffNode* root = deserialiseTree(bb);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output: " + outputPath);

    HuffNode* cur     = root;
    uint64_t  written = 0;
    while (written < totalBytes) {
        int bit = bb.readBit();
        if (bit == -1) break;

        cur = (bit == 0) ? (cur->left  ? cur->left  : cur)
                         : (cur->right ? cur->right : cur);

        if (cur->isLeaf()) {
            out.put(static_cast<char>(cur->byte));
            ++written;
            cur = root;
        }
    }
    out.close();

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    freeTree(root);
    printDecompBox(written, elapsed);
}
