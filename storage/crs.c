#include <raptor.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <glib.h>
#include <google/heap-profiler.h>
#include <kclangc.h>

#include "common/hash.h"
#include "common/error.h"
#include "common/params.h"

#include "rdfindex.h"


static GTimer *global_time = NULL;
static GTimer *partial_time = NULL;

static GThreadPool* pool;

int segments[SEGMENTS];
int graphs = 0;
char tmp_quad[QUAD_SIZE+1];

fs_rid global_g_rid;
unsigned char * global_g_s;
static KCDB* db_hashes[HASHES_NUM];

struct _index_stmt {
    fs_rid gr;
    fs_rid sr;
    fs_rid pr;
    fs_rid or;

//    gchar *gc;
//    gchar *sc;
//    gchar *pc;
//    gchar *oc;
};

typedef struct _index_stmt index_stmt;
typedef index_stmt* pointer_stmt;

struct _rid_2d {
    fs_rid level1;
    fs_rid level2;
};

typedef struct _rid_2d rid_2d;


struct _fold_stmt {
    //fs_rid col_id;
    //fs_rid row_id;
    GPtrArray *rids_2d;
};
typedef struct _fold_stmt fold_stmt;


struct _import_data {
     GPtrArray** import_segment;
     int counter;
};

typedef struct _import_data import_data;


struct _crs_segment {
    GPtrArray *fold_stmt_arr;
    GArray *colind;
    GArray *rowlist;
    GPtrArray *rowptr;
};

typedef struct _crs_segment crs_segment;

struct _crs_construction_input {
    crs_segment **segment;
    GPtrArray* import_segment;
};
typedef struct _crs_construction_input crs_construction_input;


raptor_uri *get_uri_from_path(raptor_world* world,char* path) {
    unsigned char *uri_string;
    raptor_uri *uri;
    uri_string = raptor_uri_filename_to_uri_string(path);
    uri=raptor_new_uri(world,uri_string);
    raptor_free_memory(uri_string);
    return uri;
}

guint compare_stmt(gconstpointer a, gconstpointer b) {
    index_stmt *stmt_a = *(index_stmt **) a; 
    index_stmt *stmt_b = *(index_stmt **) b;
    if (stmt_a->pr < stmt_b->pr)
        return -1;
    else {
        if (stmt_a->pr > stmt_b->pr)
            return +1;
        else {
             if (stmt_a->or < stmt_b->or)
                return -1;
             else 
                return +1;
        }
    }
}

gchar *my_str_dup(const gchar *s) {
     gchar *cpy = malloc((strlen(s)+1)*sizeof(gchar));
     strcpy(cpy,s);
     return cpy;
}

pointer_stmt new_index_stmt(fs_rid g_rid,fs_rid s_rid,fs_rid p_rid,fs_rid o_rid,
unsigned char *gc,unsigned char *sc,unsigned char *pc,unsigned char *oc) {

   pointer_stmt istmt = calloc(1,sizeof(index_stmt));
   istmt->gr = g_rid;
   istmt->sr = s_rid;
   istmt->pr = p_rid;
   istmt->or = o_rid;
   /*
   istmt->gc = my_str_dup((const gchar *) gc);
   istmt->sc = my_str_dup((const gchar *) sc);
   istmt->pc = my_str_dup((const gchar *) pc);
   istmt->oc = my_str_dup((const gchar *) oc);
   */
   return istmt;
}

void istmt_free_func(gpointer data) {
    /*pointer_stmt istmt = (pointer_stmt) data;
    free(istmt->gc);
    free(istmt->sc);
    free(istmt->pc);
    free(istmt->oc);
    */
}

rid_2d *new_rid_2d(fs_rid level1, fs_rid level2) {
    rid_2d *res = NULL;
    res = calloc(1,sizeof(rid_2d));
    res->level1=level1;
    res->level2=level2;
    return res;
}

fold_stmt *new_fold_stmt_init(pointer_stmt pstmt) {
     fold_stmt *res = NULL;
     res = calloc(1,sizeof(fold_stmt));
     //res->col_id = pstmt->or;
     //res->row_id = pstmt->pr;
     res->rids_2d = g_ptr_array_new();
     rid_2d *tuple = new_rid_2d(pstmt->sr,pstmt->gr);
     g_ptr_array_add(res->rids_2d, tuple);
     return res;
}


