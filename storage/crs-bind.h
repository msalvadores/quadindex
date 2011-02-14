#ifndef _RDF_KB_BIND_H  /* duplication check */
#define _RDF_KB_BIND_H

#include "common/hash.h"
#include "storage/crs-matrix.h"
#include "storage/rdf-kb.h"

#define SEG_RESULT_MEM_SLOT 512

GPtrArray *parse_json_bind(char *bind_input,int load_from_file);
crs_bind_data* crs_bind_data_init_new(GPtrArray *binds);
void crs_bind(crs_bind_data *bind_data, rdf_kb *kb);

#endif
