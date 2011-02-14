
#include <raptor.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <glib.h>
//#include <google/heap-profiler.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

//cc -Wall -g -std=gnu99 -O2 `pkg-config --cflags glib-2.0 gthread-2.0` -multiply_defined suppress  `pkg-config --libs glib-2.0`  testmmap.c -o testmmap

int main(int argc,char** args) {
  
    size_t ps = sysconf (_SC_PAGESIZE);
    size_t file_size;
    //size_t msize = 1 * ps;
    size_t msize = 100 * sizeof(guint);
    int fd;
    unsigned char *data;
    fd = open("mmap_data.bin", O_RDWR | O_CREAT, (mode_t) 0600);
    file_size = lseek(fd,0,SEEK_END);
    printf("file size %lu\n",file_size);
    if (file_size < msize) {
        ftruncate(fd,msize);
    }
    data = mmap(0, msize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    guint *udata = (guint *)data;
    for (guint x=0;x<110;x++) {
        udata[x]=udata[x]+1000;
    }
    //msync(data, msize, MS_ASYNC);
#if 1
{ printf("to close. press enter\n");
char foo;
read(0, &foo, 1); }
#endif 
    char **x = NULL;
    printf("no llega %s\n",*x);
    close(fd);
    exit(1);
}
