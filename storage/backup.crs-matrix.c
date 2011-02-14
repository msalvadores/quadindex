#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <kclangc.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "crs-matrix.h"
#include "crs-matrix-io.h"
#include "common/hash.h"
#include "common/error.h"

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

void g_array_insert_shift_rid(GArray* arr,guint i,fs_rid v) {
    if (i < arr->len) {
        g_array_set_size(arr,arr->len+1);

        for(guint x=arr->len-1; x>i ;x--)
             ((fs_rid *)arr->data)[x]=((fs_rid *)arr->data)[x-1];
        
        ((fs_rid *)arr->data)[i]=v;
    } else {
        g_array_append_val(arr,v);
    }
}

guint crs_merge_sort_arrays(GArray *arr,guint start,guint end, fs_rid *src, guint src_len) {
    fs_rid rid_dest=FS_RID_NULL;
    fs_rid rid_src=FS_RID_NULL;
    guint y=start;
    guint merged=0;
    for(guint x=0;x<src_len;x++) {
        rid_src = src[x];
        while(y < end && ((rid_dest=g_array_index(arr,fs_rid,y)) < rid_src))
            y++;
        if (rid_src != rid_dest) {
            merged++;
            g_array_insert_shift_rid(arr,y,rid_src); 
        }
    }
    return merged;
}

void g_ptr_array_insert_shift_many(GPtrArray *arr,gpointer *vals,guint start,guint len) {
    //log_debug("try memcpy ??????? %u %u %u",start,len,arr->len);
    g_ptr_array_set_size(arr,arr->len + len);
    for(guint x=arr->len-1; x>(start+len-1) ;x--)
         arr->pdata[x]=arr->pdata[x-len];
    for(guint x=0; x<len ;x++)
         arr->pdata[start+x]=vals[x];
}

void g_array_insert_shift_rid_many(GArray *arr,fs_rid *vals,guint start,guint len) {
    //log_debug("try memcpy ???????");
    g_array_set_size(arr,arr->len + len);
    for(guint x=arr->len-1; x>(start+len-1) ;x--)
         ((fs_rid *)arr->data)[x]=((fs_rid *)arr->data)[x-len];
    for(guint x=0; x<len ;x++)
         ((fs_rid *)arr->data)[start+x] = vals[x];
}

void g_array_insert_shift_guint(GArray* arr,guint i,guint v) {
    if (i < arr->len) {
        g_array_set_size(arr,arr->len+1);

        for(guint x=arr->len-1; x>i ;x--)
             ((guint *)arr->data)[x]=((guint *)arr->data)[x-1];
        
        ((guint *)arr->data)[i]=v;
    } else {
        g_array_append_val(arr,v);
    }
}

void g_ptr_array_insert_shift(GPtrArray* arr,guint i,gpointer p) {
    
    if (i < arr->len) {
        g_ptr_array_set_size(arr,arr->len+1);

        for(guint x=arr->len-1; x>i ;x--)
             arr->pdata[x]=arr->pdata[x-1];
    
        arr->pdata[i] = p;
    } else {
        g_ptr_array_add(arr,p);
    }
}

int crs_mmap_mem_overflow(crs_mmap_mem *mmap_mem, size_t len) {
    return (mmap_mem->data + mmap_mem->len) < (mmap_mem->append + len); 
}

crs_mmap_mem *crs_mmap_mem_new_fd(int fd) {
    crs_mmap_mem *mem = calloc(1,sizeof(crs_mmap_mem));
    size_t file_size = lseek(fd,0,SEEK_END);
    if (!file_size) {
        file_size = CRS_INDEX_PAGES * sysconf(_SC_PAGESIZE);
        log_debug("empty file truncate to default");
        ftruncate(fd,file_size); 
    }
    mem->data = mmap(NULL,
                     file_size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fd, 0);
    mem->append = mem->data + (3 * sizeof(guint));
    mem->len =  file_size;
    mem->mapped = FILE_MAPPED; 
    mem->fd = fd;
    return mem;
}

crs_mmap_mem *crs_mmap_mem_new(guint pages) {
    size_t ps = sysconf(_SC_PAGESIZE);
    crs_mmap_mem *mem = calloc(1,sizeof(crs_mmap_mem));
    mem->data = mmap(NULL,
                     pages * ps,
                     PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_SHARED,
                     -1, 0);
    mem->append = mem->data + (3 * sizeof(guint));
    mem->len =  pages * ps;
    mem->mapped = ANON_MAPPED; 
    return mem;
}

