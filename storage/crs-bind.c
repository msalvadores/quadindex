#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <glib-object.h>
#include <json-glib/json-glib.h>

#include "storage/crs-bind.h"
#include "storage/rdf-kb.h"
#include "common/error.h"
#include "common/params.h"

#define undo_index(i,e,r0,c0,r1,c1) \
    i->row == e ? r0 : ( i->col == e ? c0 : (i->row_bin == e ? r1 : c1 ))

typedef struct {
    fs_rid *l;
    guint ll;
    guint lc; 

    fs_rid *m;
    guint ml;
    guint mc;
} fs_rid_iterator;

fs_rid_iterator *fs_rid_iterator_new_empty() {
    return (fs_rid_iterator *)malloc(sizeof(fs_rid_iterator));
}

void fs_rid_iterator_set(fs_rid_iterator *it,fs_rid *l,guint ll,fs_rid *m,guint ml) {
    it->l=l; 
    it->ll=ll; 
    it->m=m; 
    it->ml=ml;
    it->mc = 0;
    it->lc = 0;
}


fs_rid_iterator *fs_rid_iterator_new(fs_rid *l,guint ll,fs_rid *m,guint ml) {
    fs_rid_iterator *it = fs_rid_iterator_new_empty();
    fs_rid_iterator_set(it,l,ll,m,ml);
    return it;
}


guint fs_rid_next(fs_rid_iterator *it) {

    if (it->mc == it->ml || it->lc == it->ll)
        return G_MAXUINT;

    fs_rid m = it->m[it->mc];
    //log_debug("%llx",m);
    guint ll = it->ll - it->lc;

    fs_rid *l = it->l + it->lc;
    guint mid = 0;
    if (m != FS_RID_NULL) {
        guint min=0,max=ll;
        //log_debug("before while ll %u",ll);
        if (ll>1) {
        do {
            mid = min + (max - min) / 2;
            //log_debug("mid %u",mid);
            if (m > l[mid])
                min = mid + 1;
             else
                max = mid - 1;
        } while (l[mid] != m && !(min > max) && mid > 0 && mid < ll);
        }
        //log_debug("after while");
        it->mc++;
        it->lc += mid;
        if ( m ==  l[mid]) 
            return mid;
        return G_MAXUINT;
    }
    it->lc++;
    return it->lc - 1;
}

size_t get_mem_slot_size() {
    return SEG_RESULT_MEM_SLOT * 4 * sizeof(fs_rid);
}

crs_bind_seg_result *crs_bind_seg_result_new() {
    crs_bind_seg_result *r  = malloc(sizeof(crs_bind_seg_result));
    r->mem_size = 0;
    r->len_data = 0;
    r->data = NULL; 
    return r;
}

void crs_bind_seg_result_append(crs_bind_seg_result *r,fs_rid m,fs_rid s,fs_rid p,fs_rid o) {
#if 1
    if (r->mem_size == 0) {
        r->mem_size = get_mem_slot_size();
        r->data = g_malloc(r->mem_size);
    } else if (((r->len_data + 1) * 4 * sizeof(fs_rid)) > r->mem_size) {
        fs_rid *new = g_try_realloc(r->data,get_mem_slot_size() + r->mem_size);
        r->mem_size += get_mem_slot_size();
        if (!new) {
            /* TODO: handle error */
            log_debug("error reallocating");
        }
        r->data = new; 
    }
    *(r->data + (r->len_data * 4)) = m;
    *(r->data + (r->len_data * 4) + 1) = s;
    *(r->data + (r->len_data * 4) + 2) = p;
    *(r->data + (r->len_data * 4) + 3) = o;
    r->len_data++;
#endif
}

