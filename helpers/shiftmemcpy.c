
/* memcpy example */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>




int main(int argc,char **argv) {
        int *a = malloc(1024 * sizeof(int));
        for (int i=0;i<512;i++)
                a[i]=i;
        unsigned char *c = (unsigned char *)a; 
        memcpy(c + (65 * sizeof(int)),c,(512 - 65) * sizeof(int));
        for(int i=0; i < 512 + 65;i++)
                printf("%i --> %i\n",i,a[i]);
}

