#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


typedef unsigned long long int fs_rid;

void quickaux(fs_rid *arr,int start, int end) {
    if (start < end) {
        fs_rid pivot = arr[end];
        int i = start;
        int j = end;
        while (i != j) { 
            if (arr[i] < pivot) { 
                i = i + 1; 
            } 
            else { 
                arr[j] = arr[i]; 
                arr[i] = arr[j-1]; 
                j = j - 1; 
            } 
      } 
      arr[j] = pivot;
      quickaux(arr, start, j-1); 
      quickaux(arr, j+1, end); 
    }
}

void quicksort(fs_rid *arr,int len) {
    quickaux(arr,0,len - 1);
}

int main(int argc,char *arcgv) {
    fs_rid arr[] = { 0xe2fc20bed41cc1d7, 0xe2fc20bed41cc1d6, 0xe2fc20bed41cc1d9, 0xe2fc20bed41cc1d5,0xe2fc20bed41cc1da};
    
    quicksort(arr,4);
    int i=0;
    for (i=0;i<4;i++) {
        printf("%llx\n",arr[i]);
    }
    return 0;
}
