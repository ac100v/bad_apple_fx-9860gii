int flashfile_open(const char *filename);
void flashfile_close(int fd);
int flashfile_eof(int fd);
int flashfile_getc(int fd);
