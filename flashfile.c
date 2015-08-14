#define MAX_FILE 4

#define FILE_STATE_CLOSED 0
#define FILE_STATE_OPENED 1
#define FILE_STATE_EOF    3

typedef struct {
    unsigned short signature;
    unsigned short serial_number;
    unsigned short unknown[3];
    unsigned short total_area;
    unsigned short seq_area;
    unsigned short sector_id;
    unsigned short start;
    unsigned short size_minus_1;
    unsigned short unknown3[6];
} area_info;

static struct {
    const unsigned char *rp;
    const area_info *area;
    unsigned short sector_rest;
    unsigned char state;
} file[MAX_FILE];

static unsigned char *sectortbl[32];
static char flashfile_initialized_flag;

static void flashfile_init(void)
{
    int i;
    // create sector->address table by reading sector-directory
    for (i = 0; i < 24; i++) {
        int sector = *(unsigned short *)(0x8027002a + i * 32);
        if (sector < 32) {
            int addr = *(unsigned long *)(0x80270024 + i * 32);
            sectortbl[sector] = (unsigned char *)(addr + 0x80000000);
        }
    }
    flashfile_initialized_flag = 1;
}

static void flashfile_first_sector(int fd)
{
    const area_info *p = file[fd].area;
    file[fd].rp = sectortbl[p->sector_id] + p->start;
    file[fd].sector_rest = p->size_minus_1;
}

static void flashfile_next_sector(int fd)
{
    const area_info *p = file[fd].area;
    if (p->total_area == p->seq_area) {
        file[fd].state = FILE_STATE_EOF;
    } else {
        file[fd].area++;
        flashfile_first_sector(fd);
    }
}

int flashfile_open(const char *filename)
{
    int fd;
    int i;
    unsigned short str[12];
    const unsigned short *file_directory = (const unsigned short *)0x80270320;
    
    // initialize
    if (!flashfile_initialized_flag) {
        flashfile_init();
    }
    
    // find unused fd
    for (fd = 0; fd < MAX_FILE; fd++) {
        if (file[fd].rp == 0) break;
    }
    if (fd == MAX_FILE) {
        return -1; // maximum number of files have already opened
    }
    
    // convert ASCIIZ to FONTCHARACTER[]
    for (i = 0; i < 12; i++) {
        if (filename[i] == '\0') break;
        str[i] = filename[i];
    }
    for (; i < 12; i++) {
        str[i] = 0xffff;
    }
    
    // find file
    for (;; file_directory += 16) {
        if (file_directory[0] == 0xffff) {
            return -1; // file not found
        }
        if (file_directory[0] != 0x5120) continue;
        // found filename_info
        for (i = 0; i < 12; i++) {
            if (file_directory[i+4] != str[i]) break;
        }
        if (i == 12) break; // file found
    }
    
    file[fd].area = (const area_info *)(file_directory + 16);
    file[fd].state = FILE_STATE_OPENED;
    flashfile_first_sector(fd);
    return fd;
}

void flashfile_close(int fd)
{
    if (fd >= 0 && fd < MAX_FILE) {
        file[fd].state = FILE_STATE_CLOSED;
    }
}

int flashfile_eof(int fd)
{
    if (fd < 0 || fd >= MAX_FILE) return 1;
    return (file[fd].state != FILE_STATE_OPENED);
}


int flashfile_getc(int fd)
{
    unsigned char r;
    if (file[fd].state != FILE_STATE_OPENED) {
        return -1;
    }
    r = *file[fd].rp++;
    if (file[fd].sector_rest-- == 0) {
        flashfile_next_sector(fd);
    }
    return r;
}


