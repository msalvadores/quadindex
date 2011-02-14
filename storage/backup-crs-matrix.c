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
#define CRS_HEADER 6 * sizeof(guint)

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

#define COLIND_OFFSET 0
#define ROWLIST_OFFSET 1
#define ROWPTR_OFFSET 2
#define COLIND_LEN 3 
#define ROWLIST_LEN 4
#define ROWPTR_LEN 5

gint sort_quads (gconstpointer a, gconstpointer b, gpointer user_data) {
    crs_matrix_index *index = (crs_matrix_index *)user_data; /* 0-->row_index 1-->row_bin */
    fs_rid *quad_a = *(fs_rid **) a; 
    fs_rid *quad_b = *(fs_rid **) b;
    if (quad_a[index->row] < quad_b[index->row])
       return -1;
    else {
        if (quad_a[index->row] > quad_b[index->row])
            return +1;
        else { //same row
            if (quad_a[index->col] < quad_b[index->col])
                return -1;
            else if (quad_a[index->col] > quad_b[index->col])
                return +1;
            else {
                if (quad_a[index->row_bin] > quad_b[index->row_bin])
                    return +1;
                else {
                    if (quad_a[index->row_bin] < quad_b[index->row_bin])
                        return -1;
                    else { //same row
                        if (quad_a[index->col_bin] > quad_b[index->col_bin])
                            return +1;
                        else if (quad_a[index->col_bin] < quad_b[index->col_bin])
                            return -1;
                        return 0;
                   }
               }
            }
        }
    }
}

crs_mmap_mem *crs_mmap_mem_new_fd(int fd) {
    crs_mmap_mem *mem = calloc(1,sizeof(crs_mmap_mem));
    size_t file_size = lseek(fd,0,SEEK_END);
    //log_debug("file size %zu",file_size);
    mem->data = mmap(NULL,
                     file_size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fd, 0);
    mem->len =  file_size;
    mem->mapped = FILE_MAPPED; 
    mem->fd = fd;
    return mem;
}

crs_mmap_mem *crs_mmap_mem_new(size_t size_mmap) {
    crs_mmap_mem *mem = calloc(1,sizeof(crs_mmap_mem));
    mem->data = mmap(NULL,
                     size_mmap,
                     PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_SHARED,
                     -1, 0);
    mem->len =  size_mmap;
    mem->mapped = ANON_MAPPED; 
    return mem;
}

crs_matrix *crs_matrix_new(GPtrArray *temp_values, crs_matrix_index *index_conf) {
    crs_matrix *res = calloc(1,sizeof(crs_matrix));
    res->temp_values = temp_values;
    res->index_conf = index_conf;
    return res;
}


static void crs_matrix_mmap_anon(crs_matrix *matrix, size_t size_index, size_t size_bin) {
    matrix->index = crs_mmap_mem_new(size_index);
    matrix->bin = crs_mmap_mem_new(size_bin);
    //log_debug("memories %p %p",matrix->index->data,matrix->bin->data);
}

static size_t crs_memory(guint rowlist_len,guint n_quads) {
    size_t r = CRS_HEADER + ((rowlist_len + 1) * sizeof(guint)) +  ((rowlist_len + n_quads) * sizeof(fs_rid));
    //log_debug("rowlist %u nquads %u mem %zu",rowlist_len,n_quads,r);
    return r;
}


void crs_matrix_init(unsigned char *mem,guint rowlist_len,guint nquads_len) {
   //log_debug("init crs_matrix_init %u %u",rowlist_len,nquads_len);
    guint *c = (guint *)mem;
    c[0]=CRS_HEADER;
    c[1]=c[0] + nquads_len * sizeof(fs_rid);
    c[2]=c[1] + (rowlist_len * sizeof(fs_rid));
    c[3]=0;
    c[4]=0;
    c[5]=0;
   //log_debug("end crs_matrix_init");
}


guint crs_colind_len(unsigned char *mem) {
    return ((guint *)mem)[COLIND_LEN];
}
guint crs_rowlist_len(unsigned char *mem) {
    return ((guint *)mem)[ROWLIST_LEN];
}
guint crs_rowptr_len(unsigned char *mem) {
    return ((guint *)mem)[ROWPTR_LEN];
}

fs_rid *crs_colind_head(unsigned char *mem) {
   return (fs_rid *) (mem + ((guint *)mem)[COLIND_OFFSET]);
}

fs_rid *crs_colind_tail(unsigned char *mem) {
    return crs_colind_head(mem) + ((guint *)mem)[COLIND_LEN];
}

void crs_matrix_append_colind(unsigned char *mem,fs_rid val) {
    crs_colind_tail(mem)[0]=val;
    ((guint *)mem)[COLIND_LEN]++;
}