void append_2d(fold_stmt *f,pointer_stmt s) {
    rid_2d *tuple = new_rid_2d(s->sr,s->gr);
    g_ptr_array_add(f->rids_2d,tuple);
}


fold_stmt *update_crs_segment(crs_segment *segment,pointer_stmt pstmt) {
     int inc_row_ptr=0;
     fold_stmt *fs = new_fold_stmt_init(pstmt);
     if (segment->fold_stmt_arr->len > 0) {
        fs_rid last_row_pr = g_array_index(segment->rowlist,fs_rid,segment->rowlist->len-1);
        inc_row_ptr = (pstmt->pr == last_row_pr);
     }
     g_ptr_array_add(segment->fold_stmt_arr,fs);
     g_array_append_val(segment->colind,pstmt->or);
     if (inc_row_ptr) {
         guint last_index = segment->rowptr->len-1;
         guint* val_row = (guint *) g_ptr_array_index(segment->rowptr,last_index);
         (*val_row)++;
     } else {
        g_array_append_val(segment->rowlist,pstmt->pr);
        guint *x=NULL;
        x = malloc(sizeof(guint));
        *x=1;
         g_ptr_array_add(segment->rowptr,x);
     }
     return fs;
}

void crs_construction_worker(gpointer worker_data, gpointer user_data) {
    /* START init vars */ 
    crs_construction_input *input = (crs_construction_input *) worker_data;
    GPtrArray *data = input->import_segment;
    crs_segment **crs_res = input->segment;
    crs_segment *crs = calloc(1,sizeof(crs_segment));
    *crs_res = crs;
    
    crs->colind = g_array_new(FALSE, FALSE, sizeof(fs_rid));
    crs->rowlist = g_array_new(FALSE, FALSE, sizeof(fs_rid));
    crs->rowptr = g_ptr_array_new();
    crs->fold_stmt_arr = g_ptr_array_new();
    /* END init vars */ 

    GTimer *ts = g_timer_new();
    g_ptr_array_sort (data, (GCompareFunc) compare_stmt);
    log_debug("SORT time = %.2lf secs [%p]",g_timer_elapsed(ts,NULL),data);
    g_timer_start(ts);
    fold_stmt *prev_fold_stmt = NULL;
    pointer_stmt pstmt = NULL;
    fs_rid prev_pr = 0x0;
    fs_rid prev_or = 0x0;
    for (guint z=0 ;z<data->len;z++) {
        pstmt = (pointer_stmt) g_ptr_array_index(data,z);
        if (prev_fold_stmt != NULL && 
            pstmt->pr == prev_pr && pstmt->or == prev_or) {
            append_2d(prev_fold_stmt,pstmt);
        } else {
            prev_fold_stmt = update_crs_segment(crs,pstmt);
            prev_pr = pstmt->pr;
            prev_or = pstmt->or;
        }
        //istmt_free_func(pstmt);
        free(pstmt);
    }
    g_ptr_array_free(data,TRUE);
    log_debug ("CRS time = %.2lf secs [%p]",g_timer_elapsed(ts,NULL),worker_data);
    g_timer_destroy(ts);
}



