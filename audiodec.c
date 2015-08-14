#include "fxlib.h"
#include "sound.h"
#include <stdio.h>
#include "flashfile.h"

static unsigned char *fileptr;

#if 0
unsigned short *flashentry;
unsigned char *flashrp;
unsigned int flashrest = 0;

unsigned long sectortbl[25];

void makesectortbl(void)
{
    int i;
    for (i = 0; i < 25; i++) {
        sectortbl[i] = 0x80000000;
    }
    for (i = 0; i < 24; i++) {
        int sector = *(unsigned short *)(0x8027002a + i * 32);
        int addr   = *(unsigned long  *)(0x80270024 + i * 32) + 0x80000000;
        if (sector != 0xff) {
            sectortbl[sector] = addr;
        }
    }
}

void nextentry(void)
{
    flashentry += 16;
    //flashrp = (unsigned char *)(0x80270000 + flashentry[7] * 0x10000 + flashentry[8]);
    flashrp = (unsigned char *)(sectortbl[flashentry[7]] + flashentry[8]);
    flashrest = flashentry[9] + 1;
}

void flashopen(void)
{
    unsigned short *p = (unsigned short *)0x80270320;
    makesectortbl();
    while (p[0] != 0x5120 ||  p[14] != 'u' || p[15] != 'd') p += 16;
    flashentry = p;
    nextentry();
    {
        char str[100];
        unsigned int key;
        sprintf(str, "%08x %04x", (int)flashrp, flashrest);
        locate(1, 1);
        Print(str);
        sprintf(str, "%08x", (int)fileptr);
        locate(1, 2);
        Print(str);
        GetKey(&key);
    }
}

unsigned char flashgetc(void)
{
    unsigned char r = *flashrp++;
    if (--flashrest == 0) nextentry();
    return r;
}
#endif

static uint8_t audio_filebuf[120];
int audio_fd;
//static unsigned char *fileptr;

// Read 1/30 second of audio (4-bit LPCM @ 7200Hz) from file,
// and copy to DMA buffer with converting to PWM waveform
void audio_dec(void)
{
    static const unsigned long ptn[] = {
        0x00000000,
#if 0
        0x00000001, 0x00010001, 0x00010101, 0x01010101,
        0x01010111, 0x01110111, 0x01111111, 0x11111111,
        0x11111115, 0x11151115, 0x11151515, 0x15151515,
        0x15151555, 0x15551555, 0x15555555, 0x55555555,
        0x55555557, 0x55575557, 0x55575757, 0x57575757,
        0x57575777, 0x57775777, 0x57777777, 0x77777777,
        0x7777777f, 0x777f777f, 0x777f7f7f, 0x7f7f7f7f,
        0x7f7f7fff, 0x7fff7fff, 0x7fffffff, 0xffffffff,
#elif 0
        0x00000001, 0x00000003, 0x00000007, 0x0000000f,
        0x0001000f, 0x0003000f, 0x0007000f, 0x000f000f,
        0x000f010f, 0x000f030f, 0x000f070f, 0x000f0f0f,
        0x010f0f0f, 0x030f0f0f, 0x070f0f0f, 0x0f0f0f0f,
        0x0f0f0f1f, 0x0f0f0f3f, 0x0f0f0f7f, 0x0f0f0fff,
        0x0f1f0fff, 0x0f3f0fff, 0x0f7f0fff, 0x0fff0fff,
        0x0fff1fff, 0x0fff3fff, 0x0fff7fff, 0x0fffffff,
        0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff,
#else
        0x00000001, 0x00000101, 0x00010101, 0x01010101,
        0x01010103, 0x01010303, 0x01030303, 0x03030303,
        0x03030307, 0x03030707, 0x03070707, 0x07070707,
        0x0707070f, 0x07070f0f, 0x070f0f0f, 0x0f0f0f0f,
        0x0f0f0f1f, 0x0f0f1f1f, 0x0f1f1f1f, 0x1f1f1f1f,
        0x1f1f1f3f, 0x1f1f3f3f, 0x1f3f3f3f, 0x3f3f3f3f,
        0x3f3f3f7f, 0x3f3f7f7f, 0x3f7f7f7f, 0x7f7f7f7f,
        0x7f7f7fff, 0x7f7fffff, 0x7fffffff, 0xffffffff,
#endif
    };
    int i;
    uint32_t w;
    uint32_t *p = (uint32_t *)dmasound_get_buf();
    
    //Bfile_ReadFile(audio_fd, audio_filebuf, sizeof(audio_filebuf), -1);
    for (i = 0; i < sizeof(audio_filebuf); i++) {
        //audio_filebuf[i] = *fileptr++;
        //audio_filebuf[i] = flashgetc();
        audio_filebuf[i] = flashfile_getc(audio_fd);
    }
    
    // 1/30 sec = 240 samples @ 7200Hz
    for (i = 0; i < 120; i++) {
        int s = audio_filebuf[i];
        w = ptn[(s >> 4) << 1];
        *p++ = w;
        *p++ = w;
        w = ptn[(s << 1) & 0x1f];
        *p++ = w;
        *p++ = w;
    }
}

int audio_init(void)
{
    // \\fls0\BADAPPLE.aud
    const FONTCHARACTER filename[] = {'\\', '\\', 'f', 'l', 's', '0', '\\', 'B', 'A', 'D', 'A', 'P', 'P', 'L', 'E', '.', 'a', 'u', 'd', '\0'};
    //FONTCHARACTER dummy1[32];
    //FILE_INFO info;
    //Bfile_FindFirst(filename, &audio_fd, dummy1, &info);
    //fileptr = (const unsigned char *)(info.address + 0x80000000);
    //Bfile_FindClose(audio_fd);
    //audio_fd = Bfile_OpenFile(filename, _OPENMODE_READ);
    //flashopen();
    audio_fd = flashfile_open("BADAPPLE.aud");
    return (audio_fd >= 0);
}
