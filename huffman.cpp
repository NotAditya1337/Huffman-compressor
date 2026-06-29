// =============================================================================
// huffman.cpp — Lossless File Compressor/Decompressor using Huffman Coding
// Compile : g++ -std=c++17 -O2 -o huffman huffman.cpp
// Compress: ./huffman -c input.txt compressed.bin
// Decomp  : ./huffman -d compressed.bin output.txt
// =============================================================================

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <queue>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <chrono>
#include <iomanip>

// =============================================================================
// SECTION 1: HUFFMAN TREE NODE
// Each leaf holds a byte value. Internal nodes hold combined frequencies.
// =============================================================================

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

void freeTree(HuffNode* node) {
    if (!node) return;
    freeTree(node->left);
    freeTree(node->right);
    delete node;
}

// =============================================================================
// SECTION 2: MIN-HEAP COMPARATOR
// std::priority_queue is a max-heap by default — inverting the comparison
// makes the lowest-frequency node always appear at the top.
// =============================================================================

struct MinHeapCmp {
    bool operator()(const HuffNode* a, const HuffNode* b) const {
        if (a->freq != b->freq) return a->freq > b->freq;
        return a->byte > b->byte;
    }
};

// =============================================================================
// SECTION 3: TREE BUILDER
// =============================================================================

class TreeBuilder {
public:
    // GREEDY ALGORITHM: always merge the two cheapest nodes first.
    // This guarantees the most frequent bytes end up closest to the root
    // (shortest codes) and the rarest bytes deepest (longest codes).
    static HuffNode* build(const std::unordered_map<uint8_t, uint64_t>& freqTable) {
        std::priority_queue<HuffNode*, std::vector<HuffNode*>, MinHeapCmp> pq;
        for (auto& [b, f] : freqTable)
            pq.push(new HuffNode(b, f));

        if (pq.size() == 1) {
            HuffNode* only = pq.top(); pq.pop();
            return new HuffNode(only->freq, only, nullptr);
        }

        while (pq.size() > 1) {
            HuffNode* l = pq.top(); pq.pop(); // Lowest freq
            HuffNode* r = pq.top(); pq.pop(); // Second lowest
            pq.push(new HuffNode(l->freq + r->freq, l, r));
        }
        return pq.top();
    }

    // DFS traversal: left edge = '0', right edge = '1'
    static void buildCodeTable(HuffNode* node, const std::string& prefix,
                               std::unordered_map<uint8_t, std::string>& table) {
        if (!node) return;
        if (node->isLeaf()) { table[node->byte] = prefix.empty() ? "0" : prefix; return; }
        buildCodeTable(node->left,  prefix + "0", table);
        buildCodeTable(node->right, prefix + "1", table);
    }
};

// =============================================================================
// SECTION 4: BIT BUFFER
// Holds the entire bit stream in memory so tree + payload share one
// contiguous buffer — avoids byte-alignment bugs between the two.
// =============================================================================

class BitBuffer {
public:
    std::vector<uint8_t> data;
    uint8_t wBuf  = 0; int wBits = 0; // write state
    size_t  rByte = 0; int rBits = 7; // read state

    // ---- WRITE ----
    void writeBit(int bit) {
        // Pack MSB-first; flush a complete byte to data[] when full
        wBuf |= (static_cast<uint8_t>(bit & 1) << (7 - wBits));
        if (++wBits == 8) { data.push_back(wBuf); wBuf = 0; wBits = 0; }
    }
    void writeBits(const std::string& bits) {
        for (char c : bits) writeBit(c == '1' ? 1 : 0);
    }
    int flushWrite() {                      // returns padding bit count
        if (wBits == 0) return 0;
        int pad = 8 - wBits;
        data.push_back(wBuf); wBuf = 0; wBits = 0;
        return pad;
    }

    // ---- READ ----
    void initRead() { rByte = 0; rBits = 7; }
    int readBit() {
        if (rByte >= data.size()) return -1;
        // Extract next bit from MSB side of current byte
        int bit = (data[rByte] >> rBits) & 1;
        if (--rBits < 0) { ++rByte; rBits = 7; }
        return bit;
    }

