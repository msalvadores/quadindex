#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

//cc -Wall -g `pkg-config --cflags glib-2.0`  `pkg-config --libs glib-2.0`  mytest.c -o mytest -ltcmalloc

int main(int argc, char *argv[]) {
    printf("%d\n",sizeof(int));
    GPtrArray *arr =  g_ptr_array_new();
    int i,j;
    int *x;
    for (j=0;j<500;j++) {
    for (i=0;i<100000;i++) {
        x=g_malloc(sizeof(int));
        *x=i;
        g_ptr_array_add(arr,x);
    }
    }
    exit(1);
}