void statement_handler(void* user_data, const raptor_statement* st) {
   raptor_term *g, *s, *p, *o;
  
   g = st->graph;
   s = st->subject;
   p = st->predicate;
   o = st->object;
  
   import_data *idata = (import_data *) user_data;

   idata->counter++;

   if (idata->counter == 0)
       g_timer_start(partial_time);

   /* init index logic */
   unsigned char *gc = NULL;
   fs_rid g_rid;
   if (graphs) {
       gc = raptor_uri_as_string(g->value.uri);
       g_rid = fs_hash_uri((const char *)gc);
   } else {
       g_rid = global_g_rid; 
       gc = global_g_s;
   }

   unsigned char *sc = raptor_uri_as_string(s->value.uri);
   unsigned char *pc = raptor_uri_as_string(p->value.uri);
   unsigned char *oc = NULL;
   unsigned char *o_lang = NULL;
   unsigned char *o_datatype = NULL;
   fs_rid s_rid = fs_hash_uri((const char *) sc);
   fs_rid p_rid = fs_hash_uri((const char *) pc);
   fs_rid o_rid = 0x0;
   if (o->type == RAPTOR_TERM_TYPE_URI) {
       oc = raptor_uri_as_string(o->value.uri);
       o_rid = fs_hash_uri((const char *) oc);
   } else if (o->type == RAPTOR_TERM_TYPE_LITERAL) {
        oc = o->value.literal.string;
        if (o->value.literal.datatype) {
            o_datatype = raptor_uri_as_string(o->value.literal.datatype);
            o_rid = fs_hash_literal((const char *) oc,fs_hash_uri((const char *) o_datatype));
        } else if (o->value.literal.language != NULL) {
            o_lang = o->value.literal.language;
            o_rid = fs_hash_literal((const char *) oc, fs_hash_uri((const char *) o_lang));
        } else {
            o_rid = fs_hash_literal((const char *) oc, FS_RID_NULL);
        }
        oc =  raptor_term_as_string(o);
   } else if (o->type == RAPTOR_TERM_TYPE_BLANK) {
    //not for the moment
   }
   
   int seg_id = s_rid % SEGMENTS;
   segments[seg_id]++;
   pointer_stmt istmt = new_index_stmt(g_rid,s_rid,p_rid,o_rid,gc,sc,pc,oc);
   g_ptr_array_add(idata->import_segment[seg_id],istmt);
  
   fs_rid hashes[4] = {g_rid,s_rid,p_rid,o_rid};
   char tmp_rid[16+1];
   unsigned char *strings[4] = {gc,sc,pc,oc};
   int assigned_hash=0;
   for(int i=0;i<4;i++) {
       sprintf(tmp_rid,"%llx",hashes[i]);
       assigned_hash = hashes[i] % HASHES_NUM;
       kcdbset(db_hashes[assigned_hash],(const char *) tmp_rid, 17*sizeof(char) ,(const char *) strings[i], strlen((const char *) strings[i]));
   }

   if (!(idata->counter % STAT_BATCH)) {
        double kt = idata->counter/1e3;
        log_debug("parsing progress %.0lf kT %.2lf kT/s %.2lf kT/s",kt,kt/g_timer_elapsed(global_time,NULL), 
                (STAT_BATCH/1e3)/g_timer_elapsed(partial_time,NULL));
        fflush(stdout);
        g_timer_start(partial_time);
   }
}