void crs_mmap_mem_destroy(crs_mmap_mem *mem) {
    munmap(mem->data,mem->len);
    if(mem->mapped == FILE_MAPPED)
        close(mem->fd);
    free(mem);
}

size_t crs_mmap_mem_resize(crs_mmap_mem *mem,guint extra_pages) {
    //log_debug(" resize to %u extra pages",extra_pages);
    size_t ps = sysconf(_SC_PAGESIZE);
    size_t new_size = mem->len + (extra_pages * ps);
    if (mem->mapped == ANON_MAPPED) {
        unsigned char *new_data = mmap(NULL,
                            new_size,
                            PROT_READ | PROT_WRITE,
                            MAP_ANON | MAP_SHARED,
                            -1, 0);
        memcpy(new_data,mem->data,mem->len);
        mem->append = new_data + (mem->append - mem->data);
        munmap(mem->data,mem->len);
        mem->data = new_data;
    } else if (mem->mapped == FILE_MAPPED) {
        munmap(mem->data,mem->len);
        ftruncate(mem->fd,new_size);
        mem->data = mmap(NULL,
                            new_size,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED,
                            mem->fd, 0);
        mem->append = mem->data + (mem->append - mem->data);
    } else
        log_info("why resize nothing ??");

    mem->len = new_size;
    return new_size;
    //log_debug("end resize %p new_size %lu",mem->data,new_size);
}


void crs_matrix_append_bin(crs_matrix *matrix,crs_mmap_mem *bin) {
    size_t len_bin = bin->append - bin->data;
    if (crs_mmap_mem_overflow(matrix->bin,len_bin)) {
        size_t ps = sysconf(_SC_PAGESIZE);
        size_t free_space =  matrix->bin->len - (matrix->bin->append -  matrix->bin->data);
        size_t new_pages = ((len_bin - free_space) / ps) + 512; 
        crs_mmap_mem_resize(matrix->bin,new_pages);
    }
    memcpy(matrix->bin->append,bin->data,len_bin);
    matrix->bin->append += len_bin;
}

void crs_mmap_mem_sync(crs_mmap_mem *mem) {
    msync(mem->data,mem->len,MS_ASYNC);
}

guint get_colind_len(crs_mmap_mem *mem) {
    return ((guint *) mem->data)[2];
}

guint get_rowlist_len(crs_mmap_mem *mem) {
    return ((guint *)mem->data)[1];
}

guint get_rowptr_len(crs_mmap_mem *mem) {
    return ((guint *)mem->data)[0];
}

guint * get_rowptr_ptr(crs_mmap_mem *mem) {
    return (guint *) (mem->data +  (3 * sizeof(guint)));
}

fs_rid * get_rowlist_ptr(crs_mmap_mem *mem) {
    guint *counters = (guint *) mem->data;
    return (fs_rid *) (mem->data + 
                       (3 * sizeof(guint)) +
                       (counters[0] * sizeof(guint)));
}

fs_rid * get_colind_ptr(crs_mmap_mem *mem) {
    guint *counters = (guint *) mem->data;
    return (fs_rid *) (mem->data + 
                       (3 * sizeof(guint)) +
                       (counters[0] * sizeof(guint)) +
                       (counters[1] * sizeof(fs_rid)));
}

crs_mmap_bin_iterator *crs_mmap_bin_iterator_new() {
     crs_mmap_bin_iterator *iterator = calloc(1,sizeof(crs_mmap_bin_iterator));
     iterator->cursor = calloc(1, sizeof(crs_mmap_mem));
     return iterator;
}

void crs_mmap_bin_iterator_init(crs_mmap_bin_iterator *iterator, crs_mmap_mem *bin_mem) {
     iterator->bin_mem = bin_mem;
     iterator->offset = 0;
}

int crs_mmap_bin_iterator_has_next(crs_mmap_bin_iterator *it) {
    return it->bin_mem->len > it->offset; 
}

