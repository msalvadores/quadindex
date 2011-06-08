#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "crs-matrix-list.h"
#include "crs-matrix.h"
#include "common/hash.h"
#include "common/error.h"

#define crs_matrix_inverse_list_get_key(l,i) (l->data[2*i])
#define crs_matrix_inverse_list_get_value(l,i) (l->data[(2*i)+1])
#define crs_matrix_inverse_list_set_key(l,i,v) (l->data[2*i]=v)
#define crs_matrix_inverse_list_set_value(l,i,v) (l->data[(2*i)+1]=v)
#define crs_list_collapsed_set_key(c,i,k) ( ( (fs_rid *)(c->keys + (i * (sizeof(fs_rid) + sizeof(guint))) ))[0]=k)
#define crs_list_collapsed_set_key_len(c,i,k) ( ((guint *) ( c->keys + ((i * (sizeof(fs_rid) + sizeof(guint))) + sizeof(fs_rid)) ))[0]=k)
#define crs_list_collapsed_get_key(c,i) ( ( (fs_rid *)(c->keys + (i * (sizeof(fs_rid) + sizeof(guint))) )) [0])
#define crs_list_collapsed_get_key_len(c,i) ( ((guint *) ( c->keys + ((i * (sizeof(fs_rid) + sizeof(guint))) + sizeof(fs_rid)) ))[0])
#define crs_list_collapsed_append_count(c,i) ( ((guint *) (c->data + c->data_offset_last))[0] = i )
#define crs_list_collapsed_key_size(i) (i->key_i+1)

crs_matrix_inverse_list *crs_matrix_inverse_list_new() {
    crs_matrix_inverse_list *res = malloc(sizeof(crs_matrix_inverse_list));
    res->alloc_len=ALLOC_SLOT_INVERSE_LIST;
    res->data = calloc(ALLOC_SLOT_INVERSE_LIST, 2 * sizeof(fs_rid));
    res->last = 0;
    return res;
}

void crs_matrix_inverse_list_append(crs_matrix_inverse_list *il,fs_rid k,fs_rid v) {
    if (il->last == il->alloc_len) {
        realloc(il->data, (il->alloc_len + ALLOC_SLOT_INVERSE_LIST) * 2 *  sizeof(fs_rid));
        il->alloc_len += ALLOC_SLOT_INVERSE_LIST;
    }
    il->data[il->last*2]=k;
    il->data[(il->last*2)+1]=v;
    il->last++;
    il->sorted=0;
}

static void crs_matrix_inverse_list_swap(crs_matrix_inverse_list *il,int a, int b) {
    fs_rid tmp[2] = {FS_RID_NULL,FS_RID_NULL};
    tmp[0]=crs_matrix_inverse_list_get_key(il,a);
    tmp[1]=crs_matrix_inverse_list_get_value(il,a);
    crs_matrix_inverse_list_set_key(il,a,crs_matrix_inverse_list_get_key(il,b));
    crs_matrix_inverse_list_set_value(il,a,crs_matrix_inverse_list_get_value(il,b));
    crs_matrix_inverse_list_set_key(il,b,tmp[0]);
    crs_matrix_inverse_list_set_value(il,b,tmp[1]);
}


/* INIT crs_list_collapsed_iterator */
crs_list_collapsed_iterator *crs_list_collapsed_iterator_new(crs_list_collapsed *ic) {
    crs_list_collapsed_iterator *res = malloc(sizeof(crs_list_collapsed_iterator)); 
    res->ic = ic;
    res->key_i = 0;
    res->key = FS_RID_NULL;
    res->len = 0;
    res->i = 0;
    res->pdata = (guint *)ic->data;
    return res;
}

void crs_list_collapsed_iterator_free(crs_list_collapsed_iterator *it) {
    free(it);
}

int crs_list_collapsed_iterator_next(crs_list_collapsed_iterator *it) {
    if (it->i >= it->len && it->key != FS_RID_NULL) {
        if (!(it->key_i < crs_list_collapsed_key_size(it->ic)))
            return 0;
    }
    if (!(it->i < it->len)) {
       if (it->key != FS_RID_NULL) {
           it->pdata += it->len;
           it->key_i++;
       }
       it->key = crs_list_collapsed_get_key(it->ic,it->key_i);
       it->len = crs_list_collapsed_get_key_len(it->ic,it->key_i);
       it->i = 0;
    }
    it->val = it->pdata[it->i];
    it->i++;
    return 1;
}
/* END crs_list_collapsed_iterator */

static void crs_matrix_inverse_list_dump(crs_matrix_inverse_list *il) {
    for (int i = 0;i<(int)il->last;i++) {
        printf("[crs_matrix_inverse_list_dump] [%i] %llx %lld\n",i,crs_matrix_inverse_list_get_key(il,i),crs_matrix_inverse_list_get_value(il,i));
    }
}