int main(int argc,char** args) {
    //log_debug("%llu\n",ULONG_LONG_MAX);
    if (argc < 3) {
        printf("usage: rdfindex format file_path\n");
        exit(-1);
    }
    char *file_path = args[2];
    FILE *stream = fopen(file_path,"r");
    if (!stream) {
        log_debug("error: [%s] file not found",file_path);
        exit(-1);
    }
    fs_hash_init();
    g_thread_init(NULL);
    
    graphs = !strcmp(args[1],"trig");
    if (!graphs) {
        if (argc > 3) {
            global_g_rid = fs_hash_uri(args[3]);
            global_g_s = (unsigned char *) args[3];
        } else {
            printf("error: if format is not trig 4th param should be graph\n");
            exit(-1);
        }
    }
    
    log_debug("[%s] parsing ... \n",file_path);
    global_time = g_timer_new();
    partial_time = g_timer_new();


    GPtrArray* import_segment[SEGMENTS];

    for(int j=0;j<SEGMENTS;j++) {
        import_segment[j] = g_ptr_array_new();
    }
    for(int j=0;j<HASHES_NUM;j++) {
         db_hashes[j] = kcdbnew();
         if (!kcdbopen(db_hashes[j],  g_strdup_printf(HASHES_NAME,"KB_NAME",j) , KCOWRITER | KCOCREATE)) {
         //if (!kcdbopen(db_hashes[j],  "*" , KCOWRITER | KCOCREATE)) {
            fprintf(stderr, "open error: %s\n", kcecodename(kcdbecode(db_hashes[j])));
            exit(-1);
         }
    }

    import_data idata = { .counter = 0, .import_segment = import_segment };

    log_info("import_segment init");
    raptor_world* world;
    world = raptor_new_world(); 
    raptor_uri *uri = get_uri_from_path(world,file_path);
    raptor_uri *base_uri = raptor_uri_copy(uri);

    raptor_parser* rdf_parser;
    rdf_parser = raptor_new_parser(world, args[1]);
    raptor_parser_set_option(rdf_parser, RAPTOR_OPTION_NO_NET, NULL, 1);

    int counter = 0;
    raptor_parser_set_statement_handler(rdf_parser, &idata , (raptor_statement_handler) statement_handler);

    raptor_parser_parse_start(rdf_parser, base_uri);

    unsigned char *buffer;
    buffer = (unsigned char*) malloc (sizeof(char)*READ_BUF_SIZE);
    size_t n;

    while((n= fread(buffer,1, READ_BUF_SIZE,stream))) {
        raptor_parser_parse_chunk(rdf_parser, buffer, n, 0);
    }
    raptor_parser_parse_chunk(rdf_parser, NULL, 0, 1);
    
    raptor_free_uri(base_uri);
    raptor_free_uri(uri);
    raptor_free_parser(rdf_parser);
    raptor_free_world(world);
    free(buffer);
    fclose(stream);

    log_info("finished parsed. init crs construct ... ");
    crs_segment *crs_segments[SEGMENTS];
    pool = g_thread_pool_new(crs_construction_worker, NULL, THREAD_POOL_SIZE, FALSE, NULL);
    crs_construction_input input[SEGMENTS];
    for(int j=0;j<SEGMENTS;j++) {
        input[j].segment = &crs_segments[j];
        input[j].import_segment = idata.import_segment[j];
    }
    for(int j=0;j<SEGMENTS;j++) {
        g_thread_pool_push(pool, &input[j], NULL);
    }
    g_thread_pool_free(pool,FALSE,TRUE);

    for(int j=0;j<SEGMENTS;j++) {
        log_debug("--------------- SEGMENT %d-------------\n",j);
        crs_segment *this_segment = crs_segments[j];
        log_info("len(values) = %d",this_segment->fold_stmt_arr->len);
        log_info("len(colind) = %d",this_segment->colind->len);
        log_info("len(rowlist) = %d",this_segment->rowlist->len);
        log_info("len(rowptr) = %d",this_segment->rowptr->len);
        int total = 0;
        for (int t=0;t<this_segment->rowlist->len;t++) {
            fs_rid p = g_array_index(this_segment->rowlist,fs_rid,t);
            int assigned_hash = p % HASHES_NUM;
            char tmp_rid[16+1];
            size_t vsize;
            sprintf(tmp_rid,"%llx",p);
            char *vbuf = kcdbget(db_hashes[assigned_hash],tmp_rid, 17*sizeof(char), &vsize);
            printf("predicate %llx %s\n",p,vbuf);
            kcfree(vbuf);
        }
        for (int t=0;t<this_segment->rowptr->len;t++) {
            printf("%d ",*((guint *)g_ptr_array_index(this_segment->rowptr,t)));
            total += *((guint *)g_ptr_array_index(this_segment->rowptr,t));
        }
        printf("\n total %d \n",total);
        /*for (int t=0;t<this_segment->fold_stmt_arr->len;t++) {
            guint len = ((fold_stmt *)g_ptr_array_index(this_segment->fold_stmt_arr,t))->rids_2d->len;
            if (len > 1) 
            printf(" [%d %d] ",t,len);
        }*/
        //printf("\n");
     }


/*
 KCCUR* cur;
 char *kbuf, *vbuf;
 size_t ksiz, vsiz;
 const char *cvbuf;
 cur = kcdbcursor(db_hashes[0]);
 kccurjump(cur);
 while ((kbuf = kccurget(cur, &ksiz, &cvbuf, &vsiz, 1)) != NULL) {
    printf("%s:%s\n", kbuf, cvbuf);
    kcfree(kbuf);
 }
 kccurdel(cur);
*/
    for(int j=0;j<HASHES_NUM;j++) {
        /* close the database */
        if (!kcdbclose(db_hashes[j])) {
            fprintf(stderr, "close error: %s\n", kcecodename(kcdbecode(db_hashes[j])));
        }
        /* delete the database object */
        kcdbdel(db_hashes[j]);
    }

    g_timer_stop (global_time);
    g_timer_stop (partial_time);

    counter = idata.counter;
    log_debug("done indexing %.2lf kT rate %.2lf kT/s!",counter/1e3, (counter/g_timer_elapsed(global_time,NULL))/1e3);
    log_debug ("time = %.2lf secs",g_timer_elapsed(global_time,NULL));
    int i;
    for (i=0;i<SEGMENTS;i++)
        printf(" %d ",segments[i]);
    printf("\n");
    //printf("sleeping ... forever ....\n");
    //sleep(100000);
    exit(1);
}
