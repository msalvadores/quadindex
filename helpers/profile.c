#include <raptor.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <glib.h>
//#include <google/heap-profiler.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "common/hash.h"
#include <fcntl.h>

#define N 400 * 1024
#define B 2048
//cc -Wall -g -std=gnu99 -O2 `pkg-config --cflags glib-2.0 gthread-2.0` -ltcmalloc  -bind_at_load -multiply_defined suppress  `pkg-config --libs glib-2.0`  profile.c -o profile

void *some_not_free_memory() {
    void *x=malloc(B);
    memset(x,B,1);
    return x;
}

void *some_free_memory() {
    void *x=malloc(B);
    memset(x,B,1);
    return x;
}

int main(int argc,char** args) {
    printf("size of guint %lu\n",sizeof(guint));
    printf("size of char %lu\n",sizeof(char));
    printf("size of size_t %lu\n",sizeof(size_t));
    printf("size of fs_rid %lu\n",sizeof(fs_rid));
    printf("size of ulli %lu\n",sizeof(unsigned long long int));
    int x = -1;
    if (x)
        printf("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

    printf("size of page %lu\n",sysconf(_SC_PAGE_SIZE));
    return 0;

    size_t file_size;
    int fd;
    unsigned char *data;
    guint rowptr_len = 10;
    guint rowlist_len = 5;
    guint colind_len = 20;

    size_t FILESIZE =  (3 * sizeof(guint)) + (rowptr_len * sizeof(guint)) + (rowlist_len * sizeof(fs_rid)) + (colind_len * sizeof(fs_rid));
    fd = open("mmap_data.bin", O_RDWR | O_CREAT, (mode_t) 0600);
    file_size = lseek(fd,0,SEEK_END);
    printf("current file size %lu\n",file_size);
    if (!file_size) {
        lseek(fd, FILESIZE-1, SEEK_SET);
        write(fd, "", 1);
        data = mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        guint *counters = (guint *)data;
        counters[0] = rowptr_len;
        counters[1] = rowlist_len;
        counters[2] = colind_len;

        guint *rowptr = (guint *)(data + (3 * sizeof(guint)));
        for (guint i=0;i<10;i++)
            rowptr[i]=i;

        fs_rid *rowlist = (fs_rid *) (data + (10 * sizeof(guint)) + (3 * sizeof(guint)));
        rowlist[0]=0xcc91e31785d18782;
        for (guint i=1;i<4;i++)
            rowlist[i]=0x3e32703c203e3173;
        rowlist[4]=0xe95e3998e1fb6613;
        
        fs_rid *colind = (fs_rid *) (data + (10 * sizeof(guint)) + (5 * sizeof(fs_rid)) + (3 * sizeof(guint)));
        for (guint i=0;i<20;i++)
            colind[i]=0x3e32703c203e3173;
        colind[9]=0xe95e3998e1fb6613;
        colind[15]=0xcc91e31785d18782;
        munmap(data, FILESIZE);
        close(fd);
    } else {
        data = mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        guint *counters = (guint *)data;
        rowptr_len = counters[0];
        rowlist_len = counters[1];
        colind_len = counters[2];
        guint *rowptr = (guint *)(data + (3 * sizeof(guint)));
        for (guint i=0;i<rowptr_len;i++)
            printf("%u %u\n",i,rowptr[i]);
        fs_rid *rowlist = (fs_rid *) (data + (10 * sizeof(guint)) + (3 * sizeof(guint)));
        for (guint i=0;i<rowlist_len;i++)
            printf("%u %llx\n",i,rowlist[i]);
        fs_rid *colind = (fs_rid *) (data + (10 * sizeof(guint)) + (5 * sizeof(fs_rid)) + (3 * sizeof(guint)));
        for (guint i=0;i<colind_len;i++)
            printf("%u %llx\n",i,colind[i]);
        colind_len++;
        munmap(data, FILESIZE);
        FILESIZE =  (3 * sizeof(guint)) + (rowptr_len * sizeof(guint)) + (rowlist_len * sizeof(fs_rid)) + (colind_len * sizeof(fs_rid));
        lseek( fd, FILESIZE, SEEK_SET );
        close(fd);
   }
#if 0
    GArray* arr = g_array_new(FALSE, FALSE, sizeof(guint));
    printf("len %u \n",arr->len);
    for(guint x=0;x<10;x++)
        g_array_append_val(arr,x);
    g_array_set_size(arr,12);
    int h = 11;
    arr->data[h]=1011;
    ((guint *)arr->data)[10]=1010;
    for(guint x=0;x<arr->len;x++)
        printf("%u --> %u \n",x,g_array_index(arr,guint,x));
    return 0;

    for(guint x=0;x<10;x++)
        g_array_append_val(arr,x);

    for(guint x=5;x<arr->len;x++)
        ((guint *)arr->data)[x] = 100 + x;

    guint g=99;
    g_array_insert_val(arr,5,g);

    for(guint x=0;x<10;x++)
        printf("%u --> %u \n",x,g_array_index(arr,guint,x));

   return 0;
   GPtrArray* ptra = g_ptr_array_new();

   guint vals[] = {0,1,2,3,4};

    guint z = 10000;

    for(guint x=0;x<5;x++)
        g_ptr_array_add(ptra,&vals[x]);

    for(guint x=0;x<5;x++)
        printf("!!!! %u --> %u \n",x,*((guint *)g_ptr_array_index(ptra,x)));

    g_ptr_array_add(ptra,&z);

    for(guint x=ptra->len-1;x>2;x--)
         ptra->pdata[x]=ptra->pdata[x-1];
    
    ptra->pdata[2] = &z;

    for(guint x=0;x<ptra->len;x++)
        printf("with z %u --> %u \n",x,*((guint *)g_ptr_array_index(ptra,x)));

    return 0;
    void *f[N];
    void *n;
    for(int i=0;i<N;i++) {
       f[i] = some_free_memory();
       n = some_not_free_memory();
    }
    printf("freeing some ... \n");
    for(int i=0;i<N;i++) {
        free(f[i]);
    }
#endif
    exit(1);
}
