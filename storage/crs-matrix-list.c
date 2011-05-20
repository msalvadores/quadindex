#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "crs-matrix-list.h"
#include "crs-matrix.h"
#include "common/hash.h"
#include "common/error.h"

void crs_matrix_list_build(crs_matrix *matrix) {
    
}

crs_matrix_inverse_list *crs_matrix_inverse_list_new() {
    crs_matrix_inverse_list *res = malloc(sizeof(crs_matrix_inverse_list));
    res->alloc_len=ALLOC_SLOT_INVERSE_LIST;
    res->data = calloc(ALLOC_SLOT_INVERSE_LIST, 3 * sizeof(fs_rid));
    res->last = 0;
    return res;
}

void crs_matrix_inverse_list_append(crs_matrix_inverse_list *il,fs_rid c,fs_rid r,fs_rid v) {
    if (il->last == il->alloc_len) {
        realloc(il->data, (il->alloc_len + ALLOC_SLOT_INVERSE_LIST) * 3 *  sizeof(fs_rid));
        il->alloc_len += ALLOC_SLOT_INVERSE_LIST;
    }
    il->data[il->last*3]=c;
    il->data[(il->last*3)+1]=r;
    il->data[(il->last*3)+1]=v;
    il->last++;
}


void crs_matrix_inverse_list_free(crs_matrix_inverse_list *il) {
    free(il->data);
    free(il);
}