fs_rid *crs_rowlist_head(unsigned char *mem) {
    return (fs_rid *) (mem + ((guint *)mem)[ROWLIST_OFFSET]);
}

fs_rid *crs_rowlist_tail(unsigned char *mem) {
    return crs_rowlist_head(mem) + ((guint *)mem)[ROWLIST_LEN];
}

void crs_matrix_append_rowlist(unsigned char *mem,fs_rid val) {
    crs_rowlist_tail(mem)[0]=val;
    ((guint *)mem)[ROWLIST_LEN]++;
}

guint *crs_rowptr_head(unsigned char *mem) {
    return (guint *) (mem + ((guint *)mem)[ROWPTR_OFFSET]);
}

guint *crs_rowptr_tail(unsigned char *mem) {
    return crs_rowptr_head(mem) + ((guint *)mem)[ROWPTR_LEN];
}

void crs_matrix_append_rowptr(unsigned char *mem,guint val) {
    crs_rowptr_tail(mem)[0]=val;
    ((guint *)mem)[ROWPTR_LEN]++;
}

static size_t *crs_memory_sizes(GPtrArray* quads,crs_matrix_index* index_conf,
                                guint *rowlist_index_p,guint *nquads_index_p,
                                GArray *bin_nquads,GArray *bin_rowlists) {


    fs_rid index_col = FS_RID_NULL, index_row = FS_RID_NULL, pre_index_col = FS_RID_NULL, pre_index_row = FS_RID_NULL;
    fs_rid bin_col = FS_RID_NULL, bin_row = FS_RID_NULL, pre_bin_col = FS_RID_NULL, pre_bin_row = FS_RID_NULL;
    
    guint i=0;
    guint n_quads_index = 0,index_row_list = 0;
    guint current_bin_nquads = 0, current_bin_rowlist = 0;
    guint len_quads=quads->len;
    size_t bin_size = 0;
    fs_rid *quad = (fs_rid *) g_ptr_array_index(quads,0);
    while (i < len_quads) {
       
        index_col=quad[index_conf->col];
        index_row=quad[index_conf->row];
        
        if (index_row != pre_index_row)
            index_row_list++;

        n_quads_index++;
        
        pre_bin_col = FS_RID_NULL;
        pre_bin_row = FS_RID_NULL;
        pre_index_col = index_col;
        pre_index_row = index_row;

        /* new bit element */
        current_bin_nquads = 0;
        current_bin_rowlist = 0;

        do {
            bin_col=quad[index_conf->col_bin];
            bin_row=quad[index_conf->row_bin];
            if (pre_bin_row != bin_row || pre_bin_col != bin_col) {

                if (bin_row != pre_bin_row) 
                    current_bin_rowlist++;
                current_bin_nquads++;
                pre_bin_col = bin_col;
                pre_bin_row = bin_row;
            }
            i++;
            if (i<len_quads) {
                quad = g_ptr_array_index(quads,i);
                index_row=quad[index_conf->row];
                index_col=quad[index_conf->col];
            }
        } while (i<len_quads && pre_index_row == index_row &&  pre_index_col == index_col);
        //log_debug("bin size %u %u",current_bin_rowlist,current_bin_nquads);
        bin_size += crs_memory(current_bin_rowlist, current_bin_nquads);
        g_array_append_val(bin_nquads,current_bin_nquads);
        g_array_append_val(bin_rowlists,current_bin_rowlist);
    }

    size_t index_size=crs_memory(index_row_list,n_quads_index);
    size_t *res = calloc(2,sizeof(size_t));
    res[0]=index_size;
    res[1]=bin_size;
    *nquads_index_p = n_quads_index;
    *rowlist_index_p = index_row_list;
    return res;
}

