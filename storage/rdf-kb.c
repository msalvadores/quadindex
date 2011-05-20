#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <kclangc.h>
#include <errno.h>

#include "storage/rdf-kb.h"
#include "common/rdf-parser.h"
#include "storage/crs-matrix.h"
#include "storage/crs-bind.h"
#include "common/params.h"
#include "common/error.h"

typedef struct {
   crs_matrix *matrix; 
} internal_worker_data;

typedef struct {
    crs_matrix *new_matrix;
    crs_matrix *kb_matrix; 
    crs_matrix *merge_matrix;
} internal_worker_merge;

static KCDB* rdf_kb_open_hash_storage(int segment,gchar *kb_name,GError **error) {
    KCDB *hash_st = kcdbnew();
    gchar *path = g_strdup_printf(HASHES_NAME,kb_name,segment);
    if (!kcdbopen(hash_st, path , KCOWRITER | KCOCREATE)) {
    //if (!kcdbopen(hash_st,  "*" , KCOWRITER | KCOCREATE)) {
        if (error)
            g_set_error (error,RDF_KB,RDF_CRS_ERROR_CREATE_CRS, "Unable to open hash file [%s] : %s", path, kcecodename(kcdbecode(hash_st)));
        return NULL;
     }
     g_free(path);
     return hash_st;
}

rdf_kb *rdf_kb_open(const char *kb_name,int create,GError **error) {
    
    if (!g_thread_get_initialized())
        g_thread_init(NULL);

    rdf_kb *kb=malloc(1*sizeof(rdf_kb));
    kb->name = g_strdup(kb_name);
    if (g_file_test(kb_name,G_FILE_TEST_EXISTS)  && 
        !g_file_test(kb_name,G_FILE_TEST_IS_DIR)) {
        if (error != NULL)
            g_set_error (error,RDF_KB,RDF_KB_INVALID_KB_NAME, "Invalid name for KB %s, it exists and is not a directory", kb_name);
        return NULL;
    }
    if (!g_file_test(kb_name,G_FILE_TEST_EXISTS)) {
        if (!create) {
            g_set_error (error,RDF_KB,RDF_KB_INVALID_KB_NAME, "Invalid name for KB %s, it does not exist", kb_name);
            return NULL;
        }
        int res = g_mkdir_with_parents(kb_name,0777);
        if (res) {
            if (error != NULL)
                g_set_error (error,RDF_KB,RDF_KB_ERROR_CREATING_DIR, "Error mkdir for directory %s (%s)", kb_name, g_strerror(errno));
            return NULL;
        }
        log_info("KB data directory created for %s",kb_name);
        gchar *dir_hashes = g_strdup_printf("%s/%s",kb_name,HASHES_DIR);
        res = g_mkdir_with_parents(dir_hashes,0777);
        if (res) {
            if (error != NULL)
                g_set_error (error,RDF_KB,RDF_KB_ERROR_CREATING_DIR, "Error mkdir for directory %s (%s)", dir_hashes, g_strerror(errno));
            return NULL;
        }
        kb->dir_hashes = dir_hashes;
        gchar *dir_csr = g_strdup_printf("%s/%s",kb_name,CSR_DIR);
        res = g_mkdir_with_parents(dir_csr,0777);
        if (res) {
            if (error != NULL)
                g_set_error (error,RDF_KB,RDF_KB_ERROR_CREATING_DIR, "Error mkdir for directory %s (%s)", dir_csr, g_strerror(errno));
            return NULL;
        }
        kb->dir_csr = dir_csr;
    }
    
    /* TODO: move to configuration */
    crs_matrix_index *index = malloc(sizeof(crs_matrix_index));;
    index->row=PREDICATE;
    index->col=OBJECT;
    index->row_bin=GRAPH;
    index->col_bin=SUBJECT;
    /* end move to */
    kb->index = index;

    kb->hash_stores = malloc(HASHES_NUM * sizeof(KCDB *));
    log_debug("[kb=%s] opnening hashes",kb_name);
    for (int n=0;n<HASHES_NUM;n++) {
        GError *error_hash_open=NULL;
        kb->hash_stores[n] = rdf_kb_open_hash_storage(n,kb->name,&error_hash_open);
        if (error_hash_open) {
            g_propagate_error(error, error_hash_open);
            return NULL;           
       }
    }
    log_debug("[kb=%s] hashes opened",kb_name);
    kb->matrix = malloc(SEGMENTS * sizeof(crs_matrix *));
    for (guint n=0;n<SEGMENTS;n++) {
        GError *error_crs_open=NULL;
        gchar *file_crs = g_strdup_printf(CRS_NAME,kb->name,n);
        kb->matrix[n] = crs_load_matrix(file_crs,kb->index,n,&error_crs_open);
        kb->matrix[n]->id = n;
        if (error_crs_open) {
            g_propagate_error(error, error_crs_open);
            return NULL;           
       }
    }

    return kb;
}