    void writeTo(std::ofstream& out) {
        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    void readFrom(std::ifstream& in) {
        data.assign(std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
    }
};

// =============================================================================
// SECTION 5: TREE SERIALISATION (preorder)
// Writes the tree shape directly into the bit stream so the decoder
// reconstructs the identical tree — no frequency table needed.
//   Internal node → bit 0, recurse left, recurse right
//   Leaf node     → bit 1, then 8 bits of byte value
// =============================================================================

static void serialiseTree(HuffNode* node, BitBuffer& bb) {
    if (node->isLeaf()) {
        bb.writeBit(1);
        for (int i = 7; i >= 0; --i) bb.writeBit((node->byte >> i) & 1);
        return;
    }
    bb.writeBit(0);
    serialiseTree(node->left,  bb);
    serialiseTree(node->right, bb);
}

static HuffNode* deserialiseTree(BitBuffer& bb) {
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

// =============================================================================
// SECTION 6: BIG-ENDIAN HELPERS
// =============================================================================

static void writeU64(std::ofstream& o, uint64_t v) {
    for (int i = 7; i >= 0; --i) o.put(static_cast<char>((v >> (8*i)) & 0xFF));
}
static uint64_t readU64(std::ifstream& i) {
    uint64_t v = 0;
    for (int k = 0; k < 8; ++k) v = (v << 8) | static_cast<uint8_t>(i.get());
    return v;
}

// =============================================================================
// SECTION 7: DISPLAY HELPERS
// =============================================================================

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

// =============================================================================
// SECTION 8: ENCODER
// File layout:
//   [8 bytes] total symbol count
//   [1 byte]  padding bit count
//   [N bytes] serialised tree bits + encoded payload, packed together
// =============================================================================

class Encoder {
public:
    static void encode(const std::string& inputPath, const std::string& outputPath) {
        auto t0 = std::chrono::high_resolution_clock::now();

        // Pass 1: count byte frequencies
        std::ifstream in(inputPath, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot open input: " + inputPath);

        std::unordered_map<uint8_t, uint64_t> freqTable;
        uint64_t totalBytes = 0;
        for (int c; (c = in.get()) != EOF; ++totalBytes)
            freqTable[static_cast<uint8_t>(c)]++;
        if (totalBytes == 0) throw std::runtime_error("Input file is empty.");

        // Build tree and code table
        HuffNode* root = TreeBuilder::build(freqTable);
        std::unordered_map<uint8_t, std::string> codeTable;
        TreeBuilder::buildCodeTable(root, "", codeTable);

        // Build entire bit stream: tree shape first, then encoded payload
        BitBuffer bb;
        serialiseTree(root, bb);

        in.clear(); in.seekg(0);
        for (int c; (c = in.get()) != EOF;)
            bb.writeBits(codeTable[static_cast<uint8_t>(c)]);

        int padding = bb.flushWrite();

        // Write output file
        std::ofstream out(outputPath, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open output: " + outputPath);

        writeU64(out, totalBytes);
        out.put(static_cast<char>(padding));
        bb.writeTo(out);
        out.close();

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();

        // Get actual compressed file size
        uint64_t compressedSize = static_cast<uint64_t>(
            std::ifstream(outputPath, std::ios::binary | std::ios::ate).tellg());

        freeTree(root);
        printBox(totalBytes, compressedSize, elapsed);
    }
};

// =============================================================================
// SECTION 9: DECODER
// =============================================================================

class Decoder {
public:
    static void decode(const std::string& inputPath, const std::string& outputPath) {
        auto t0 = std::chrono::high_resolution_clock::now();

        std::ifstream in(inputPath, std::ios::binary);
        if (!in) throw std::runtime_error("Cannot open input: " + inputPath);

        uint64_t totalBytes = readU64(in);
        int      padding    = static_cast<uint8_t>(in.get());

        // Load remaining bytes into bit buffer and deserialise tree
        BitBuffer bb;
        bb.readFrom(in);
        bb.initRead();

        HuffNode* root = deserialiseTree(bb);

        std::ofstream out(outputPath, std::ios::binary);
        if (!out) throw std::runtime_error("Cannot open output: " + outputPath);

        // Walk the tree bit by bit; emit a byte each time we reach a leaf
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
};

// =============================================================================
// SECTION 10: CLI
// =============================================================================

static void printUsage(const char* prog) {
    std::cout << "\nHuffman File Compressor\n"
              << "Usage:\n"
              << "  " << prog << " -c <input_file>  <output.bin>   compress\n"
              << "  " << prog << " -d <input.bin>   <output_file>  decompress\n\n"
              << "Examples:\n"
              << "  " << prog << " -c examples\\sample.txt compressed.bin\n"
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
