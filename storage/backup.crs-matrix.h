#ifndef _RDF_KB_CRS_H  /* duplication check */
#define _RDF_KB_CRS_H

#include <glib.h>

#include "common/hash.h"

/* ERROR CODES */
#define RDF_CRS 0xC0
#define RDF_CRS_ERROR_CREATE_CRS 0xC1

#define CRS_INDEX_PAGES 128 
#define CRS_BIN_PAGES 1024

#define NOT_MAPPED 0
#define ANON_MAPPED 1
#define FILE_MAPPED 2

enum _crs_matrix_index_type {
    GRAPH,
    SUBJECT,
    PREDICATE,
    OBJECT
};

typedef enum _crs_matrix_index_type crs_matrix_index_type;

struct _crs_matrix_index { //mapping to g,s,o
    crs_matrix_index_type row;
    crs_matrix_index_type row_bit;
    crs_matrix_index_type col;
    crs_matrix_index_type col_bit;
};
typedef struct _crs_matrix_index crs_matrix_index;

typedef struct {
    unsigned char* data;
    unsigned char* append;

    size_t len;
    int mapped; /* 0 = NOT_MAPPED, 1 ANON_MAPPED, 2 BACKED_FILE_MAPPED */
    int fd; /* IF BACKED_FILE_MAPPED THIS HOLDS THE FILE DESC */
} crs_mmap_mem;

typedef struct {
    gchar *crs_index_file;
    gchar *crs_bin_file;

    crs_matrix_index *index_conf;
    GPtrArray *temp_values;

    
    crs_mmap_mem *index; /* guint[3] + rowlist, rowptr, colind */ 
    crs_mmap_mem *bin; /* guint[3] + rowlist, rowptr, colind */ 

} crs_matrix;

typedef struct {
    crs_mmap_mem *bin_mem;
    crs_mmap_mem *cursor;
    size_t offset;

    guint colind_len;
    guint rowptr_len;
    guint rowlist_len;

    fs_rid *colind;
    fs_rid *rowlist;
    guint *rowptr;
} crs_mmap_bin_iterator;

void crs_matrix_sync(crs_matrix *matrix,GError **error);
crs_matrix *crs_load_matrix(gchar *file_crs,crs_matrix_index *index,GError **error);

crs_matrix *crs_matrix_new(GPtrArray *temp_values,crs_matrix_index *inde_conf);
crs_matrix *crs_matrix_merge(crs_matrix *kb_matrix, crs_matrix *new_matrix);

void crs_matrix_build(crs_matrix *crs_m,GError *error);
void crs_matrix_free(crs_matrix *crs_m);
guint get_colind_len(crs_mmap_mem *mem);
guint get_rowptr_len(crs_mmap_mem *mem);
guint get_rowlist_len(crs_mmap_mem *mem);
fs_rid * get_colind_ptr(crs_mmap_mem *mem);
fs_rid * get_rowlist_ptr(crs_mmap_mem *mem); 
guint * get_rowptr_ptr(crs_mmap_mem *mem);

crs_mmap_bin_iterator *crs_mmap_bin_iterator_new();
void crs_mmap_bin_iterator_init(crs_mmap_bin_iterator *iterator, crs_mmap_mem *bin_mem);
int crs_mmap_bin_iterator_has_next(crs_mmap_bin_iterator *it);
void crs_mmap_bin_iterator_next(crs_mmap_bin_iterator *it);
void crs_mmap_bin_iterator_free(crs_mmap_bin_iterator *i);
crs_mmap_mem *crs_mmap_mem_new_fd(int fd);
void crs_mmap_mem_sync(crs_mmap_mem *mem);
void crs_mmap_mem_destroy(crs_mmap_mem *mem);
#endif