void rdf_kb_sync(rdf_kb *kb,GError **error) {
    for(int j=0;j<SEGMENTS;j++) {
        crs_matrix_sync(kb->matrix[j],error);
    }
}

void rdf_kb_construct_matrix(gpointer worker_data, gpointer user_data) {
    internal_worker_data *internal_data = (internal_worker_data *) worker_data; 
    crs_matrix *matrix = internal_data->matrix;
    GError *error=NULL;
    crs_matrix_build(matrix,error);
    log_debug("index colind %u",crs_colind_len(matrix->index->data));
    log_debug("bin colind %u",crs_colind_len(matrix->bin->data));

    /* TODO: control error */
}


void rdf_kb_merge_matrix(gpointer worker_data, gpointer user_data) {
    internal_worker_merge *internal_data = (internal_worker_merge *) worker_data; 
    internal_data->merge_matrix = crs_matrix_merge(internal_data->kb_matrix,internal_data->new_matrix);
}

void rdf_kb_import_data_from_file(rdf_kb *kb,const char *uri,const char *model,const char *format, GError **error) {
    
    if (!fs_hash_uri)
       fs_hash_init();

    rdf_parser_init();
    
    GError *parse_error=NULL;
    GPtrArray **quads = rdf_parser_parse_file_kb(kb,uri,model,format,&parse_error);
    if (!quads && parse_error) {
        g_propagate_error(error, parse_error);
        return;
    }
    
    if (!kb->matrix) {
        kb->matrix = calloc(SEGMENTS, sizeof(crs_matrix *));
    }

    GThreadPool *pool = g_thread_pool_new(rdf_kb_construct_matrix, NULL, THREAD_POOL_SIZE, FALSE, NULL);
    internal_worker_data worker_data[SEGMENTS];
    for (int nseg=0;nseg<SEGMENTS;nseg++) {
        worker_data[nseg].matrix = crs_matrix_new(quads[nseg],kb->index,nseg);
        g_thread_pool_push(pool, &worker_data[nseg], NULL);
    }
    g_thread_pool_free(pool,FALSE,TRUE);

    pool = g_thread_pool_new(rdf_kb_merge_matrix, NULL, THREAD_POOL_SIZE, FALSE, NULL);
    internal_worker_merge worker_merge[SEGMENTS];
    for (int nseg=0;nseg<SEGMENTS;nseg++) {
        worker_merge[nseg].new_matrix = worker_data[nseg].matrix;
        worker_merge[nseg].kb_matrix = kb->matrix[nseg];
        g_thread_pool_push(pool, &worker_merge[nseg], NULL);
        kb->matrix[nseg] = worker_data[nseg].matrix;
    }
    g_thread_pool_free(pool,FALSE,TRUE);
    for (int nseg=0;nseg<SEGMENTS;nseg++) {
        kb->matrix[nseg] = worker_merge[nseg].merge_matrix;
    }
    GError *error_persist=NULL;
    rdf_kb_sync(kb,&error_persist);
    /* TODO: check errors */ 

    rdf_parser_close();
}

char *rdf_kb_rid_unhash(rdf_kb *kb, fs_rid rid) {
     int hash_table = rid % HASHES_NUM;
     char tmp_rid[16+1];
     char *vbuf;
     size_t vsiz;
     sprintf(tmp_rid,"%llx",rid);
     vbuf = kcdbget(kb->hash_stores[hash_table], tmp_rid , 16 * sizeof(char), &vsiz);
     //log_debug("%llx %s", rid, vbuf);
     return vbuf;
}

