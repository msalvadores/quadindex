#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <sys/types.h>
#include <kclangc.h>

#include "crs-matrix.h"
#include "crs-matrix-io.h"
#include "common/hash.h"
#include "common/error.h"
#include "common/params.h"


void print_array_guint(const char* lit,GArray *arr) {
    fprintf(stdout,"%s\n",lit);
    for(guint x=0;x<arr->len;x++)
        fprintf(stdout,"\t%d --> %u\n",x,g_array_index(arr,guint,x));
}
void print_array_fs_rid(const char* lit,GArray *arr) {
    fprintf(stdout,"%s\n",lit);
    for(guint x=0;x<arr->len;x++)
        fprintf(stdout,"\t%d --> %llx\n",x,g_array_index(arr,fs_rid,x));
}

void crs_io_create_empty_storage(gchar *file_crs_path, GError **error) {
    FILE *crs_file = fopen(file_crs_path,"w");
    if (!crs_file) {
        g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to open CRS file for writting [%s] : %s", file_crs_path,  g_strerror(errno));
        return;
    }



    unsigned char *buffer_header = calloc(1, CRS_IO_FILE_HEADER_LEN);
    guint * version = (guint *) (buffer_header + 0);
    
    guint * const len_col_ind = (guint *) (version + sizeof(guint));
    guint * const size_col_ind = (guint *) (len_col_ind + sizeof(guint));

    guint * const len_row_list = (guint *) (size_col_ind + sizeof(guint));
    guint * const size_row_list = (guint *) (len_row_list + sizeof(guint));

    guint * const len_row_ptr = (guint *) (size_row_list + sizeof(guint));
    guint * const size_row_ptr = (guint *) (len_row_ptr + sizeof(guint));

    *version=0;
    *len_col_ind=0; 
    *len_row_list=0; 
    *len_row_ptr=0; 
    *size_col_ind = CRS_IO_OFFSET_SPACE; 
    *size_row_list = CRS_IO_OFFSET_SPACE; 
    *size_row_ptr = CRS_IO_OFFSET_SPACE; 
    fwrite(buffer_header,1,CRS_IO_FILE_HEADER_LEN,crs_file);

    char *buffer_empty = calloc(1, CRS_IO_OFFSET_SPACE * sizeof(char));
    for (int i=0;i<3;i++) {
        size_t x = fwrite(buffer_empty,1,CRS_IO_OFFSET_SPACE,crs_file);
        if (x < CRS_IO_OFFSET_SPACE) { 
             g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to write enough in [%s] %lu/%d: %s", file_crs_path,x,CRS_IO_OFFSET_SPACE,g_strerror(errno));
             fclose(crs_file);
             return;
         }
    }
    fclose (crs_file);
    free(buffer_header);
    free(buffer_empty);
    log_debug("created %s",file_crs_path);


    gchar *bit_file_path = g_strdup_printf(BIT_NAME,file_crs_path);
    FILE *crs_bit_file = fopen(bit_file_path,"w");
    if (!crs_bit_file) {
        g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to open CRS file for writting [%s] : %s", bit_file_path,  g_strerror(errno));
        return;
    }
   
    buffer_header = calloc(1, CRS_IO_FILE_BIT_HEADER_LEN); 
    version = (guint *) (buffer_header + 0);
    guint * const size = (guint *) (buffer_header + sizeof(guint));
    guint * const sorted = (guint *) (size + sizeof(guint));
    *version=0;
    *size=0;
    *sorted=0; /* to know when the list is sorted */

    fwrite(buffer_header,1,CRS_IO_FILE_BIT_HEADER_LEN,crs_bit_file);
    /* TODO check fwrite err */
    
    buffer_empty = calloc(1, CRS_IO_BIT_HEADER_OFFSET * sizeof(char));
    fwrite(buffer_header,1,CRS_IO_BIT_HEADER_OFFSET,crs_bit_file);
    /* TODO check fwrite err */
    
    fclose (crs_bit_file);
    free(buffer_header);
    free(buffer_empty);
    log_debug("created %s",bit_file_path);
    g_free(bit_file_path);
}