size_t crs_mem_size(guint rowptr_len,guint rowlist_len,guint colind_len) {
    return ((rowptr_len + 3) * sizeof (guint)) + ((rowlist_len + colind_len) * sizeof(fs_rid));
}
void crs_mmap_bin_iterator_next(crs_mmap_bin_iterator *it) {
    unsigned char *offset_cursor = it->bin_mem->data + it->offset; 
    it->cursor->data = offset_cursor;
    
    it->colind_len = get_colind_len(it->cursor);
    it->colind = get_colind_ptr(it->cursor);

    it->rowptr_len = get_rowptr_len(it->cursor);
    it->rowptr = get_rowptr_ptr(it->cursor);

    it->rowlist_len = get_rowlist_len(it->cursor);
    it->rowlist = get_rowlist_ptr(it->cursor);

    it->offset += crs_mem_size(it->rowptr_len,it->rowlist_len,it->colind_len);
}

void crs_mmap_bin_iterator_free(crs_mmap_bin_iterator *i) {
    free(i->cursor);
    free(i);
}

void crs_matrix_append_rowptr(crs_mmap_mem *mem,guint val) {
    if (crs_mmap_mem_overflow(mem,sizeof(fs_rid))) {
        crs_mmap_mem_resize(mem,1024);
    }
    //rowlist needs shifting colind and rowptr
    void *rowlist =(void *) get_rowlist_ptr(mem);
    size_t s = (get_colind_len(mem) + get_rowlist_len(mem)) * sizeof(fs_rid);
    memcpy(rowlist + sizeof(guint), rowlist, s);
    *((guint *)rowlist)=val; /* rowlist is now the last position of rowptr */ 
    mem->append += sizeof(guint);
    ((guint *)mem->data)[0]++; 
}

void crs_matrix_append_rowlist(crs_mmap_mem *mem,fs_rid val) {
    if (crs_mmap_mem_overflow(mem,sizeof(fs_rid))) {
        crs_mmap_mem_resize(mem,512);
    }
    //rowlist needs shifting colind
    fs_rid *colind = get_colind_ptr(mem);
    memcpy(colind + 1, colind, get_colind_len(mem) * sizeof(fs_rid));
    *colind=val; /* colind is now the last position of rowlist */ 
    mem->append += sizeof(fs_rid);
    ((guint *)mem->data)[1]++; 
}

void crs_matrix_append_colind(crs_mmap_mem *mem,fs_rid val) {
    if (crs_mmap_mem_overflow(mem,sizeof(fs_rid))) {
        crs_mmap_mem_resize(mem,512);
    }
    //colind doesn't need shift
    fs_rid * p = (fs_rid *)mem->append;
    *p = val;
    mem->append += sizeof(fs_rid);
    ((guint *)mem->data)[2]++; 
}

static void crs_matrix_mmap_anon(crs_matrix *matrix) {
    matrix->index = crs_mmap_mem_new(CRS_INDEX_PAGES);
    matrix->bin = crs_mmap_mem_new(CRS_INDEX_PAGES * 1024);
    matrix->bin->append = matrix->bin->data; /* only used for copies; get rid of 3guint offset */
}

crs_matrix *crs_load_matrix(gchar *file_crs,crs_matrix_index *index,GError **error) {
    GError *error_load_crs=NULL;
    crs_matrix *crs = crs_io_load_matrix(file_crs,index,&error_load_crs);
    if (error_load_crs) {
        g_propagate_error(error, error_load_crs);
        return NULL;
    }
    return crs;
}

crs_matrix *crs_matrix_new(GPtrArray *temp_values, crs_matrix_index *index_conf) {
    crs_matrix *res = calloc(1,sizeof(crs_matrix));
    res->temp_values = temp_values;
    res->index_conf = index_conf;
    return res;
}


void crs_matrix_sync(crs_matrix *matrix,GError **error) {
    /* TODO: persist blocking access when import/query/update */
    crs_mmap_mem_sync(matrix->index);
    crs_mmap_mem_sync(matrix->bin);
}

gint sort_quads (gconstpointer a, gconstpointer b, gpointer user_data) {
    int *sort_indexes = (int *)user_data; /* 0-->row_index 1-->row_bin */
    fs_rid *quad_a = *(fs_rid **) a; 
    fs_rid *quad_b = *(fs_rid **) b;
    if (quad_a[sort_indexes[0]] < quad_b[sort_indexes[0]])
       return -1;
    else {
        if (quad_a[sort_indexes[0]] > quad_b[sort_indexes[0]])
            return +1;
        else { //same row
            if (quad_a[sort_indexes[1]] < quad_b[sort_indexes[1]])
                return -1;
            else if (quad_a[sort_indexes[1]] > quad_b[sort_indexes[1]])
                return +1;
            return 0;
        }
    }
}

