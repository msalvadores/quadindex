#ifndef _RDF_KB_CRLIST_H  /* duplication check */
#define _RDF_KB_CRLIST_H

#include <glib.h>

#include "common/hash.h"

#define ALLOC_SLOT_INVERSE_LIST 1024
#define ALLOC_SLOT_INVERSE_LIST_DATA 8192

typedef struct {
    guint id; /* segment id */

} crs_matrix_list;

typedef struct {
    guint last;
    size_t alloc_len;
    fs_rid *data;
    int sorted;
} crs_matrix_inverse_list;


typedef struct {

    unsigned char *keys; //sorted (rid,len)
    guint key_i;
    guint keys_alloc;

    unsigned char *data; //ints
    size_t data_offset_last;
    size_t data_alloc;

} crs_list_collapsed;

typedef struct {
    crs_list_collapsed *ic;
    guint key_i;
    fs_rid key;
    guint len;
    guint i;
    guint val;
    guint *pdata;
} crs_list_collapsed_iterator;

void crs_matrix_inverse_list_append(crs_matrix_inverse_list *il,fs_rid c,fs_rid v);
void crs_matrix_inverse_list_free(crs_matrix_inverse_list *il);
crs_matrix_inverse_list *crs_matrix_inverse_list_new();
void crs_matrix_inverse_list_sort(crs_matrix_inverse_list *il);
crs_list_collapsed* crs_list_collapsed_new(crs_matrix_inverse_list *il);

crs_list_collapsed_iterator *crs_list_collapsed_iterator_new();
void crs_list_collapsed_iterator_free(crs_list_collapsed_iterator *it);
int crs_list_collapsed_iterator_next(crs_list_collapsed_iterator *it);
#endif