void rdf_kb_fill_quad(fs_rid *quad,crs_matrix_index *index,fs_rid row, fs_rid col, fs_rid row_bin, fs_rid col_bin) {
    quad[index->row] = row;
    quad[index->col] = col;
    quad[index->row_bin] = row_bin;
    quad[index->col_bin] = col_bin;

}

void rdf_kb_log_unhashed_quad(rdf_kb *kb, int output_graph, fs_rid *quad,FILE *out) {
   char *buff_g = rdf_kb_rid_unhash(kb,quad[0]); 
   char *buff_s = rdf_kb_rid_unhash(kb,quad[1]); 
   char *buff_p = rdf_kb_rid_unhash(kb,quad[2]); 
   char *buff_o = rdf_kb_rid_unhash(kb,quad[3]); 
   //fprintf(out,"%llx %llx %llx %llx\n", quad[0], quad[1], quad[2], quad[3]);
   if (FS_IS_URI(quad[1]))
        fprintf(out,"<%s> ",buff_s);
   else
        fprintf(out,"_:bid%s ",buff_s +7);
   
   fprintf(out,"<%s> ", buff_p);
    
   if (FS_IS_URI(quad[3]))
       fprintf(out,"<%s> ", buff_o);
   else {
       if (FS_IS_LITERAL(quad[3]))
            fprintf(out,"%s ", buff_o);
       else
            fprintf(out,"_:bid%s ", buff_o + 7);
   }

   if (output_graph)
       fprintf(out,"<%s>", buff_g);
   fprintf(out," .\n");

   free(buff_s);
   free(buff_p);
   free(buff_o);
   free(buff_g);
}

void rdf_kb_dump(rdf_kb *kb, const char *format, const char *file_out, GError **error) {
    if (!kb->matrix) {
        if (error)
            g_set_error (error,RDF_KB,RDF_KB_ERROR_DUMP_NO_MATRIX, "No matrix attach to KB, no data to dump");
        return;
    }
   
    FILE *dump_out = NULL;
    if (!file_out)
        dump_out=stdout;
    else {
        dump_out = fopen(file_out,"w");
        if (!dump_out) {
            if (error)
                g_set_error (error,RDF_KB,RDF_KB_ERROR_OPEN_FILE_FOR_DUMP, "Error open file for dump: %s %s",file_out, g_strerror(errno));
            else
                log_warn("Not possible to open file for dumping RDF data: %s %s",file_out,g_strerror(errno));
            return;
        }
    }
    
    int output_graph = !strcmp(format,"nquads");

    /* this could be run through a ThreadPool ... not for the moment */
    
    crs_mmap_bin_iterator *bin_it = crs_mmap_bin_iterator_new();
    for (int nseg=0;nseg<SEGMENTS;nseg++) {
        
        crs_matrix *matrix = kb->matrix[nseg];
        crs_mmap_bin_iterator_init(bin_it, matrix->bin);

        //guint rowptr_len = crs_rowptr_len(matrix->index);
        guint *index_rowptr = crs_rowptr_head(matrix->index->data);
        
        //guint rowlist_len = crs_rowlist_len(matrix->index);
        fs_rid *index_rowlist = crs_rowlist_head(matrix->index->data); 
        
        guint colind_len = crs_colind_len(matrix->index->data);
        fs_rid *index_colind = crs_colind_head(matrix->index->data);
         
        fs_rid quad[4] = {FS_RID_NULL,FS_RID_NULL,FS_RID_NULL,FS_RID_NULL};
        //log_debug("Elements in first level %d",colind_len);
        guint row_i=0;
        for (guint col_id = 0; col_id < colind_len; col_id++) {

            fs_rid col_rid = index_colind[col_id];
            while (index_rowptr[row_i] <= col_id)
                row_i++;
            fs_rid row_rid = index_rowlist[row_i-1];
            crs_mmap_bin_iterator_next(bin_it);
            //log_debug("Elements in second level(%u) %d %p",col_id,bin_it->colind_len,bin_it);
            fs_rid col_bit_rid = FS_RID_NULL;
            guint row_bit_i = 0;
            for (guint col_bit_i = 0; col_bit_i < bin_it->colind_len; col_bit_i++) {
                 col_bit_rid = bin_it->colind_head[col_bit_i];
                 while (bin_it->rowptr_head[row_bit_i] <= col_bit_i)
                        row_bit_i++;
                fs_rid row_bit_rid = bin_it->rowlist_head[row_bit_i-1];
   //log_debug("%llx %llx %llx %llx", row_rid, col_rid, row_bit_rid, col_bit_rid);
                rdf_kb_fill_quad(quad,kb->index,row_rid,col_rid,row_bit_rid,col_bit_rid);
                //printf("row_i %d\n",row_i);
                rdf_kb_log_unhashed_quad(kb,output_graph,quad,dump_out);
            }
        }
    }
    crs_mmap_bin_iterator_free(bin_it);
    
    if (dump_out != stdout)
        fclose(dump_out);
}