void crs_bit_matrix_merge(void* kb_bit,void* new_bit) {
    log_debug("init BIT Merge kb_bit len %u new_bit len %u",kb_bit->colind->len,new_bit->colind->len);
    GTimer *ts = g_timer_new();
    guint row_kb_i=0;
    for(guint row_new_i=0 ; row_new_i<new_bit->rowlist->len ; row_new_i++) {
        if (row_new_i % 1000 ==0)
            log_debug("progress bit %u/%u",row_new_i,new_bit->rowlist->len);
        guint col_new_start = g_array_index(new_bit->rowptr,guint,row_new_i);
        guint col_new_end = g_array_index(new_bit->rowptr,guint,row_new_i+1);
        guint col_new_len = col_new_end - col_new_start;
        fs_rid *colind_new_data = ((fs_rid *) new_bit->colind->data) + col_new_start;

        fs_rid row_new_rid = g_array_index(new_bit->rowlist,fs_rid,row_new_i); 
        fs_rid row_kb_rid = FS_RID_NULL;

        while( row_kb_i < kb_bit->rowlist->len && 
              (( row_kb_rid=g_array_index(kb_bit->rowlist,fs_rid,row_kb_i) ) < row_new_rid))
            row_kb_i++;
       
       if (row_kb_i >= kb_bit->rowlist->len)
            row_kb_rid = FS_RID_NULL;
       
       guint new_coldinds = 0;
       if (row_kb_rid != row_new_rid) {
            /* append new row_rid */
            g_array_insert_shift_rid(kb_bit->rowlist,row_kb_i,row_new_rid);
            
            /* new ptr entry with init value same as next ptr */
            guint row_ptr_kb = g_array_index(kb_bit->rowptr,guint,row_kb_i);
            g_array_insert_shift_guint(kb_bit->rowptr,row_kb_i,row_ptr_kb);
            
            /* colind(row_new_rid) doesn't exist so we insert all in one go in the middle of row_ptr_kb */
            g_array_insert_shift_rid_many(kb_bit->colind, colind_new_data, row_ptr_kb, col_new_len);
            new_coldinds =  col_new_len;
       } else {
            /* we just need to merge colind and inc ptr(s) */
            guint col_kb_start = g_array_index(kb_bit->rowptr,guint,row_kb_i);
            guint col_kb_end = g_array_index(kb_bit->rowptr,guint,row_kb_i+1);

            new_coldinds = crs_merge_sort_arrays(kb_bit->colind,col_kb_start,col_kb_end,colind_new_data,col_new_len);
       }
       
       /* inc ptr values as many times as new coldinds from row_kb_i */ 
       for(guint x=row_kb_i+1;x<kb_bit->rowptr->len;x++)
            ((guint *)kb_bit->rowptr->data)[x] = ((guint *)kb_bit->rowptr->data)[x] + new_coldinds;
    }
    new_bit->destroy_after_merge = TRUE;
    log_debug("End BIT merge time = %.2lf",g_timer_elapsed(ts,NULL));
}

void crs_mmap_mem_cpy(crs_mmap_mem *dest, crs_mmap_mem* orig) {
    size_t len = orig->append - orig->data;
    if (len > dest->len) {
        log_debug("resize dest");
        size_t ps = sysconf(_SC_PAGESIZE);
        size_t new_pages = ((len - dest->len) / ps) + 1; 
        crs_mmap_mem_resize(dest,new_pages);
    }
    memcpy(dest->data,orig->data,len);
    dest->len = len;
}