void crs_bind_segment(gpointer segment_data, gpointer pbind_data) {
    log_debug("IN");
    crs_bind_data *bind_data = (crs_bind_data *) pbind_data;
    crs_matrix *matrix = (crs_matrix *) segment_data;

    fs_rid *row = bind_data->rids[bind_data->index->row]; 
    guint row_len = bind_data->rid_sizes[bind_data->index->row];    

    fs_rid *col = bind_data->rids[bind_data->index->col]; 
    guint col_len = bind_data->rid_sizes[bind_data->index->col]; 

    fs_rid *row_bin = bind_data->rids[bind_data->index->row_bin]; 
    guint row_bin_len = bind_data->rid_sizes[bind_data->index->row_bin]; 
  
    fs_rid *col_bin = bind_data->rids[bind_data->index->col_bin]; 
    guint col_bin_len = bind_data->rid_sizes[bind_data->index->col_bin]; 

    fs_rid col_rid = FS_RID_NULL, row_rid = FS_RID_NULL,  
    col_bin_rid = FS_RID_NULL, row_bin_rid = FS_RID_NULL;

    fs_rid *row_list = crs_rowlist_head(matrix->index->data);
    fs_rid_iterator *it_row = fs_rid_iterator_new(row_list,crs_rowlist_len(matrix->index->data),row,row_len);
    fs_rid_iterator *it_col = fs_rid_iterator_new_empty();

    fs_rid_iterator *it_bin_col = fs_rid_iterator_new_empty();
    fs_rid_iterator *it_bin_row = fs_rid_iterator_new_empty();
   
    guint *rowptr = crs_rowptr_head(matrix->index->data);
    guint *rowbinptr = NULL;
    guint rowptr_init, rowptr_end , rowptr_bin_init , rowptr_bin_end;
    fs_rid *colind = crs_colind_head(matrix->index->data);
    fs_rid *colbinind = NULL;
    guint col_match,row_match, row_bin_match, col_bin_match;

    crs_mmap_bin_iterator *bin_it = crs_mmap_bin_iterator_new();
    crs_mmap_bin_iterator_init(bin_it,matrix->bin);
    crs_mmap_bin_iterator_next(bin_it);

    crs_bind_seg_result *rs = bind_data->seg_rs[matrix->id];
    
    fs_rid colval,rowval,colbinval,rowbinval;
    guint match=0; 
    while((row_match=fs_rid_next(it_row)) < G_MAXUINT) {
        //log_debug("row match index %d",row_match);
        rowptr_init = rowptr[row_match];
        rowptr_end = rowptr[row_match+1];
        //log_debug("row pointers [%d,%d]",rowptr_init,rowptr_end);
        fs_rid_iterator_set(it_col,colind + rowptr_init, rowptr_end - rowptr_init, col, col_len);
        while((col_match=fs_rid_next(it_col)) < G_MAXUINT) {
            crs_mmap_bin_iterator_fetch(bin_it, col_match + rowptr_init);
            //log_debug("bin iterator positioned %u, eltos %u", bin_it->i, crs_colind_len(bin_it->cursor));
            rowbinptr = bin_it->rowptr_head;
            colbinind = bin_it->colind_head;
            fs_rid_iterator_set(it_bin_row, bin_it->rowlist_head, bin_it->rowlist_len, row_bin, row_bin_len);
            while((row_bin_match=fs_rid_next(it_bin_row)) < G_MAXUINT) {
                rowptr_bin_init = rowbinptr[row_bin_match];
                rowptr_bin_end = rowbinptr[row_bin_match+1];
                fs_rid_iterator_set(it_bin_col,colbinind + rowptr_bin_init, rowptr_bin_end - rowptr_bin_init, col_bin, col_bin_len);
                while((col_bin_match=fs_rid_next(it_bin_col)) < G_MAXUINT) {
                    match++;
                    rowval=row_list[row_match];
                    colval=colind[col_match + rowptr_init];
                    rowbinval=bin_it->rowlist_head[row_bin_match];
                    colbinval=colbinind[col_bin_match + rowptr_bin_init];

//log_debug("MATCH [row:%llx col:%llx row_bin:%llx col_bin:%llx]",rowval,colval,rowbinval,colbinval);

    crs_bind_seg_result_append(rs,
        undo_index(bind_data->index,GRAPH,rowval,colval,rowbinval,colbinval),
        undo_index(bind_data->index,SUBJECT,rowval,colval,rowbinval,colbinval),
        undo_index(bind_data->index,PREDICATE,rowval,colval,rowbinval,colbinval),
        undo_index(bind_data->index,OBJECT,rowval,colval,rowbinval,colbinval)
        );
                }
            }
        }

    }
log_debug("end bind [%u matches]",match);
    /* TODO: destroy all the iterators objects */
}


void quickaux(fs_rid *arr,guint start, guint end) {
    if (start < end) {
        fs_rid pivot = arr[end];
        guint i = start;
        guint j = end;
        while (i != j) { 
            if (arr[i] < pivot) { 
                i = i + 1; 
            } 
            else { 
                arr[j] = arr[i]; 
                arr[i] = arr[j-1]; 
                j = j - 1; 
            } 
      } 
      arr[j] = pivot;
      quickaux(arr, start, j-1); 
      quickaux(arr, j+1, end); 
    }
}

void quicksort(fs_rid *arr,int len) {
    quickaux(arr,0,len - 1);
}


void crs_bind(crs_bind_data *bind_data, rdf_kb *kb) {
    if (!g_thread_get_initialized())
        g_thread_init(NULL);

    /*TODO: cost of having this pool locally */
    GThreadPool *pool = g_thread_pool_new(crs_bind_segment, bind_data, THREAD_POOL_SIZE, FALSE, NULL);
    
    bind_data->index = kb->index;
    
    //TODO: sort all to do a linear scan
    for(guint i=0;i<4; i++) {
        if (bind_data->rid_sizes[i] > 1 && bind_data->rid_sizes[i] < G_MAXUINT)
            quicksort(bind_data->rids[i], bind_data->rid_sizes[i]);
    }
    for (int nseg=0;nseg<SEGMENTS;nseg++) {
        g_thread_pool_push(pool, kb->matrix[nseg], NULL);
    }
    g_thread_pool_free(pool,FALSE,TRUE);
    log_debug("pool freed"); 
}