void crs_matrix_build(crs_matrix *crs_m,GError *error) {
    GTimer *ts = g_timer_new();
   
    g_ptr_array_sort_with_data(crs_m->temp_values, (GCompareDataFunc) sort_quads, crs_m->index_conf);
   
    log_debug("SORT time = %.2lf secs [%p]",g_timer_elapsed(ts,NULL),crs_m);
    g_timer_start(ts);
    GArray *bin_rowlists = g_array_new(FALSE,FALSE,sizeof(guint));
    GArray *bin_nquads = g_array_new(FALSE,FALSE,sizeof(guint));
    
    guint index_rowlist_len=0,index_nquads=0;
    size_t *memory_sizes = crs_memory_sizes(crs_m->temp_values,crs_m->index_conf,&index_rowlist_len,&index_nquads,bin_nquads,bin_rowlists);
    log_debug("MEMSIZES time = %.2lf secs [%p] [%zu %zu]",g_timer_elapsed(ts,NULL),crs_m,memory_sizes[0],memory_sizes[1]);
    g_timer_start(ts);
    crs_matrix_mmap_anon(crs_m,memory_sizes[0],memory_sizes[1]);
    crs_matrix_init(crs_m->index->data,index_rowlist_len,index_nquads);
    guint len_quads = crs_m->temp_values->len;
    fs_rid *quad;
    fs_rid index_col = FS_RID_NULL, index_row = FS_RID_NULL, pre_index_col = FS_RID_NULL, pre_index_row = FS_RID_NULL;
    fs_rid bin_col = FS_RID_NULL, bin_row = FS_RID_NULL, pre_bin_col = FS_RID_NULL, pre_bin_row = FS_RID_NULL;
    guint same_row_counter_index=0, same_row_counter_bin=0;

    unsigned char *bin_append = crs_m->bin->data;
    guint bin_matrix_i=0;
    guint current_bin_nquads=0;
    guint current_bin_rowlist=0; 
    guint i=0;
    quad = (fs_rid *) g_ptr_array_index(crs_m->temp_values,0);
    while (i < len_quads) {
       
        index_col=quad[crs_m->index_conf->col];
        index_row=quad[crs_m->index_conf->row];
        
        /* index reg */
        if (index_row != pre_index_row) {
            crs_matrix_append_rowlist(crs_m->index->data,index_row);
            crs_matrix_append_rowptr(crs_m->index->data,same_row_counter_index);
        }
        crs_matrix_append_colind(crs_m->index->data,index_col);
        same_row_counter_index++;
        same_row_counter_bin=0;

        pre_bin_col = FS_RID_NULL;
        pre_bin_row = FS_RID_NULL;
        pre_index_col = index_col;
        pre_index_row = index_row;

        /* new bit element */
        if (i>0)
            bin_append = bin_append + crs_memory(current_bin_rowlist,current_bin_nquads);
        current_bin_nquads = g_array_index(bin_nquads,guint,bin_matrix_i);
        current_bin_rowlist = g_array_index(bin_rowlists,guint,bin_matrix_i);
        bin_matrix_i++;

        crs_matrix_init(bin_append,current_bin_rowlist,current_bin_nquads);

        do {
            bin_col=quad[crs_m->index_conf->col_bin];
            bin_row=quad[crs_m->index_conf->row_bin];
            if (pre_bin_row != bin_row || pre_bin_col != bin_col) {
                if (bin_row != pre_bin_row) {
                    //log_debug("rowlist append bin %llx",bin_row);
                    crs_matrix_append_rowlist(bin_append,bin_row);
                    crs_matrix_append_rowptr(bin_append,same_row_counter_bin);
                }
                crs_matrix_append_colind(bin_append,bin_col);
                same_row_counter_bin++;
                pre_bin_col = bin_col;
                pre_bin_row = bin_row;
            }
            i++;
            if (i<len_quads) {
                quad = g_ptr_array_index(crs_m->temp_values,i);
                index_row=quad[crs_m->index_conf->row];
                index_col=quad[crs_m->index_conf->col];
            }
        } while (i<len_quads && pre_index_row == index_row &&  pre_index_col == index_col);
        crs_matrix_append_rowptr(bin_append,same_row_counter_bin);
    }
    crs_matrix_append_rowptr(crs_m->index->data,same_row_counter_index);

    g_array_free(bin_rowlists,TRUE);
    g_array_free(bin_nquads,TRUE);
    free(memory_sizes);

    for (guint i=0; i< len_quads;i++) {
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
    //log_debug("colind kb_matrix index %u",crs_colind_len(crs_m->index->data));
    //log_debug("colind kb_matrix bin %u",crs_colind_len(crs_m->bin->data));
    g_timer_destroy(ts);
}



size_t crs_mmap_mem_resize(crs_mmap_mem *mem,size_t extra_bytes) {
    //log_debug(" resize to %zu extra_bytes ",extra_bytes);
    size_t new_size = mem->len + extra_bytes;
    if (mem->mapped == ANON_MAPPED) {
        unsigned char *new_data = mmap(NULL,
                            new_size,
                            PROT_READ | PROT_WRITE,
                            MAP_ANON | MAP_SHARED,
                            -1, 0);
        memcpy(new_data,mem->data,mem->len);
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
    } else
        log_info("why resize nothing ??");

    mem->len = new_size;
    return new_size;
    //log_debug("end resize %p new_size %lu",mem->data,new_size);
}



void crs_mmap_mem_cpy(crs_mmap_mem *dest, crs_mmap_mem* orig) {
    if ( orig->len > dest->len) {
        crs_mmap_mem_resize(dest,orig->len - dest->len);
    }
    memcpy(dest->data,orig->data, orig->len);
    dest->len =  orig->len;
}

guint crs_relative_complement_size(fs_rid *a,guint len_a,fs_rid *b,guint len_b) {
    #if 0
    for (guint x=0;x<len_a;x++)
        printf("a[%u]=%llx\n",x,a[x]);
    for (guint x=0;x<len_b;x++)
        printf("b[%u]=%llx\n",x,b[x]);
    #endif
    guint i=0,j=0;
    guint res=0;
    int go_on = 1;
    while(i < len_a) {
        go_on = 1;
        while (j < len_b && go_on) {
            if (b[j] <= a[i]) {
                if (a[i] != b[j])
                    res++;
                j++;
            } else
                go_on = 0;
        }
        i++;
    }
    res += len_b - j;
    return res;
}

guint* crs_merge_size(unsigned char *mem_a,unsigned char *mem_b) {
    guint *res = calloc(2,sizeof(guint));
    res[0] = crs_colind_len(mem_b);
    res[1] = crs_relative_complement_size(crs_rowlist_head(mem_a),crs_rowlist_len(mem_a),crs_rowlist_head(mem_b),crs_rowlist_len(mem_b));
    //log_debug("merge size %u %u",res[0],res[1]);
    return res;
}

void crs_restruct_mem_bin(crs_mmap_mem *mem,unsigned char **bin_cursor_p,guint *new_sizes) {
    guint extra_colind = new_sizes[0];
    guint extra_rowlist = new_sizes[1];

    size_t new_bytes = ((extra_colind + extra_rowlist) * sizeof(fs_rid)) + (extra_rowlist * sizeof(guint));

    unsigned char *bin_kb_cursor = *bin_cursor_p;
    size_t offset_kb_cursor = bin_kb_cursor - mem->data;
    size_t crs_mem_space = crs_memory(crs_rowlist_len(bin_kb_cursor),crs_colind_len(bin_kb_cursor));
    size_t rest_bin = (mem->data + mem->len) - (bin_kb_cursor + crs_mem_space);
    crs_mmap_mem_resize(mem,new_bytes);
    bin_kb_cursor = mem->data + offset_kb_cursor;
    *bin_cursor_p = bin_kb_cursor;
    guint *header = (guint *) bin_kb_cursor;

    /* move rest of bin(s) */
    if (rest_bin) {
        unsigned char *p = (unsigned char *)crs_rowptr_tail(bin_kb_cursor);
        memcpy(p + new_bytes, p, rest_bin);
    }
    /* END move rest of bin(s) */
    unsigned char *cursor =(unsigned char *) crs_rowptr_head(bin_kb_cursor);
    size_t offset=0;
    offset = (extra_colind + extra_rowlist) * sizeof(fs_rid);
    cursor += offset; 
    memcpy(cursor,crs_rowptr_head(bin_kb_cursor),crs_rowptr_len(bin_kb_cursor) * sizeof(guint));
    header[ROWPTR_OFFSET]+=offset;

    cursor =(unsigned char *) crs_rowlist_head(bin_kb_cursor);
    offset = extra_colind * sizeof(fs_rid); 
    cursor += offset;
    memcpy(cursor,crs_rowlist_head(bin_kb_cursor),crs_rowlist_len(bin_kb_cursor) * sizeof(fs_rid));
    header[ROWLIST_OFFSET]+=offset;
}

void crs_restruct_mem(crs_mmap_mem *mem,guint *new_sizes) {
    guint extra_colind = new_sizes[0];
    guint extra_rowlist = new_sizes[1];
    
    //log_debug("extra_colind %u extra_rowlist %u",extra_colind,extra_rowlist);
    size_t new_bytes = ((extra_colind + extra_rowlist) * sizeof(fs_rid)) + (extra_rowlist * sizeof(guint));
    //log_debug("new bytes %zu",new_bytes);
    size_t offset = 0;
    size_t crs_mem_space = crs_memory(crs_rowlist_len(mem->data),crs_colind_len(mem->data));
    if ( mem->len < (crs_mem_space + new_bytes) ) 
        crs_mmap_mem_resize(mem,new_bytes);
   
    guint *header = (guint *) mem->data;

    unsigned char *cursor =(unsigned char *) crs_rowptr_head(mem->data);
    offset = (extra_colind + extra_rowlist) * sizeof(fs_rid);
    cursor += offset; 
    memcpy(cursor,crs_rowptr_head(mem->data),crs_rowptr_len(mem->data) * sizeof(guint));
    header[ROWPTR_OFFSET]+=offset;

    cursor =(unsigned char *) crs_rowlist_head(mem->data);
    offset = extra_colind * sizeof(fs_rid); 
    cursor += offset;
    memcpy(cursor,crs_rowlist_head(mem->data),crs_rowlist_len(mem->data) * sizeof(fs_rid));
    header[ROWLIST_OFFSET]+=offset;
}



void crs_bin_insert_vals(crs_mmap_mem *mem,unsigned char **kb_cursor_p,unsigned char *new_start,size_t new_len){
    unsigned char *kb_cursor = *kb_cursor_p;
    //log_debug("inserting vals ... new_len %zu mem %p kb_cursor %p",new_len,mem->data,kb_cursor);
    size_t pre_cursor_offset = kb_cursor - mem->data;
    size_t shift_bytes = (mem->data + mem->len) - kb_cursor;
    crs_mmap_mem_resize(mem,new_len);
    kb_cursor = mem->data + pre_cursor_offset; 
    //log_debug("pre_cursor_offset %zu shift bytes %zu mem len %zu ",pre_cursor_offset,shift_bytes,mem->len);
    if (shift_bytes>0) 
        memcpy(kb_cursor+new_len,kb_cursor,shift_bytes);
    memcpy(kb_cursor,new_start,new_len);
    *kb_cursor_p = kb_cursor;
}

void crs_colind_insert_vals(unsigned char *mem, fs_rid *vals, guint start, guint len) {
    fs_rid *colind = crs_colind_head(mem);
    memcpy(colind + start + len, colind + start, (crs_colind_len(mem) - start) * sizeof(fs_rid));
    memcpy(colind + start, vals, len * sizeof(fs_rid));
    ((guint *)mem)[COLIND_LEN] += len;
}

void crs_rowlist_insert(unsigned char *mem,guint i,fs_rid val) {
    fs_rid *rowlist = crs_rowlist_head(mem);
    guint len = crs_rowlist_len(mem);
    for(guint x=len;x>i;x--)
        rowlist[x]=rowlist[x-1];
    rowlist[i]=val;
    ((guint *)mem)[ROWLIST_LEN]++;
}

void crs_rowptr_insert(unsigned char *mem,guint i,guint val) {
    guint *rowptr = crs_rowptr_head(mem);
    guint len = crs_rowptr_len(mem);
    for(guint x=len;x>i;x--)
        rowptr[x]=rowptr[x-1];
    rowptr[i]=val;
    ((guint *)mem)[ROWPTR_LEN]++;
}

void crs_array_insert_shift_rid(fs_rid *arr,guint i,guint len,fs_rid val) {
    for(guint x=len-1;x>i && x != G_MAXUINT;x--) {
        //log_debug("arr[%u]=%llx",x,arr[x]);
        arr[x]=arr[x-1];
    }
    arr[i]=val;
}

guint crs_merge_sort_arrays(fs_rid *arr,guint start,guint end,guint arr_end, fs_rid *src, guint src_len) {
    fs_rid rid_dest=FS_RID_NULL;
    fs_rid rid_src=FS_RID_NULL;
    guint y=start;
    guint merged=0;
    for(guint x=0;x<src_len;x++) {
        rid_src = src[x];
        while(y < (end+merged) && (rid_dest=arr[y]) < rid_src)
            y++;
        //if (rid_src != rid_dest) {
            //log_debug("adding !!!!!!!!!!!!!!! %llx y %d end %d merge %d",rid_src,y,arr_end,merged);
            //crs_array_insert_shift_rid(arr,y,end+merged,rid_src);
            merged++; 
            crs_array_insert_shift_rid(arr,y,arr_end+merged,rid_src);
            //end++;
        //}
    }
    return merged;
}

void crs_bit_matrix_merge(crs_mmap_mem *kb_bin_mem,unsigned char **cursor_kb_bin_p,unsigned char *new_bin) {
    
    //log_debug("init BIT Merge kb_bit len %u new_bit len %u",crs_colind_len(*cursor_kb_bin_p),crs_colind_len(new_bin));
    GTimer *ts = g_timer_new();

    guint *index_new_sizes = crs_merge_size(*cursor_kb_bin_p,new_bin);
    crs_restruct_mem_bin(kb_bin_mem,cursor_kb_bin_p,index_new_sizes);

    unsigned char *cursor_kb_bin = *cursor_kb_bin_p; 
    guint row_kb_i=0;
    guint new_rowlist_len=crs_rowlist_len(new_bin);
    for(guint row_new_i=0 ; row_new_i<new_rowlist_len ; row_new_i++) {
        guint col_new_start = crs_rowptr_head(new_bin)[row_new_i];
        guint col_new_end = crs_rowptr_head(new_bin)[row_new_i+1];
        guint col_new_len = col_new_end - col_new_start;
        fs_rid *colind_new_data = crs_colind_head(new_bin) + col_new_start;

        fs_rid row_new_rid = crs_rowlist_head(new_bin)[row_new_i]; 
        fs_rid row_kb_rid = FS_RID_NULL;

        while( row_kb_i < crs_rowlist_len(cursor_kb_bin) && 
              (( row_kb_rid=crs_rowlist_head(cursor_kb_bin)[row_kb_i] ) < row_new_rid))
            row_kb_i++;
       
       if (row_kb_i >= crs_rowlist_len(cursor_kb_bin))
            row_kb_rid = FS_RID_NULL;
       
       guint new_coldinds = 0;
       if (row_kb_rid != row_new_rid) {
            /* append new row_rid */
            crs_rowlist_insert(cursor_kb_bin,row_kb_i,row_new_rid);
            
            /* new ptr entry with init value same as next ptr */
            guint row_ptr_kb = crs_rowptr_head(cursor_kb_bin)[row_kb_i];
            crs_rowptr_insert(cursor_kb_bin,row_kb_i,row_ptr_kb);
            
            /* colind(row_new_rid) doesn't exist so we insert all in one go in the middle of row_ptr_kb */
            crs_colind_insert_vals(cursor_kb_bin, colind_new_data, row_ptr_kb, col_new_len);
            new_coldinds =  col_new_len;

       } else {

           /*for (guint zzz=0;zzz<col_new_len;zzz++)
                printf("new [%u] %016llx\n",zzz,colind_new_data[zzz]);
       fs_rid *zz = crs_colind_head(cursor_kb_bin);
       for (guint zzz=0;zzz<crs_colind_len(cursor_kb_bin);zzz++)
            printf("before [%u] %016llx\n",zzz,zz[zzz]);
*/
            /* we just need to merge colind and inc ptr(s) */
            guint col_kb_start = crs_rowptr_head(cursor_kb_bin)[row_kb_i];
            guint col_kb_end = crs_rowptr_head(cursor_kb_bin)[row_kb_i+1];
            guint len_colind = ((guint *)cursor_kb_bin)[COLIND_LEN];
            new_coldinds = crs_merge_sort_arrays(crs_colind_head(cursor_kb_bin),col_kb_start,col_kb_end,len_colind,colind_new_data,col_new_len);
            ((guint *)cursor_kb_bin)[COLIND_LEN] += new_coldinds;

#if 0
{//debug
fs_rid *zz = crs_rowlist_head(cursor_kb_bin);
for (guint zzz=0;zzz<crs_rowlist_len(cursor_kb_bin);zzz++)
printf("rowlist [%u] %016llx\n",zzz,zz[zzz]);
}//debug
#endif 
       }

       /* inc ptr values as many times as new coldinds from row_kb_i */ 
       guint *rowptr = crs_rowptr_head(cursor_kb_bin);
       guint rowptr_len = crs_rowptr_len(cursor_kb_bin);
       for(guint x=row_kb_i+1;x<rowptr_len;x++)
           rowptr[x] += new_coldinds;

    }
    //log_debug("End BIT merge time = %.2lf",g_timer_elapsed(ts,NULL));

}
crs_matrix *crs_matrix_merge(crs_matrix *kb_matrix, crs_matrix *new_matrix) {

    log_debug("merge %p len_orig %zu len dest %zu",kb_matrix->index->data,new_matrix->index->len, kb_matrix->index->len);

    if (!kb_matrix->index->len) {
        /* kb empty first import just copy */
        crs_mmap_mem_cpy(kb_matrix->index,new_matrix->index);
        crs_mmap_mem_cpy(kb_matrix->bin,new_matrix->bin);
        return kb_matrix;
    }
   
    log_debug("mergin data ...");
    GTimer *ts = g_timer_new();

    guint *index_new_sizes = crs_merge_size(kb_matrix->index->data,new_matrix->index->data);
    crs_restruct_mem(kb_matrix->index,index_new_sizes);
    
    crs_mmap_bin_iterator *kb_bin_it = crs_mmap_bin_iterator_new();
    crs_mmap_bin_iterator_init(kb_bin_it,kb_matrix->bin);
    crs_mmap_bin_iterator_next(kb_bin_it);

    crs_mmap_bin_iterator *new_bin_it = crs_mmap_bin_iterator_new();
    crs_mmap_bin_iterator_init(new_bin_it,new_matrix->bin);
    crs_mmap_bin_iterator_next(new_bin_it);

    guint row_kb_i=0;
    for(guint row_new_i=0 ; row_new_i< crs_rowlist_len(new_matrix->index->data) ; row_new_i++) {
        guint col_new_start = crs_rowptr_head(new_matrix->index->data)[row_new_i];
        guint col_new_end = crs_rowptr_head(new_matrix->index->data)[row_new_i+1];
        guint col_new_len = col_new_end - col_new_start;
        fs_rid *colind_new_data = crs_colind_head(new_matrix->index->data) + col_new_start;

        log_debug("merge col_new [%d,%d] col_new_len %d new_bin_it->i %d ", col_new_start, col_new_end, col_new_len,new_bin_it->i);
        fs_rid row_new_rid = crs_rowlist_head(new_matrix->index->data)[row_new_i];
        fs_rid row_kb_rid = FS_RID_NULL;
        guint kb_rowlist_len = crs_rowlist_len(kb_matrix->index->data);
        fs_rid *kb_rowlist_head = crs_rowlist_head(kb_matrix->index->data);

        while( row_kb_i < kb_rowlist_len && 
              (( row_kb_rid=kb_rowlist_head[row_kb_i] ) < row_new_rid))
            row_kb_i++;
        //log_debug("row_kb_i %d",row_kb_i);

        if (row_kb_i >= kb_rowlist_len)
            row_kb_rid = FS_RID_NULL;

        guint new_coldinds = 0;
        guint row_ptr_kb = crs_rowptr_head(kb_matrix->index->data)[row_kb_i];
        if (row_kb_rid != row_new_rid) {
            //log_debug("append new row %llx",row_new_rid);
            /* append new row_rid */
            //log_debug("new row append %d %llx",row_kb_i,row_new_rid);
            crs_rowlist_insert(kb_matrix->index->data,row_kb_i,row_new_rid);

            /* new ptr entry with init value same as next ptr */
            //log_debug("new row ptr %d %d",row_kb_i,row_ptr_kb);
            crs_rowptr_insert(kb_matrix->index->data,row_kb_i,row_ptr_kb);

            /* colind(row_new_rid) doesn't exist so we insert all in one go in the middle of row_ptr_kb */
            //log_debug("new pos %d n %d %llx",row_ptr_kb,col_new_len,colind_new_data[0]);
            crs_colind_insert_vals(kb_matrix->index->data, colind_new_data, row_ptr_kb, col_new_len);
            
            size_t new_bin_len = 0;
            unsigned char *bits_new = new_bin_it->cursor;
            //log_debug("new_bin_it %d col_new_len %d",new_bin_it->i,col_new_len);
            for(int zz=0;zz<col_new_len;zz++) {
                new_bin_len += crs_memory(crs_rowlist_len(new_bin_it->cursor),crs_colind_len(new_bin_it->cursor));
                crs_mmap_bin_iterator_next(new_bin_it);
            }
            
            while(kb_bin_it->i < row_ptr_kb) {
                log_debug(" %d %u %d",kb_bin_it->i,row_ptr_kb,kb_bin_it->i < row_ptr_kb);
                crs_mmap_bin_iterator_next(kb_bin_it);
            }
            //log_debug("new_bin_len %lu",new_bin_len);
            crs_bin_insert_vals(kb_matrix->bin,&kb_bin_it->cursor,bits_new,new_bin_len);

            new_coldinds =  col_new_len;
       } else {
            //log_debug("mergin new row %llx",row_new_rid);
            /* SAME elts in rowlist, we just need to merge colind and inc ptr(s) */
            guint col_kb_start = crs_rowptr_head(kb_matrix->index->data)[row_kb_i];
            guint col_kb_end =  crs_rowptr_head(kb_matrix->index->data)[row_kb_i+1]; 
            //log_debug("col_kb[%d,%d]",col_kb_start,col_kb_end);
            fs_rid col_new_rid=FS_RID_NULL;
            fs_rid col_kb_rid=FS_RID_NULL;

            guint y = col_kb_start;
            guint x = col_new_start;
            while( x < col_new_end ) {
                col_new_rid = crs_colind_head(new_matrix->index->data)[x];

                while(y < col_kb_end && ((col_kb_rid=crs_colind_head(kb_matrix->index->data)[y]) < col_new_rid))
                    y++;
                //log_debug("col_kb_rid[%d] %llx col_new_rid[%d] %llx ", y, col_kb_rid, x, col_new_rid);

                if (col_new_rid != col_kb_rid) {
                    //log_debug("different cols at index");

                    guint start = x - col_new_start;
                    guint len_shifts = 1;
                    if (y < col_kb_end) {
                        while(x < (col_new_end-1) && (crs_colind_head(new_matrix->index->data)[x] < col_kb_rid)) { 
                            x++;
                            len_shifts++;
                        }
                        //log_debug("len_shifts %d x %d",len_shifts,x);
                    } else {
                        len_shifts = col_new_len - start;
                        x = col_new_end;
                    }
                    crs_colind_insert_vals(kb_matrix->index->data,colind_new_data + start,y, len_shifts);

                    size_t new_bin_len = 0;
                    //log_debug("new_bin_it->i %d x %d start %d",new_bin_it->i,x,start);
                    unsigned char *bits_new = new_bin_it->cursor; 
                    for(int zz=0;zz<len_shifts;zz++) {
                    //while(new_bin_it->i < x+1) {
                        new_bin_len += crs_memory(crs_rowlist_len(new_bin_it->cursor),crs_colind_len(new_bin_it->cursor));
                        crs_mmap_bin_iterator_next(new_bin_it);
                    }
                    //log_debug("new_bin_len %lu",new_bin_len);
                    while(kb_bin_it->i < y) crs_mmap_bin_iterator_next(kb_bin_it);
                    //while(kb_bin_it->i < col_kb_start+1) crs_mmap_bin_iterator_next(kb_bin_it);

                    crs_bin_insert_vals(kb_matrix->bin,&kb_bin_it->cursor,bits_new,new_bin_len);

                    new_coldinds += len_shifts;

                } else {
                    //log_debug("merge bit %d %d row_ptr_kb %u col_new_start %u y %u",kb_bin_it->i,new_bin_it->i,row_ptr_kb,col_new_start,y);
                    /* the bit matrix already exists we should merge bit matrixes */
                    while(kb_bin_it->i < y) crs_mmap_bin_iterator_next(kb_bin_it);
                    crs_bit_matrix_merge(kb_matrix->bin,&kb_bin_it->cursor,new_bin_it->cursor);
                    crs_mmap_bin_iterator_next(new_bin_it); /* to cursor forward to next new_bin */
                }

                x++;
            }
        }
        guint *rowptr = crs_rowptr_head(kb_matrix->index->data);
        guint rowptr_len = crs_rowptr_len(kb_matrix->index->data);
        for(guint x=row_kb_i+1;x<rowptr_len;x++)
            rowptr[x] += new_coldinds;
    }
    
    crs_matrix_free(new_matrix);
    log_debug("End merge time = %.2lf",g_timer_elapsed(ts,NULL));
    return kb_matrix;
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

void crs_mmap_mem_sync(crs_mmap_mem *mem) {
    msync(mem->data,mem->len,MS_ASYNC);
}

void crs_matrix_sync(crs_matrix *matrix,GError **error) {
    /* TODO: persist blocking access when import/query/update */
    crs_mmap_mem_sync(matrix->index);
    crs_mmap_mem_sync(matrix->bin);
}

void crs_mmap_mem_destroy(crs_mmap_mem *mem) {
    munmap(mem->data,mem->len);
    if(mem->mapped == FILE_MAPPED)
        close(mem->fd);
    free(mem);
}

void crs_matrix_free(crs_matrix *crs_m) {

    crs_mmap_mem_sync(crs_m->index);
    crs_mmap_mem_destroy(crs_m->index);
    
    crs_mmap_mem_sync(crs_m->bin);
    crs_mmap_mem_destroy(crs_m->bin);
}


size_t crs_size_ptr(unsigned char *mem) {
    return ((unsigned char *)crs_rowptr_tail(mem)) - mem;
}

crs_mmap_bin_iterator *crs_mmap_bin_iterator_new() {
     crs_mmap_bin_iterator *iterator = calloc(1,sizeof(crs_mmap_bin_iterator));
     iterator->cursor = NULL;
     return iterator;
}

void crs_mmap_bin_iterator_init(crs_mmap_bin_iterator *iterator, crs_mmap_mem *bin_mem) {
     iterator->bin_mem = bin_mem;
     iterator->offset = 0;
     iterator->cursor = bin_mem->data;
     iterator->i = -1;
}

int crs_mmap_bin_iterator_has_next(crs_mmap_bin_iterator *it) {
    return it->colind_len > 0;
}

void crs_mmap_bin_iterator_next(crs_mmap_bin_iterator *it) {
//    log_debug("cursor %p it->i %d",it->cursor,it->i);
    if (it->i != -1)
        it->cursor = (unsigned char *) crs_rowptr_tail(it->cursor);
    it->i++;
    
    it->colind_len = crs_colind_len(it->cursor);
    it->colind_head = crs_colind_head(it->cursor);

    it->rowptr_len = crs_rowptr_len(it->cursor);
    it->rowptr_head = crs_rowptr_head(it->cursor);

    it->rowlist_len = crs_rowlist_len(it->cursor);
    it->rowlist_head = crs_rowlist_head(it->cursor);
//    log_debug("cursor %p",it->cursor);
}

void crs_mmap_bin_iterator_free(crs_mmap_bin_iterator *i) {
    free(i);
}