crs_matrix *crs_matrix_merge(crs_matrix *kb_matrix, crs_matrix *new_matrix) {

    log_debug("merge %p",kb_matrix->index->data);

    if (!get_colind_len(kb_matrix->index)) {
        log_debug("return kb_matrix after copy !!!");
        /* kb empty first import just copy */
        log_debug("colind new_matrix %u",get_colind_len(new_matrix->index));
        crs_mmap_mem_cpy(kb_matrix->index,new_matrix->index);
        crs_mmap_mem_cpy(kb_matrix->bin,new_matrix->bin);
        log_debug("colind kb_matrix index %u",get_colind_len(kb_matrix->index));
        log_debug("colind kb_matrix bin %u",get_colind_len(kb_matrix->bin));
        return kb_matrix;
    }

    if (!crs_m->mapped) {
        GError *error_mmap=NULL;
        crs_io_matrix_mmap(crs_m,&error_mmap);
        if (error) { 
            g_propagate_error(error, error_mmap);
            return;
        }
    }

    GTimer *ts = g_timer_new();
    if (!kb_matrix->colind->len) {
        /* the matrix is empty */
        new_matrix->crs_file = strdup(kb_matrix->crs_file); 
        crs_matrix_free(kb_matrix,FALSE);
        return new_matrix;
    }
    /* real merge */
    log_debug("merging data ...");

    guint row_kb_i=0;
    for(guint row_new_i=0 ; row_new_i<new_matrix->rowlist->len ; row_new_i++) {
        guint col_new_start = g_array_index(new_matrix->rowptr,guint,row_new_i);
        guint col_new_end = g_array_index(new_matrix->rowptr,guint,row_new_i+1);
        guint col_new_len = col_new_end - col_new_start;
        fs_rid *colind_new_data = ((fs_rid *) new_matrix->colind->data) + col_new_start;
       // ??  gpointer *bits_new = new_matrix->values->pdata + col_new_start;
//
        fs_rid row_new_rid = g_array_index(new_matrix->rowlist,fs_rid,row_new_i); 
        fs_rid row_kb_rid = FS_RID_NULL;
//
        while( row_kb_i < kb_matrix->rowlist->len && 
              (( row_kb_rid=g_array_index(kb_matrix->rowlist,fs_rid,row_kb_i) ) < row_new_rid))
            row_kb_i++;
       
       if (row_kb_i >= kb_matrix->rowlist->len)
            row_kb_rid = FS_RID_NULL;
//       
       guint new_coldinds = 0;
       if (row_kb_rid != row_new_rid) {
            //log_debug("new row %llx",row_new_rid);
            /* append new row_rid */
            g_array_insert_shift_rid(kb_matrix->rowlist,row_kb_i,row_new_rid);
            
            /* new ptr entry with init value same as next ptr */
            guint row_ptr_kb = g_array_index(kb_matrix->rowptr,guint,row_kb_i);
            g_array_insert_shift_guint(kb_matrix->rowptr,row_kb_i,row_ptr_kb);
            
            /* colind(row_new_rid) doesn't exist so we insert all in one go in the middle of row_ptr_kb */
            g_array_insert_shift_rid_many(kb_matrix->colind, colind_new_data, row_ptr_kb, col_new_len);
            
            /* same for kb->values where the bit_matrix (second level) are kept sorted */
            g_ptr_array_insert_shift_many(kb_matrix->values,bits_new, row_ptr_kb, col_new_len);

            new_coldinds =  col_new_len;
//
       } else {
            /* we just need to merge colind and inc ptr(s) */
            guint col_kb_start = g_array_index(kb_matrix->rowptr,guint,row_kb_i);
            guint col_kb_end = g_array_index(kb_matrix->rowptr,guint,row_kb_i+1);

            fs_rid col_new_rid=FS_RID_NULL;
            fs_rid col_kb_rid=FS_RID_NULL;

            guint y = col_kb_start;
            guint x = col_new_start;
            while( x < col_new_end ) {
                //log_debug("loop %u/%u",x,col_new_end);
                col_new_rid = g_array_index(new_matrix->colind,fs_rid,x);

                while(y < col_kb_end && ((col_kb_rid=g_array_index(kb_matrix->colind,fs_rid,y)) < col_new_rid))
                    y++;
                
                //log_debug("found y %u col_kb_rid %llx col_new_rid %llx",x,col_kb_rid,col_new_rid);

                if (col_new_rid != col_kb_rid) {
                    guint start = x - col_new_start;
                    guint len_shifts = 1;
                    if (y < col_kb_end) {
                        //log_debug("aqui %u %u %u %u",start,x,col_new_start,col_new_end);
                        while(x < (col_new_end-1) && (g_array_index(new_matrix->colind,fs_rid,x) < col_kb_rid)) { 
                            x++;
                            len_shifts++;
                        }
                    } else {
                        len_shifts = col_new_len - start;
                        x = col_new_end;
                    }
                    
                    #if 0 
                    log_debug("start %u len_shifts %u col_new_start %u col_new_end %u",start,len_shifts,col_new_start,col_new_end);
                    for (guint hh=0;hh<new_matrix->colind->len;hh++)
                        log_debug("colind rid %u %llx",hh,g_array_index(new_matrix->colind,fs_rid,hh));
                    for(guint j=0;j<col_new_len;j++)
                        log_debug("el pointer %u %llx",j,*(colind_new_data + j));
                    #endif

                    g_array_insert_shift_rid_many(kb_matrix->colind,colind_new_data + start, y, len_shifts); 
                    g_ptr_array_insert_shift_many(kb_matrix->values,bits_new + start, y, len_shifts); 
                   
                    #if 0 
                    for (guint hh=0;hh<kb_matrix->colind->len;hh++)
                        log_debug("rid after shift %llx",g_array_index(kb_matrix->colind,fs_rid,hh));
                    
                    for (guint hh=0;hh<kb_matrix->values->len;hh++)
                        log_debug("pointer %p",g_ptr_array_index(kb_matrix->values,hh));
                    #endif

                    new_coldinds += len_shifts;

                } else {
                    //log_debug("merge bit");
                    /* the bit matrix already exists we should merge bit matrixes */
                    crs_bit_matrix* kb_bit = (crs_bit_matrix *) g_ptr_array_index(kb_matrix->values,y); 
                    crs_bit_matrix* new_bit = (crs_bit_matrix *) g_ptr_array_index(new_matrix->values,x); 
                    crs_bit_matrix_merge(kb_bit,new_bit);
                }
                x++;
            }
       }
       
       /* inc ptr values as many times as new coldinds from row_kb_i */ 
       for(guint x=row_kb_i+1;x<kb_matrix->rowptr->len;x++)
            ((guint *)kb_matrix->rowptr->data)[x] = ((guint *)kb_matrix->rowptr->data)[x] + new_coldinds;
    }

    crs_matrix_free(new_matrix,TRUE);
    log_debug("End merge time = %.2lf",g_timer_elapsed(ts,NULL));

    return kb_matrix;
return NULL;
}