static void crs_matrix_collapse_list_dump(crs_list_collapsed *ic) {
    crs_list_collapsed_iterator *it = crs_list_collapsed_iterator_new(ic);
   
    fs_rid prev = FS_RID_NULL;
    while(crs_list_collapsed_iterator_next(it)) {
        if (prev != it->key) {
            printf("\n[crs_matrix_collapse_list_dump] [%i] [%llx * %i]",it->key_i,it->key,it->len);
            prev = it->key;
        }
        printf(" %u",it->val);
        
    }
    printf("\n");
    crs_list_collapsed_iterator_free(it);
}

void crs_matrix_inverse_list_quicksort(crs_matrix_inverse_list *il,int left,int right) {
    //log_debug("left %d right %d",left,right);
    if ((right-left) > 0) {
        int pivot = (right + left) / 2;
        int left_idx = left;
        int right_idx = right;
        while(left_idx <= pivot && right_idx >= pivot) {
            while(crs_matrix_inverse_list_get_key(il,left_idx) < crs_matrix_inverse_list_get_key(il,pivot) && left_idx <= pivot)
                left_idx++;
            while(crs_matrix_inverse_list_get_key(il,right_idx) > crs_matrix_inverse_list_get_key(il,pivot) && right_idx >= pivot)
                right_idx--;
            if (right_idx != left_idx)
                crs_matrix_inverse_list_swap(il,right_idx,left_idx);
            left_idx++;
            right_idx--;
            if (left_idx -1 == pivot) {
                pivot = ++right_idx;
            } else if (right_idx +1 == pivot) {
                pivot = --left_idx;
            }

        }
        crs_matrix_inverse_list_quicksort(il,left,pivot-1);
        crs_matrix_inverse_list_quicksort(il,pivot+1,right);
    }
}

void crs_matrix_inverse_list_sort(crs_matrix_inverse_list *il) {
    crs_matrix_inverse_list_quicksort(il,0,(int) (il->last-1));
    il->sorted = 1;
    crs_matrix_inverse_list_dump(il);
}

void crs_list_collapsed_append_key(crs_list_collapsed *ic, fs_rid key) {
    log_debug("append key %llx",key);
    if (ic->keys_alloc <= ic->key_i + 1 || ic->key_i == G_MAXUINT) {
        ic->keys_alloc += ALLOC_SLOT_INVERSE_LIST;
        ic->keys = realloc(ic->keys, ic->keys_alloc * (sizeof(guint) + sizeof(fs_rid)));
        if (ic->key_i == G_MAXUINT) ic->key_i = 0;
    } else
        ic->key_i++;
    crs_list_collapsed_set_key(ic,ic->key_i,key);
    crs_list_collapsed_set_key_len(ic,ic->key_i,0);
}

void crs_list_collapsed_inc_key_len(crs_list_collapsed *ic, guint i) {
   crs_list_collapsed_set_key_len(ic,i,crs_list_collapsed_get_key_len(ic,i)+1);
}

void crs_list_collapsed_append_value(crs_list_collapsed *ic,guint val) {
    
    if (ic->data_offset_last >= ic->data_alloc) {
         ic->data_alloc += ALLOC_SLOT_INVERSE_LIST_DATA;
         ic->data = realloc(ic->data, ic->data_alloc * sizeof(guint));
    }
    crs_list_collapsed_inc_key_len(ic,ic->key_i);
    crs_list_collapsed_append_count(ic,val);
    ic->data_offset_last += sizeof(guint);
    log_debug("val %u",val);
}

crs_list_collapsed* crs_list_collapsed_new(crs_matrix_inverse_list *il) {
    if (!il->sorted) {
        crs_matrix_inverse_list_sort(il);
    }
    crs_list_collapsed *res = malloc(sizeof(crs_list_collapsed));
    fs_rid prev = FS_RID_NULL;
    fs_rid key = FS_RID_NULL;
    guint val=0;
    res->key_i = G_MAXUINT;
    res->keys_alloc = 0;
    res->data_offset_last = 0;
    res->data_alloc = 0;
    for (int i = 0;i<(int)il->last;i++) {
        key=crs_matrix_inverse_list_get_key(il,i);
        val=crs_matrix_inverse_list_get_value(il,i);
        if (prev != key) {
            crs_list_collapsed_append_key(res,key);
            prev = key;
            //SORT PREVIOUS
        }
        crs_list_collapsed_append_value(res,val);
    }
    crs_matrix_collapse_list_dump(res);
    return res;
}

void crs_matrix_inverse_list_free(crs_matrix_inverse_list *il) {
    free(il->data);
    free(il);
}
