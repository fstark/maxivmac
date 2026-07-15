/*
 * huffman.c — StuffIt Huffman-only decompressor (method 3)
 *
 * Ported from XADMaster XADStuffItHuffmanHandle.m
 * (The Unarchiver) by MacPaw Inc.  Licensed under LGPL 2.1.
 *
 * The Huffman tree is embedded in the stream header:
 * - Bit 1 followed by 8-bit byte value = leaf node
 * - Bit 0 = internal node (recurse left then right)
 *
 * Decoding reads MSB-first bits and walks the tree.
 */

#include "unsit.h"

/* ── Tree ────────────────────────────────────────── */

#define HUFF_MAX_NODES 512

typedef struct {
    int branches[2];  /* child node index, or -(value+2) for leaf */
} HNode;

static HNode huff_nodes[HUFF_MAX_NODES];
static int   huff_nnodes;

#define H_IS_LEAF(n) (huff_nodes[n].branches[0] == huff_nodes[n].branches[1])
#define H_LEAF_VAL(n) (-(huff_nodes[n].branches[0]) - 2)

static int huff_alloc(void)
{
    int n = huff_nnodes++;
    if (n >= HUFF_MAX_NODES) return 0;
    huff_nodes[n].branches[0] = -1;
    huff_nodes[n].branches[1] = -1;
    return n;
}

/* Parse the tree recursively from the MSB bitstream */
static void huff_parse_tree(BitReader *br, int node)
{
    if (br_bit(br) == 1) {
        /* Leaf: read 8-bit value */
        int val = (int)br_bits(br, 8);
        huff_nodes[node].branches[0] = -(val + 2);
        huff_nodes[node].branches[1] = -(val + 2);
    } else {
        /* Internal node: left then right */
        int left = huff_alloc();
        int right = huff_alloc();
        huff_nodes[node].branches[0] = left;
        huff_nodes[node].branches[1] = right;
        huff_parse_tree(br, left);
        huff_parse_tree(br, right);
    }
}

/* Decode one symbol by walking the tree with MSB bits */
static int huff_decode(BitReader *br)
{
    int node = 0;
    while (!H_IS_LEAF(node)) {
        int bit = br_bit(br);
        node = huff_nodes[node].branches[bit];
        if (node < 0) return -1;
    }
    return H_LEAF_VAL(node);
}

/* ── Main decompressor ───────────────────────────── */

int decompress_huffman(UFile *uf, u8 *outbuf, s32 length)
{
    BitReader br;
    s32 outpos = 0;

    br_init(&br, uf);

    /* Build tree from stream header */
    huff_nnodes = 1;
    huff_nodes[0].branches[0] = -1;
    huff_nodes[0].branches[1] = -1;
    huff_parse_tree(&br, 0);

    /* Decode symbols */
    while (outpos < length) {
        int val = huff_decode(&br);
        if (val < 0) return -1;
        outbuf[outpos++] = (u8)val;
    }

    return 0;
}