guint rowptr_list_size(GPtrArray *values,int row_position) {
    /*returns rowlist size and returns (rowptr-1) */
    fs_rid prev=FS_RID_NULL;
    guint pre_size =0;
    fs_rid *quad;
    guint len_tmp = values->len;
    for (guint i=0; i<len_tmp; i++) {
       quad = (fs_rid *) g_ptr_array_index(values,i);
       if (!(prev == quad[row_position])) {
           prev = quad[row_position];
           pre_size++;
       }
    }
    return pre_size;
}

crs_mmap_mem *crs_bit_matrix_build(GPtrArray *temp_values,crs_matrix_index *index_conf) {
    //log_debug("built bit start"); 
    crs_mmap_mem *bit=crs_mmap_mem_new(32);

    int sort_indexes[2] = { index_conf->row_bit, index_conf->col_bit };
    g_ptr_array_sort_with_data(temp_values, (GCompareDataFunc) sort_quads, sort_indexes);
    
    fs_rid *quad;
    guint len_tmp = temp_values->len;
    fs_rid col_id = FS_RID_NULL, row_id = FS_RID_NULL, prev_col_id = FS_RID_NULL, prev_row_id = FS_RID_NULL;
    guint same_row_counter=0;
    int inc_row_ptr=0;

    for (guint i=0; i<len_tmp; i++) {
       quad = (fs_rid *) g_ptr_array_index(temp_values,i);
       col_id = quad[index_conf->col_bit];
       row_id = quad[index_conf->row_bit];
       if (i>0 && col_id == prev_col_id && row_id == prev_row_id) {
           /* nothing happens ... just the same quad again ... we don't store dups ;-) */
       } else {
            crs_matrix_append_colind(bit,col_id);
            inc_row_ptr = prev_row_id == row_id;
            if (inc_row_ptr) {
                /* same row */
            } else {
                crs_matrix_append_rowptr(bit,same_row_counter);
                if (prev_row_id != FS_RID_NULL)
                    crs_matrix_append_rowlist(bit,prev_row_id);
            }
           same_row_counter++;
       }
       prev_col_id = col_id;
       prev_row_id = row_id;
       inc_row_ptr = 0;
    }
    if (same_row_counter > 0) {
        crs_matrix_append_rowptr(bit,same_row_counter);
        crs_matrix_append_rowlist(bit,prev_row_id);
    }
    //log_debug("built bit %u",get_colind_len(bit));

    return bit;
}

