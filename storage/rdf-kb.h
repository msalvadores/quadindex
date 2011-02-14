#ifndef _RDF_KB_H  /* duplication check */
#define _RDF_KB_H

#include <glib.h>
#include <kclangc.h>

#include "crs-matrix.h"

#define RDF_KB 0xA0
#define RDF_KB_INVALID_KB_NAME 0xA1
#define RDF_KB_ERROR_CREATING_DIR 0xA2
#define RDF_KB_ERROR_OPEN_HASH 0xA3
#define RDF_KB_ERROR_CLOSING_HASH 0xA4
#define RDF_KB_ERROR_DUMP_NO_MATRIX 0xA5
#define RDF_KB_ERROR_OPEN_FILE_FOR_DUMP 0xA6

typedef struct {
    fs_rid *data;
    size_t mem_size;
    guint elements;
    guint len_data;
} crs_bind_seg_result;

typedef struct {
    crs_matrix_index *index;

    fs_rid **rids;
    guint *rid_sizes;
    
    crs_bind_seg_result **seg_rs; /* N SEG x crs_bind_rs */

} crs_bind_data;

struct _rdf_kb {
    gchar *name;
    gchar *dir_csr;
    gchar *dir_hashes;
    KCDB **hash_stores;
    crs_matrix **matrix;
    crs_matrix_index *index;
};

typedef struct _rdf_kb rdf_kb;

rdf_kb *rdf_kb_open(const char *kb_name,int create, GError **error);

void rdf_kb_import_data_from_file(rdf_kb *kb,const char *uri,const char *model,const char *format, GError **error);

void rdf_kb_dump(rdf_kb *kb,const char *format, const char *file_out, GError **error);

void rdf_kb_close(rdf_kb *kb,GError **error);

crs_bind_data **rdf_kb_json_bind(rdf_kb *kb,char *json_bind,int just_count,int load_from_file,GError **error);

guint rdf_kb_size(rdf_kb *kb,GError **error);

#endif
