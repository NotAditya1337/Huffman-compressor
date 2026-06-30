# Huffman File Compressor

A command-line lossless file compression tool built from scratch in C++, implementing the Huffman Coding algorithm. Uses only the C++ Standard Library — no external dependencies.

---

## Demo

```
$ ./huffman -c examples/sample.txt examples/compressed.bin

  +---------------------------------------+
  |        COMPRESSION COMPLETE           |
  +---------------------------------------+
  | Original size  :       2472 bytes     |
  | Compressed size:       1541 bytes     |
  | Compression    :       37.7%          |
  | Time taken     :     0.0004s          |
  +---------------------------------------+

$ ./huffman -d examples/compressed.bin examples/restored.txt

  +---------------------------------------+
  |       DECOMPRESSION COMPLETE          |
  +---------------------------------------+
  | Restored size  :       2472 bytes     |
  | Time taken     :     0.0001s          |
  +---------------------------------------+

$ diff examples/sample.txt examples/restored.txt
# no output — files are byte-for-byte identical
```

---

## How It Works

Standard ASCII encoding gives every character exactly 8 bits — whether it appears once or ten thousand times. Huffman coding exploits character frequency to assign shorter codes to common characters and longer codes to rare ones.

### The Algorithm (3 Steps)

**Step 1 — Frequency Count**
Scan the entire file and count how often each byte value appears.

**Step 2 — Build the Huffman Tree (Greedy Algorithm)**
- Push all byte values into a **min-heap** sorted by frequency.
- Repeatedly extract the two nodes with the lowest frequency.
- Merge them into an internal node whose frequency is their sum.
- Push the merged node back. Repeat until one node remains — the **root**.

```
Bytes: [a:2] [b:3] [c:4]

Round 1: merge a:2 + b:3 -> node:5
Heap: [c:4] [node:5]

Round 2: merge c:4 + node:5 -> root:9

        root:9
       /      \
     c:4     node:5
             /    \
           a:2   b:3

Codes:  c -> 0   (1 bit)
        a -> 10  (2 bits)
        b -> 11  (2 bits)
```

**Step 3 — Encode**
Replace every byte in the file with its Huffman code. Write the packed bits to disk along with a serialised copy of the tree so the decoder can reconstruct it exactly.

### Why It's Lossless
The tree uses **prefix-free codes** — no code is a prefix of another. This means decoding is completely unambiguous: walk the tree bit by bit, and every time you reach a leaf you have exactly one byte. No separators needed.

---

## File Format

```
[ 8 bytes ] total symbol count
[ 1 byte  ] padding bit count
[ N bytes ] serialised Huffman tree (preorder) + encoded payload, packed as bits
```

The tree is serialised directly into the bit stream (not as a frequency table), so the decoder reconstructs the **identical** tree regardless of how equal-frequency nodes are ordered.

---

## Data Structures Used

| Structure | Role | Why |
|---|---|---|
| Binary Tree (`HuffNode`) | Huffman Tree | Stores prefix-free codes |
| Min-Heap (`std::priority_queue`) | Tree construction | Greedy extraction of two lowest-frequency nodes in O(log n) |
| Hash Map (`std::unordered_map`) | Frequency table + code table | O(1) lookup per byte |
| Bit Buffer (`std::vector<uint8_t>`) | I/O | Packs individual bits into bytes for disk write |

**Time Complexity:** O(n log k) — n = file size, k = unique bytes (≤ 256)
**Space Complexity:** O(k) for the tree and code table

---

## Build and run

Requires a C++17 compiler (g++ or clang++) and `make`.

```bash
make                  # builds the huffman executable
make test             # builds (if needed) and runs a sample compress/decompress
make clean            # removes build artifacts

./huffman -c <input_file> <output.bin>     # compress
./huffman -d <input.bin> <output_file>     # decompress
```

### Examples
```bash
./huffman -c examples/sample.txt  examples/compressed.bin
./huffman -d examples/compressed.bin examples/restored.txt

./huffman -c report.pdf           report.bin
./huffman -d report.bin           report_restored.pdf
```

---

## Performance

| File | Original | Compressed | Ratio |
|---|---|---|---|
| `examples/sample.txt` (English text) | 2,472 bytes | 1,541 bytes | **37.7%** |
| `examples/sample.html` (markup) | 2,868 bytes | 1,933 bytes | **32.6%** |
| Already compressed (.zip, .jpg) | — | larger | not suitable |

> Huffman coding works best on files with non-uniform byte distribution (text, source code, markup, CSV).
> Binary files and already-compressed formats see little or no benefit.

---

## Project Structure

```
huffman_project/
├── include/
│   └── Huffman.hpp     — public interface (HuffNode, TreeBuilder, BitBuffer, Encoder, Decoder)
├── src/
│   ├── Huffman.cpp     — implementation (tree building, bit packing, encode/decode)
│   └── main.cpp        — CLI argument parsing
├── examples/
│   ├── sample.txt       — test file for demo
│   └── sample.html      — second test file (markup)
├── Makefile
└── README.md
```

Interface (`.hpp`) and implementation (`.cpp`) are split deliberately: `main.cpp` only depends on what `Huffman.hpp` exposes, and has no knowledge of how the tree or bit packing actually work internally.

---

## Key Concepts

- **Greedy algorithm** — merging the two minimum-frequency nodes at each step is locally optimal and produces the globally optimal prefix-free code tree (proven by Huffman, 1952)
- **Prefix-free codes** — guarantee unambiguous decoding without any delimiters
- **Tree serialisation** — the tree shape is written into the file header using a preorder traversal (internal node -> `0`, leaf -> `1` + byte value), so the decoder never needs to rebuild from frequencies
- **Lossless compression** — every bit of the original is perfectly reconstructed; contrast with lossy formats like JPEG or MP3

---

## Possible extensions

- Adaptive Huffman coding (single-pass, no upfront frequency scan)
- Canonical Huffman codes for a smaller serialised tree
- Benchmark mode comparing against zlib/DEFLATE on the same input
- Directory/archive mode to compress multiple files into one output