fs_rid get_rid_from_json (JsonNode *element_node) {
    fs_rid res = FS_RID_NULL;
    if ( JSON_NODE_HOLDS_OBJECT(element_node) ) { 
        /* literal */
        JsonObject *literal_obj = json_node_get_object(element_node);
        const char *value = json_node_get_string( json_object_get_member(literal_obj,"value") );
        
        fs_rid literal_rid = FS_RID_NULL;
        if (json_object_has_member(literal_obj,"datatype")) { 
            const char *datatype =  json_node_get_string( json_object_get_member(literal_obj,"datatype") );
            literal_rid = fs_hash_literal(value,fs_hash_uri(datatype));
        } else if (json_object_has_member(literal_obj,"lang")) { 
            const char *lang =  json_node_get_string( json_object_get_member(literal_obj,"lang") );
            literal_rid = fs_hash_literal(value,fs_hash_uri(lang));
        } else 
            literal_rid = fs_hash_literal(value,FS_RID_NULL);
        
        res = literal_rid;
    } else { 
        /* URI */
        const char *uri = json_node_get_string(element_node);
        res = fs_hash_uri(uri);
    }
    log_debug("RES FOR BIND %llx",res);
    return res;
}


GArray *get_array_values(JsonNode *node) {
     GArray *res = NULL;
     const fs_rid null_rid = FS_RID_NULL;
     if (JSON_NODE_HOLDS_ARRAY(node)) {
        res = g_array_new(FALSE,FALSE,sizeof(fs_rid)); 
        JsonArray *arr = json_node_get_array(node);
        guint len_json_arr = json_array_get_length(arr);
        if (len_json_arr) {
            for (guint i=0; i < json_array_get_length(arr); i++) {
                fs_rid rid = get_rid_from_json( json_array_get_element(arr,i));
                g_array_append_val(res,rid);
            }
        } else {
           g_array_append_val(res,null_rid);
        }
     }
     return res;
}

GPtrArray *parse_json_bind(char *bind_input,int load_from_file) {
    log_debug("IN");
    JsonParser *parser;
    JsonNode *root;
    GError *error=NULL;
    g_type_init ();
    parser = json_parser_new ();
    if (load_from_file)
        json_parser_load_from_file (parser, bind_input, &error);
    else
        json_parser_load_from_data (parser, bind_input, strlen(bind_input),&error);
    if (error) {
        log_debug("Unable to parse `%s': %s\n", bind_input, error->message);
        fflush(stdout);
        g_error_free (error);
        g_object_unref (parser);
        return NULL;
    }
    GPtrArray *res = g_ptr_array_new(); 
    root = json_parser_get_root (parser);
    if (!JSON_NODE_HOLDS_ARRAY(root)) {
         log_debug("Array of binds don't found");
         return NULL;
    }
    JsonArray *binds = json_node_get_array(root);
    guint lb = json_array_get_length(binds);
    for (guint b=0; b < lb; b++) {
         //log_debug("Bind %u",b);
         GPtrArray *values = g_ptr_array_new(); 
         JsonArray *bind = json_node_get_array(json_array_get_element(binds,b) );
         GArray *mvalues = get_array_values(json_array_get_element(bind,0));
         g_ptr_array_add(values,mvalues);
         GArray *svalues = get_array_values(json_array_get_element(bind,1));
         g_ptr_array_add(values,svalues);
         GArray *pvalues = get_array_values(json_array_get_element(bind,2));
         g_ptr_array_add(values,pvalues);
         GArray *ovalues = get_array_values(json_array_get_element(bind,3));
         g_ptr_array_add(values,ovalues);
         g_ptr_array_add(res,values);
    }
    g_object_unref (parser);
    log_debug("OUT");
    return res;
}

crs_bind_data *crs_bind_data_init_new(GPtrArray *bind) {
    crs_bind_data *res = malloc(sizeof(crs_bind_data));
    res->rids =  malloc(4 *sizeof(fs_rid *));
    res->rid_sizes =  malloc(4 *sizeof(guint *));
    
    for (int i=0;i<4;i++) {
        if (g_ptr_array_index(bind,i)) {
            GArray *arr_rids = (GArray *) g_ptr_array_index(bind,i);
            res->rids[i] = (fs_rid *)arr_rids->data;
            res->rid_sizes[i] = arr_rids->len;
        } else {
            res->rids[i] = NULL;
            res->rid_sizes[i] = -1;
        }
    }

    res->seg_rs = malloc(sizeof(crs_bind_seg_result *) * SEGMENTS);
    for (int i = 0; i < SEGMENTS; i++) {
       res->seg_rs[i] = crs_bind_seg_result_new();
    } 

    return res;
}