void crs_matrix_build(crs_matrix *crs_m,GError *error) {
    GTimer *ts = g_timer_new();
   
    crs_matrix_mmap_anon(crs_m);

    int sort_indexes[2] = { crs_m->index_conf->row, crs_m->index_conf->col };
    g_ptr_array_sort_with_data(crs_m->temp_values, (GCompareDataFunc) sort_quads, sort_indexes);
    
    log_debug("SORT time = %.2lf secs [%p]",g_timer_elapsed(ts,NULL),crs_m);
    g_timer_start(ts);

    fs_rid *quad;
    fs_rid col_id = FS_RID_NULL, row_id = FS_RID_NULL, prev_col_id = FS_RID_NULL, prev_row_id = FS_RID_NULL;
    guint same_row_counter=0;
    int inc_row_ptr=0;
    GPtrArray *bin_temp_values=NULL;

    guint len_tmp = crs_m->temp_values->len;
    /* calculating size */
    guint pre_size= rowptr_list_size(crs_m->temp_values,crs_m->index_conf->row,crs_m->index_conf->row_bit);
    log_info("@@@@@@@@@@@@@@@@@@@ presize %u",pre_size);

    for (guint i=0; i<len_tmp; i++) {
       quad = (fs_rid *) g_ptr_array_index(crs_m->temp_values,i);
       col_id=quad[crs_m->index_conf->col];
       row_id=quad[crs_m->index_conf->row];
       //printf("row_id %llx col_id %llx \n",row_id,col_id);
       if (i>0 && 
           col_id == prev_col_id && row_id == prev_row_id) {
            //append to same structure 2nd level
             g_ptr_array_add(bin_temp_values,quad);
       } else {
            if (bin_temp_values) {
                crs_mmap_mem *bin_mem = crs_bit_matrix_build(bin_temp_values,crs_m->index_conf);
                g_ptr_array_free(bin_temp_values,TRUE);
                bin_temp_values = NULL;
                crs_matrix_append_bin(crs_m,bin_mem);
                crs_mmap_mem_destroy(bin_mem);
            }
            //create a new one
            bin_temp_values = g_ptr_array_new();
            g_ptr_array_add(bin_temp_values,quad);
            
            crs_matrix_append_colind(crs_m->index,col_id);

            inc_row_ptr = prev_row_id == row_id;
            if (!inc_row_ptr) {
                crs_matrix_append_rowptr(crs_m->index,same_row_counter);
                if (prev_row_id != FS_RID_NULL)
                    crs_matrix_append_rowlist(crs_m->index,prev_row_id);
            }
            same_row_counter++;
       }
       prev_col_id = col_id;
       prev_row_id = row_id;
       inc_row_ptr = 0;
       //log_debug(" END process %d/%d",i,len_tmp);
    }
    if (same_row_counter > 0) {
        crs_matrix_append_rowptr(crs_m->index,same_row_counter);
        crs_matrix_append_rowlist(crs_m->index,prev_row_id);
    }
    if (bin_temp_values) {
        crs_mmap_mem *bin_mem = crs_bit_matrix_build(bin_temp_values,crs_m->index_conf);
        g_ptr_array_free(bin_temp_values,TRUE);
        crs_matrix_append_bin(crs_m,bin_mem);

        crs_mmap_mem_destroy(bin_mem);
    }

    for (guint i=0; i< len_tmp;i++) {
       quad = (fs_rid *) g_ptr_array_index(crs_m->temp_values,i);
       free(quad);
    }
    g_ptr_array_free(crs_m->temp_values,TRUE);
#if 0
    for(int g=0;g<crs_m->rowlist->len;g++)
        printf("row %llx\n",g_array_index(crs_m->rowlist,fs_rid,g));
    for(int g=0;g<crs_m->rowptr->len;g++)
        printf(" %d",g_array_index(crs_m->rowptr,guint,g));
    printf("\n");
#endif
    log_debug("BUILD time = %.2lf secs [%p]",g_timer_elapsed(ts,NULL),crs_m);
    g_timer_destroy(ts);
}

void crs_matrix_free(crs_matrix *crs_m) {

    crs_mmap_mem_sync(crs_m->index);
    crs_mmap_mem_destroy(crs_m->index);
    
    crs_mmap_mem_sync(crs_m->bin);
    crs_mmap_mem_destroy(crs_m->bin);
}
