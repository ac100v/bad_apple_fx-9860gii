// 
#include "fxlib.h"
#include <stdlib.h>
#include "sound.h"

// SCIF0
#define SCSMR  (*(volatile short *)0xa4410000)
#define SCBRR  (*(volatile char  *)0xa4410004)
#define SCSCR  (*(volatile short *)0xa4410008)
#define SCFTDR (*(volatile char  *)0xa441000c)
#define SCFSR  (*(volatile short *)0xa4410010)
#define SCFRDR (*(volatile char  *)0xa4410014)
#define SCFCR  (*(volatile short *)0xa4410018)
#define SCFDR  (*(volatile short *)0xa441001c)
#define SCLSR  (*(volatile short *)0xa4410024)

// DMA0 ch0
#define DMA0_SAR_0  (*(volatile unsigned long *)0xfe008020)
#define DMA0_DAR_0  (*(volatile unsigned long *)0xfe008024)
#define DMA0_TCR_0  (*(volatile unsigned long *)0xfe008028)
#define DMA0_CHCR_0 (*(volatile unsigned long *)0xfe00802c)

#define DMA0_DMAOR  (*(volatile unsigned short *)0xfe008060)
#define DMA0_DMARS0 (*(volatile unsigned short *)0xfe009000)


#define MSTPCR0 (*(volatile unsigned long *)0xa4150030)

#define DEF_SYSCALL(id, rettype, name, params) \
    const int sc_##id[] = {0xd201d002, 0x422b0009, 0x80010070, 0x##id}; \
    rettype (*name)params = (rettype (*)params)sc_##id;

DEF_SYSCALL(02ab, int, Serial_Open2, (unsigned short))
DEF_SYSCALL(0409, void *, Serial_ResetAndDisable, (void))
DEF_SYSCALL(040f, int, Serial_BufferedTransmitNBytes, (unsigned char *, int))
DEF_SYSCALL(0412, int, Serial_GetFreeTransmitSpace, (void))
DEF_SYSCALL(0419, int, Serial_Close, (int))
DEF_SYSCALL(06c4, int, Keyboard_PRGM_GetKey, (unsigned char *))


int PRGM_GetKey(void)
{
    unsigned char buffer[12];
    Keyboard_PRGM_GetKey(buffer);
    return (buffer[1] & 0x0f) * 10 + (buffer[2] >> 4);
}

// FIXME: It works only fx-9860GII (OS 02.04.0201).
// Memory map should be changed in other model or version.
static uint32_t vadr_to_padr(void *vadr)
{
    if (((uint32_t)vadr >= 0x08100000) && ((uint32_t)vadr < 0x08108000)) {
        // RAM
        return (uint32_t)vadr + (0x0801c000 - 0x08100000);
    } else if (((uint32_t)vadr >= 0x80000000) && ((uint32_t)vadr < 0xc0000000)) {
        // P2,P3
        return (uint32_t)vadr & 0x1fffffff;
    } else if (((uint32_t)vadr >= 0x00300000) && ((uint32_t)vadr < 0x00800000/* ?? */)) {
        // ROM
        // look up padr of 0x0030_0000 from TLB
        uint32_t offset = *(uint32_t *)0xf7000000 & 0x1ffff000;
        return (uint32_t)vadr + (offset - 0x00300000);
    }
    return 0xffffffff;
}

/**********************************************************************/

void scif_init(void)
{
    Serial_ResetAndDisable();
    Serial_Open2(4); // 115200bps
    MSTPCR0 &= ~(1u << 9); // SCIF0 clock ON
    // SH7730 22.5(c)
    SCSCR = 0; // clear TE, RE
    SCFCR = 0x06; // reset FIFO
    // SCFSR
    SCLSR = 0; // clear ORER
    SCSCR = 1; // use internal clock
    SCSMR = 0x80; // synchronized mode, clock = P_phi
    SCFCR &= ~0x06; // unreset FIFO
    SCBRR = 3+4; // bitrate
    SCSCR = 0xf1; // set TE, RE
}