size_t crs_io_bit_elt_offset(crs_bit_matrix *b) {
    return (b->rowptr->len * sizeof(guint)) + (b->rowlist->len * sizeof(fs_rid)) + (b->colind->len * sizeof(fs_rid));
}

void crs_io_matrix_persist(crs_matrix *matrix,GError **error) {

    FILE *crs_file = fopen(matrix->crs_file,"w");
    if (!crs_file) {
        g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to open CRS file for writting [%s] : %s", matrix->crs_file,  g_strerror(errno));
        return;
    }
    unsigned char *buffer_header = calloc(1, CRS_IO_FILE_HEADER_LEN);
    guint * version = (guint *) (buffer_header + 0);
    guint * const len_col_ind = (guint *) (version + sizeof(guint));
    guint * const len_row_list = (guint *) (len_col_ind + sizeof(guint));
    guint * const len_row_ptr = (guint *) (len_row_list + sizeof(guint));
    *version=0;
    *len_col_ind=matrix->colind->len; 
    *len_row_list=matrix->rowlist->len; 
    *len_row_ptr=matrix->rowptr->len; 

    fwrite(buffer_header,1,CRS_IO_FILE_HEADER_LEN,crs_file);
    log_debug("sizes lists %u %u %u",*len_row_ptr, *len_row_list, *len_col_ind);
    unsigned char *buffer = NULL;
    size_t size=0;

    print_array_guint("writing rowptr",matrix->rowptr);
    size = matrix->rowptr->len * sizeof(guint);
    buffer = malloc(size);
    memcpy (buffer, matrix->rowptr->data, size);
    /*TODO check resize spaces */
    fwrite(buffer,1,size,crs_file);
    free(buffer);
    
    fseek(crs_file, CRS_IO_OFFSET_SPACE + CRS_IO_FILE_HEADER_LEN , SEEK_SET);
    print_array_fs_rid("writing rowlist",matrix->rowlist);
    size = matrix->rowlist->len * sizeof(fs_rid);
    buffer = malloc(size);
    memcpy (buffer, matrix->rowlist->data, size);
    /*TODO check resize spaces */
    fwrite(buffer,1,size,crs_file);
    free(buffer);

    fseek(crs_file, CRS_IO_OFFSET_SPACE +  CRS_IO_OFFSET_SPACE + CRS_IO_FILE_HEADER_LEN , SEEK_SET);
    print_array_fs_rid("writing colind",matrix->colind);
    size = matrix->colind->len * sizeof(fs_rid);
    buffer = malloc(size);
    memcpy (buffer, matrix->colind->data, size);
    /*TODO check resize spaces */
    fwrite(buffer,1,size,crs_file);
    free(buffer);

    fclose (crs_file);
    free(buffer_header);

    gchar *bit_file_path = g_strdup_printf(BIT_NAME,matrix->crs_file);
    FILE *crs_bit_file = fopen(bit_file_path,"w");
    if (!crs_bit_file) {
        g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to open CRS file for writting [%s] : %s", bit_file_path,  g_strerror(errno));
        return;
    }
   
    buffer_header = calloc(1, CRS_IO_FILE_BIT_HEADER_LEN * sizeof(char)); 
    version = (guint *) buffer_header;
    guint * const mbit_len = (guint *) (buffer_header + sizeof(guint));
    guint * const sorted = (guint *) (mbit_len + sizeof(guint));
    *version=0;
    *sorted=1;
    *mbit_len = matrix->values->len;
    log_debug("saving %u elements in bit file",*mbit_len);
    fwrite(buffer_header,1,CRS_IO_FILE_BIT_HEADER_LEN,crs_bit_file);
    /* TODO check fwrite err */
    free(buffer_header);
    
    buffer = calloc(1, CRS_IO_BIT_HEADER_OFFSET * sizeof(char));
    
    unsigned char *buffer_cursor = buffer;
    crs_bit_matrix *b=NULL;
    guint n_elts = *mbit_len;
    for (guint i=0; i < n_elts; i++) {
        b = (crs_bit_matrix *) g_ptr_array_index(matrix->values,i);
        memcpy (buffer_cursor,&i, sizeof(guint));
        buffer_cursor += sizeof(guint);
        memcpy (buffer_cursor,&b->rowptr->len, sizeof(guint));
        buffer_cursor += sizeof(guint);
        memcpy (buffer_cursor,&b->rowlist->len, sizeof(guint));
        buffer_cursor += sizeof(guint);
        memcpy (buffer_cursor,&b->colind->len, sizeof(guint));
        buffer_cursor += sizeof(guint);
        size_t offset_elt =  crs_io_bit_elt_offset(b);
        memcpy (buffer_cursor,&offset_elt, sizeof(size_t));
        buffer_cursor += sizeof(size_t);
        log_debug("counter matrix bit %d %u %u %u %lu",i,b->rowptr->len,b->rowlist->len,b->colind->len,offset_elt);
    }
    fwrite(buffer,1,CRS_IO_BIT_HEADER_OFFSET,crs_bit_file);
    for (guint i=0; i<n_elts; i++) {
        b = (crs_bit_matrix *) g_ptr_array_index(matrix->values,i);
        fwrite(b->rowptr->data,1,b->rowptr->len * sizeof(guint),crs_bit_file);
        fwrite(b->rowlist->data,1,b->rowlist->len * sizeof(fs_rid),crs_bit_file);
        fwrite(b->colind->data,1,b->colind->len * sizeof(fs_rid),crs_bit_file);
    }

    /* TODO check fwrite err */
    
    fclose (crs_bit_file);
    free(buffer);
    g_free(bit_file_path);
    log_debug("end %s",matrix->crs_file);
}


