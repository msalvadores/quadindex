#include <raptor.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

//cc -Wall -g -std=gnu99 -O2 `pkg-config --cflags glib-2.0 gthread-2.0` -multiply_defined suppress  `pkg-config --libs glib-2.0`  dumb.c -o dumb

int main(int argc,char** args) {
  
    size_t ps = sysconf (_SC_PAGESIZE);
    size_t file_size;
    //size_t msize = 1 * ps;
    int fd;
    unsigned char *data;
    unsigned char *end_data;
    fd = open("mmap_data.bin", O_RDWR | O_CREAT, (mode_t) 0600);
    file_size = lseek(fd,0,SEEK_END);
    printf("file size %lu\n",file_size);
    data = mmap(0, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    end_data = data + file_size;

    guint *udata = (guint *)data;
    guint x=0;
    while((data + (x * sizeof(guint))) <= end_data) {
         printf("x[%u]=%u\n",x,udata[x]); 
         x++;
    }
    munmap(data, file_size);
    close(fd);
    exit(1);
}