void scif_uninit(void)
{
    MSTPCR0 |= 1u << 9; // SCIF0 clock OFF
}

/**********************************************************************/

void dma_init(void)
{
    MSTPCR0 &= ~(1u << 21); // DMAC0 clock ON
    
    DMA0_CHCR_0 &= ~3u; // clear DE, TE
    DMA0_DMAOR &= ~1u;
    
    DMA0_DAR_0 = (unsigned long)&SCFTDR;
    DMA0_CHCR_0 = (0u << 30)  // LCKN: バス権解放禁止
                | (0u << 25)  // RPT: 通常モード
                | (1u << 24)  // DA: DREQを同期信号としてサンプリング
                | (0u << 23)  // DO
                | (0u << 20)  // TS[3:2]: バイト単位
                | (1u << 19)  // HE
                | (0u << 18)  // HIE
                | (0u << 17)  // AM
                | (0u << 16)  // AL
                | (0u << 14)  // DM: dstアドレス固定
                | (1u << 12)  // HE: srcアドレス増加
                | (8u <<  8)  // RS: 転送要求元をDMA拡張リソースセレクタで選択
                | (0u <<  6)  // DL,DS: 内蔵周辺モジュールへのリクエストでは無視
                | (0u <<  5)  // TB: サイクルスチールモード
                | (0u <<  3)  // TS[1:0] 
                | (0u <<  2)  // IE: 割り込み禁止
                | (1u <<  1)  // TE: トランスファエンドフラグクリア
                | (0u <<  0); // DE: DMA禁止
    DMA0_DMARS0 = 0x00;
    DMA0_DMARS0 = 0x21; // destination = SCFTDR0
}


void dma_uninit(void)
{
    MSTPCR0 |= 1u << 21; // DMAC0 clock OFF
}

void dma_run(uint32_t sar, uint32_t tcr)
{
    DMA0_SAR_0 = sar;
    DMA0_TCR_0 = tcr;
    DMA0_DMAOR = 1u; // DMA enabled
    DMA0_CHCR_0 |= 1u; // DE=1
}

void dma_stop(void)
{
    DMA0_CHCR_0 &= ~3u; // DE=0
    DMA0_DMAOR = 0u; // DMA disabled
}

uint32_t dma_get_remain(void)
{
    if ((DMA0_CHCR_0 & 1) == 0) return 0;
    return DMA0_TCR_0;
}


/**********************************************************************/

uint32_t *dmabuf_area;
uint32_t dmabuf_front; // padr of audio DMA buffer (foreground)
uint32_t dmabuf_back;  // padr of audio DMA buffer (background)
uint32_t dmabuf_size;

int dmasound_init(int bufsize)
{
    // double-buffer for DMA
    dmabuf_area = malloc(bufsize * 2 + 32);
    if (!dmabuf_area) return 0;
    // DMA buffer is 32-byte aligned to avoid conflicting to cache.
    dmabuf_front = (vadr_to_padr(dmabuf_area) + 31) & ~31;
    dmabuf_back  = dmabuf_front + bufsize;
    dmabuf_size = bufsize;
    // FIXME: flushing cache
    
    scif_init();
    dma_init();
    
    return 1;
}

void dmasound_uninit(void)
{
    free(dmabuf_area);
    dmabuf_front = 0;
    dmabuf_back = 0;
    dmabuf_size = 0;
    
    scif_uninit();
    dma_uninit();
}

void *dmasound_get_buf(void)
{
    // convert padr to vadr (P2 area, non-cachable)
    return (void *)(dmabuf_back + 0xa0000000);
}

void dmasound_play(void)
{
    uint32_t tmp;
    dma_stop();
    dma_run(dmabuf_back, dmabuf_size);
    tmp = dmabuf_back;
    dmabuf_back = dmabuf_front;
    dmabuf_front = tmp;
}

int dmasound_get_remain(void)
{
    return dma_get_remain();
}