crs_matrix *crs_io_load_matrix(gchar *file_crs_path,crs_matrix_index *index, GError **error) {

   log_debug("opening %s",file_crs_path);
   FILE *crs_file = fopen(file_crs_path,"r");
   if (!crs_file) {
        g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to open CRS file for reading [%s] : %s", file_crs_path,  g_strerror(errno));
        return NULL;
   }

   unsigned char *buffer_header = calloc(1, CRS_IO_FILE_HEADER_LEN);
   size_t bread = fread (buffer_header,1,CRS_IO_FILE_HEADER_LEN,crs_file);
   if (bread < CRS_IO_FILE_HEADER_LEN) {
        g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Error reading CRS HEADER for file %s: %s", file_crs_path,  g_strerror(errno));
        return NULL;
   }

   guint * const version = (guint *) (buffer_header + 0);
   guint * const len_col_ind = (guint *) (version + sizeof(guint));
   guint * const len_row_list = (guint *) (len_col_ind + sizeof(guint));
   guint * const len_row_ptr = (guint *) (len_row_list + sizeof(guint));
   log_debug("sizes lists %u %u %u",*len_row_ptr, *len_row_list, *len_col_ind);
   crs_matrix *res = NULL;
   if (*len_col_ind > 0) {
       res = crs_matrix_alloc_with_sizes(*len_col_ind,*len_row_list,*len_row_ptr);

       size_t size_lists = (*len_col_ind *  sizeof(fs_rid)) + (*len_row_list *  sizeof(fs_rid)) + (*len_row_ptr * sizeof(guint)); 
       unsigned char *lists_data = calloc(1, size_lists);
       bread = fread (lists_data,1,size_lists,crs_file);
       guint *rowptr_buff = (guint *) lists_data;
       for (guint x=0;x<*len_row_ptr;x++) {
            g_array_insert_val(res->rowptr, x, *rowptr_buff);
            rowptr_buff += sizeof(guint);
       }
       print_array_guint("res->rowptr(0) ",res->rowptr);
       fs_rid *rid_buff = (fs_rid *) rowptr_buff;
       for (guint x=0;x<*len_row_list;x++) {
            g_array_insert_val(res->rowlist,x, *rid_buff);
            rid_buff += sizeof(fs_rid);
       }
       print_array_fs_rid("res->rowlist(0) ",res->rowlist);
       for (guint x=0;x<*len_col_ind;x++) {
            printf("A ----> %llx \n", *rid_buff);
            g_array_insert_val(res->colind, x, *rid_buff);
            rid_buff += sizeof(fs_rid);
       }
       print_array_fs_rid("res->colind(0) ",res->colind);


       /* bit file */
       
       gchar *bit_file_path = g_strdup_printf(BIT_NAME,file_crs_path);
       FILE *crs_bit_file = fopen(bit_file_path,"r");
       if (!crs_bit_file) {
            g_set_error (error,RDF_CRS,RDF_CRS_ERROR_CREATE_CRS, "Unable to open CRS file for writting [%s] : %s", bit_file_path,  g_strerror(errno));
            return NULL;
       }
       g_free(bit_file_path);
       
       buffer_header = calloc(1, CRS_IO_FILE_BIT_HEADER_LEN * sizeof(char)); 
       size_t bit_read = fread (buffer_header,1,CRS_IO_FILE_BIT_HEADER_LEN,crs_bit_file);
       log_debug("bit_read %lu ",bit_read);
       guint * const version_bit = (guint *) buffer_header;
       guint * const mbit_len = (guint *) (buffer_header + sizeof(guint));
       guint * const sorted = (guint *) (mbit_len + sizeof(guint));
       log_debug("version %u ",*version_bit);
       log_debug("mbit_len %u ",*mbit_len);
       log_debug("sorted %u ",*sorted);
       /* fill matrix->values with crs_bit_matrix(s) */
       unsigned char *buffer_header_counters =  calloc(1,CRS_IO_BIT_HEADER_OFFSET); 
       fread (buffer_header_counters,1,CRS_IO_BIT_HEADER_OFFSET,crs_bit_file);
       unsigned char *cursor=buffer_header_counters;
       unsigned char *cursor_bit=NULL;
       crs_bit_matrix *m_bit=NULL;
       guint n_elts = *mbit_len;
       unsigned char *buffer_bit_elts=NULL;
       size_t shift=0;
       for (guint i=0;i<n_elts;i++) {
            guint i_elt = *((guint *) cursor);
            cursor += sizeof(guint);
            guint rowptr_len = *((guint *) cursor);
            cursor += sizeof(guint);
            guint rowlist_len = *((guint *) cursor);
            cursor += sizeof(guint);
            guint colind_len = *((guint *) cursor);
            cursor += sizeof(guint);
            size_t slot_offset = *((size_t *) cursor); 
            cursor += sizeof(size_t);
            log_debug("counter matrix bit %u %u %u %u %lu",i_elt,rowptr_len,rowlist_len,colind_len,slot_offset);

            m_bit = crs_bit_matrix_alloc_with_sizes(rowptr_len,rowlist_len,colind_len);
            buffer_bit_elts = calloc(1,slot_offset);
            cursor_bit = buffer_bit_elts;
            fread(buffer_bit_elts,1,slot_offset,crs_bit_file);
            shift = rowptr_len * sizeof(guint);
            memcpy(m_bit->rowptr->data,cursor_bit,shift);
            m_bit->rowptr->len = rowptr_len;
            print_array_guint("m_bit->rowptr ",m_bit->rowptr);
            cursor_bit += shift;
            shift = rowlist_len * sizeof(fs_rid);
            memcpy(m_bit->rowlist->data,cursor_bit,shift);
            m_bit->rowlist->len = rowlist_len;
            print_array_fs_rid("m_bit->rowlist ",m_bit->rowlist);
            cursor_bit += shift;
            shift = colind_len * sizeof(fs_rid);
            memcpy(m_bit->colind->data,cursor_bit,shift);
            m_bit->colind->len = colind_len;
            print_array_fs_rid("m_bit->colind ",m_bit->colind);
            free(buffer_bit_elts);
            g_ptr_array_add(res->values,m_bit);
            log_debug("copied values arrays");
       }
       free(buffer_header_counters);//I only need off_sets for the moment from now on.

   

   } else {
       log_info("No data in %s therefore we create empty one",file_crs_path);
       res = crs_matrix_alloc();
   }
   res->crs_file =  file_crs_path;
   res->index = index;   

   fclose (crs_file);
   free(buffer_header);

   return res; 
}


