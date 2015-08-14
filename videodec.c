#include <stdlib.h>
#include "fxlib.h"

//#define HUFFDATA_INCLUDED
#define DIRECT_FLASH_FILE_READ

unsigned int bitstream_buf;
int bitstream_buf_len;

#ifdef HUFFDATA_INCLUDED
static const unsigned char huffdata[] = {
#include "huffdata.h"
};
static const unsigned short *huffdata_p = (const unsigned short *)huffdata;

#pragma inline (bitstream_fetch)
static unsigned int bitstream_fetch(int len)
{
    if (bitstream_buf_len < len) {
        bitstream_buf <<= 16;
        bitstream_buf |= *huffdata_p++;
        bitstream_buf_len += 16;
    }
    return bitstream_buf << (32 - bitstream_buf_len) >> (32 - len);
}
#elif defined(DIRECT_FLASH_FILE_READ)
#include "flashfile.h"
static int video_fd;

#pragma inline (bitstream_fetch)
static unsigned int bitstream_fetch(int len)
{
    if (bitstream_buf_len < len) {
        bitstream_buf <<= 16;
        bitstream_buf |= flashfile_getc(video_fd) << 8;
        bitstream_buf |= flashfile_getc(video_fd);
        bitstream_buf_len += 16;
    }
    return bitstream_buf << (32 - bitstream_buf_len) >> (32 - len);
}
#else /* file read via SDK */
static int video_fd;
static unsigned short *filebuf;
#define FILEBUF_SIZE 16384

int bitstream_ptr = FILEBUF_SIZE;

#pragma inline (bitstream_fetch)
static unsigned int bitstream_fetch(int len)
{
    if (bitstream_buf_len < len) {
        bitstream_buf <<= 16;
        if (bitstream_ptr >= FILEBUF_SIZE / 2) {
            Bfile_ReadFile(video_fd, filebuf, FILEBUF_SIZE, -1);
            bitstream_ptr = 0;
        }
        bitstream_buf |= filebuf[bitstream_ptr++];
        bitstream_buf_len += 16;
    }
    return bitstream_buf << (32 - bitstream_buf_len) >> (32 - len);
}
#endif

#pragma inline (bitstream_read)
static unsigned int bitstream_read(int len)
{
    unsigned int r = bitstream_fetch(len);
    bitstream_buf_len -= len;
    return r;
}

#pragma inline (bitstream_skip)
static void bitstream_skip(int len)
{
    bitstream_buf_len -= len;
}

typedef struct tagHuffNode {
    int value;
    struct tagHuffNode *zero;
    struct tagHuffNode *one;
} HuffNode;

// Huffman table
HuffNode      **tbl_node;
unsigned char *tbl_bits;

HuffNode g_node[511];
int n_node = 0;

void make_huff_tbl_sub(unsigned char ptn, int bits, const HuffNode *node)
{
    if (node->zero == 0) {
        int i;
        for (i = 0; i < (1 << (8 - bits)); i++) {
            tbl_node[ptn + i] = node;
            tbl_bits[ptn + i] = bits;
        }
    } else if (bits == 8) {
        tbl_node[ptn] = node;
        tbl_bits[ptn] = 0;
    } else {
        make_huff_tbl_sub(ptn, bits + 1, node->zero);
        make_huff_tbl_sub(ptn | (unsigned char)(1u << (7 - bits)), bits + 1, node->one);
    }
}

int make_huff_tbl(void)
{
    if (!(tbl_node = malloc(sizeof(HuffNode *) * 256))) return 0;
    if (!(tbl_bits = malloc(256))) return 0;
    make_huff_tbl_sub(0, 0, &g_node[0]);
    return 1;
}

void read_huff_tree(void)
{
    if (bitstream_read(1) == 0) {
        g_node[n_node].value = bitstream_read(8);
        g_node[n_node].zero = 0;
        g_node[n_node].one  = 0;
    } else {
        int me = n_node;
        g_node[me].value = 0;
        g_node[me].zero = &g_node[++n_node];
        read_huff_tree();
        g_node[me].one = &g_node[++n_node];
        read_huff_tree();
    }
}

unsigned char huff_dec(void)
{
    HuffNode *node;
    int ptn = bitstream_fetch(8);
    if (tbl_bits[ptn] > 0) {
        // Huffman code length <= 8 bits
        bitstream_skip(tbl_bits[ptn]);
        return tbl_node[ptn]->value;
    } else {
        // Huffman code length > 8 bits
        bitstream_skip(8);
        node = tbl_node[ptn];
        do {
            if (bitstream_read(1) == 0) {
                node = node->zero;
            } else {
                node = node->one;
            }
        } while (node->zero != 0);
        return node->value;
    }
}


unsigned char fb[1024] = {0};

void initfb(void)
{
    int i;
    for (i = 0; i < 1024; i++) {
        fb[i] = 0xff;
    }
}

void makefb(void)
{
    int x, y, by;
    for (x = 2; x < 14; x ++) {
        int m = huff_dec();
        for (by = 0; by < 8; by++) {
            if ((m & (1u << by)) != 0) {
                unsigned char *p = fb + (by << 7) + x;
                for (y = 0; y < 128; y += 16) {
                    p[y] ^= huff_dec();
                }
            }
        }
    }
}

void draw(void)
{
    unsigned char *cmd  = (unsigned char *)0xb4000000;
    unsigned char *data = (unsigned char *)0xb4010000;
    const unsigned char *src = fb;
    int i, j;
    for (i = 0; i < 64; i++) {
        *cmd  = 4;
        *data = 0xc0 | i;
        *cmd  = 1;
        *data = 1;
        *cmd  = 4;
        *data = 0;
        *cmd  = 7;
        for (j = 16; j > 0; j--) {
            *data = *src++;
        }
    }
}

int video_init(void)
{
    // setup for read data file
#ifdef HUFFDATA_INCLUDED
#elif defined(DIRECT_FLASH_FILE_READ)
    video_fd = flashfile_open("BADAPPLE.vid");
    if (video_fd < 0) return 0;
#else /* file read via SDK */
    // \\fls0\BADAPPLE.vid
    const FONTCHARACTER filename[] = {'\\', '\\', 'f', 'l', 's', '0', '\\', 'B', 'A', 'D', 'A', 'P', 'P', 'L', 'E', '.', 'v', 'i', 'd', '\0'};
    filebuf = malloc(FILEBUF_SIZE);
    if (!filebuf) return 0;
    video_fd = Bfile_OpenFile(filename, _OPENMODE_READ);
    if (video_fd < 0) return 0;
#endif
    
    read_huff_tree();
    if (!make_huff_tbl()) return 0;
    
    initfb();
    return 1;
}
