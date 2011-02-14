#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <kclangc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "crs-matrix.h"
#include "crs-matrix-io.h"
#include "common/hash.h"
#include "common/error.h"
#include "common/params.h"


size_t crs_io_mmap_matrix(crs_matrix *matrix) {

    int fd_index = open(matrix->crs_index_file, O_RDWR | O_CREAT, (mode_t) 0600);
    struct stat sb;
    fstat(fd_index,&sb);
    matrix->index = crs_mmap_mem_new_fd(fd_index); 

    int fd_bin = open(matrix->crs_bin_file, O_RDWR | O_CREAT, (mode_t) 0600);
    matrix->bin = crs_mmap_mem_new_fd(fd_bin); 
    return sb.st_size;
}

crs_matrix *crs_io_load_matrix(gchar *crs_path,crs_matrix_index *index,guint segment,GError **error) {
    log_debug("loading ... %s",crs_path);
    crs_matrix *matrix = crs_matrix_new(NULL,index,segment);
    
    
    matrix->crs_index_file = g_strdup_printf("%s%s",crs_path,".index");
    matrix->crs_bin_file = g_strdup_printf("%s%s",crs_path,".bin");
 
    size_t index_size = crs_io_mmap_matrix(matrix);
    if (index_size) { 
        log_debug("opening colind kb_matrix %u",crs_colind_len(matrix->index->data));
        log_debug("opening bin colind kb_matrix %u",crs_colind_len(matrix->bin->data));
    }
    else
        log_debug("starting empty KB");
    

    return matrix;
}
