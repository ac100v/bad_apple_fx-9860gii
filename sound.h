typedef signed char    int8_t;
typedef signed short   int16_t;
typedef signed long    int32_t;
typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long  uint32_t;

int PRGM_GetKey(void);

void scif_init(void);
void scif_uninit(void);
void dma_init(void);
void dma_uninit(void);
void dma_run(uint32_t sar, uint32_t tcr);
void dma_stop(void);
uint32_t dma_get_remain(void);
int dmasound_init(int bufsize);
void dmasound_uninit(void);
void *dmasound_get_buf(void);
void dmasound_play(void);
int dmasound_get_remain(void);
