#ifndef _RDF_KB_CRS_IO_H  /* duplication check */
#define _RDF_KB_CRS_IO_H

#include <glib.h>

/* ERROR CODES */
#define RDF_CRS_IO 0xD0

crs_matrix * crs_io_load_matrix(gchar *file_crs_path,crs_matrix_index *index,guint segment, GError **error);


#endif
