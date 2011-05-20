#ifndef _RDF_KB_CRLIST_H  /* duplication check */
#define _RDF_KB_CRLIST_H

#include <glib.h>

#include "common/hash.h"

#define ALLOC_SLOT_INVERSE_LIST 1024

typedef struct {
    guint id; /* segment id */

} crs_matrix_list;

typedef struct {
    guint last;
    guint alloc_len;
    fs_rid *data;
} crs_matrix_inverse_list;

void crs_matrix_inverse_list_append(crs_matrix_inverse_list *il,fs_rid c,fs_rid r,fs_rid v);
void crs_matrix_inverse_list_free(crs_matrix_inverse_list *il);
crs_matrix_inverse_list *crs_matrix_inverse_list_new();

#endif