void rdf_kb_close(rdf_kb *kb,GError **error) {
    log_info("Closing KB %s",kb->name);
    for(int j=0;j<HASHES_NUM;j++) {
        /* close the hash database */
        if (!kcdbclose(kb->hash_stores[j])) {
                if (error)
                    g_set_error (error,RDF_KB,RDF_KB_ERROR_CLOSING_HASH, "Error closing hash %d (%s)", j, 
                        kcecodename(kcdbecode(kb->hash_stores[j])));
                return;
        }
        /* delete the database object */
        kcdbdel(kb->hash_stores[j]);
    }
    free(kb->hash_stores);
    for(int j=0;j<SEGMENTS;j++) {
        crs_matrix_free(kb->matrix[j]);
    }
    free(kb->matrix);
    
    /* TODO: free matrix !!! */

    g_free(kb->name);
    free(kb->index);
    free(kb);
}

guint rdf_kb_size(rdf_kb *kb,GError **error) {
    guint size=0;
    crs_mmap_bin_iterator *bin_it = crs_mmap_bin_iterator_new();
    for(guint j=0;j<SEGMENTS;j++) {
        crs_mmap_bin_iterator_init(bin_it,kb->matrix[j]->index);
        guint colind_len = crs_colind_len(kb->matrix[j]->index->data);
        for (guint i=0;i<colind_len;i++) {
            crs_mmap_bin_iterator_next(bin_it);
            size += crs_colind_len(bin_it->cursor);
        }
    }
    crs_mmap_bin_iterator_free(bin_it);
    return size;
}

crs_bind_data **rdf_kb_json_bind(rdf_kb *kb,char *json_bind,int just_count,int use_inverse_bin_index,int load_from_file,GError **error) {
   if (!fs_hash_uri)
       fs_hash_init();
   GPtrArray *binds = parse_json_bind(json_bind,load_from_file);
   
   fs_rid m,s,p,o;
   char *sm=NULL,*ss=NULL,*sp=NULL,*so=NULL;
   crs_bind_data **binds_res = malloc(binds->len * sizeof(crs_bind_data *));
   for(guint i=0; i <  binds->len; i++) {
        GPtrArray *bind_input = (GPtrArray *)g_ptr_array_index(binds,i);
        crs_bind_data* bind_data = crs_bind_data_init_new(bind_input, use_inverse_bin_index);
        crs_bind(bind_data, kb);
        log_debug("end crs_bind");
        guint total = 0;
        for (int i = 0; i < SEGMENTS; i++) {
            crs_bind_seg_result *rs = bind_data->seg_rs[i];
            log_debug("match %u quads from segment %u",rs->len_data,0);
            total += rs->len_data;
            #if 1
            for (guint j=0;j<rs->len_data;j++) {
                m=*(rs->data + (j*4));
                s=*(rs->data + (j*4) + 1);
                p=*(rs->data + (j*4)+ 2);
                o=*(rs->data + (j*4)+ 3);
                //log_debug("--> %llx %llx %llx %llx",m,s,p,o);
                if (!just_count) {
                    log_debug("[RDF] %s %s %s %s",
                        rdf_kb_rid_unhash(kb,m),
                        rdf_kb_rid_unhash(kb,s),
                        rdf_kb_rid_unhash(kb,p),
                        rdf_kb_rid_unhash(kb,o));
                }
            }
            #endif
        } 
        log_debug("total quads %u",total);
        binds_res[i] = bind_data;
        /* TODO destroy bind data  ??*/
   }
   return binds_res;
}
