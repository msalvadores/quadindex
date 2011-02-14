#include <raptor.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <kclangc.h>
#include <glib.h>

#include "common/hash.h"
#include "rdfindex.h"

#define READ_BUF_SIZE 134217728
#define STAT_BATCH 50000
#define DATA_NAME "./data/data.kct"
#define HASHES_NAME "./data/hashes.kch"
#define DO_INDEX 1
#define SEGMENTS 2
#define THREAD_POOL_SIZE 1 * SEGMENTS
#define QUAD_SIZE 64
#define RID_SIZE 64

static GTimer *global_time = NULL;
static GTimer *partial_time = NULL;

static KCDB* db_hashes = NULL;
static KCDB* db_data = NULL;
static GThreadPool* pool;

int segments[SEGMENTS];
int graphs = 0;
char tmp_quad[QUAD_SIZE+1];

fs_rid global_g_rid;
unsigned char * global_g_s;

struct _index_stmt {
    fs_rid gr;
    fs_rid sr;
    fs_rid pr;
    fs_rid or;

    unsigned char *gc;
    unsigned char *sc;
    unsigned char *pc;
    unsigned char *oc;
};

typedef struct _index_stmt index_stmt;

struct _import_data {
     GArray** import_segment;
     KCDB* db_hashes;
     int counter;
};

typedef struct _import_data import_data;

raptor_uri *get_uri_from_path(raptor_world* world,char* path) {
    unsigned char *uri_string;
    raptor_uri *uri;
    uri_string = raptor_uri_filename_to_uri_string(path);
    uri=raptor_new_uri(world,uri_string);
    raptor_free_memory(uri_string);
    return uri;
}


void import_worker(gpointer worker_data, gpointer user_data) {
   char tmp_rid[16+1];
   index_stmt *stmt;
   GArray *data = (GArray *)worker_data;
   for(int j=0;j<data->len;j++) {
       stmt = (index_stmt *) g_array_index(data,gpointer,j);
       fs_rid hashes[4] = {stmt->gr,stmt->sr,stmt->pr,stmt->or};
       unsigned char *strings[4] = {stmt->gc,stmt->sc,stmt->pc,stmt->oc};
       for (int i=0;i<4;i++) {
       sprintf(tmp_rid,"%llx",hashes[i]);
       kcdbadd(db_hashes,(const char *) tmp_rid, RID_SIZE,(const char *) strings[i], strlen((const char *) strings[i]));
       }
   }
   printf("finish worker \n");
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

#ifdef DO_INDEX    
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
   
   /*fs_rid hashes[4] = {g_rid,s_rid,p_rid,o_rid};
   unsigned char *strings[4] = {gc,sc,pc,oc};
   for(int i=0;i<4;i++) {
       sprintf(tmp_rid,"%llx",hashes[i]);
       kcdbadd(db_hashes,(const char *) tmp_rid, RID_SIZE,(const char *) strings[i], strlen((const char *) strings[i]));
   }*/

   //sprintf(tmp_quad,"%llx%llx%llx%llx",g_rid,s_rid,p_rid,o_rid);
   //kcdbset(db_data, tmp_quad, QUAD_SIZE, "a", 1);

   //printf("%llx  %llx  %llx  %llx\n",g_rid,s_rid,p_rid,o_rid);
   //printf("%s  %s  %s %s\n",gc,sc,pc,oc);
   int seg_id = s_rid % SEGMENTS;
   segments[seg_id]++;
   index_stmt *istmt;
   istmt = calloc(1,sizeof(index_stmt));
   istmt->gr = g_rid;
   istmt->sr = s_rid;
   istmt->pr = p_rid;
   istmt->or = o_rid;
   istmt->gc = gc;
   istmt->sc = sc;
   istmt->pc = pc;
   istmt->oc = oc;
   g_array_append_val(idata->import_segment[seg_id],istmt);
   /* end index logic */
#endif

   if (!(idata->counter % STAT_BATCH)) {
        char message[200];
        double kt = idata->counter/1e3;
        sprintf(message,"indexing progress %.0lf kT %.2lf kT/s %.2lf kT/s",kt,kt/g_timer_elapsed(global_time,NULL), 
                (STAT_BATCH/1e3)/g_timer_elapsed(partial_time,NULL));
        puts(message);
        fflush(stdout);
        g_timer_start(partial_time);
   }
}


int main(int argc,char** args) {
    //printf("%llu\n",ULONG_LONG_MAX);
    printf("%u\n",G_MAXUINT);
    if (argc < 3) {
        printf("usage: rdfindex format file_path\n");
        exit(-1);
    }
    char *file_path = args[2];
    FILE *stream = fopen(file_path,"r");
    if (!stream) {
        printf("error: [%s] file not found\n",file_path);
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
    
    printf("[%s] parsing ... \n",file_path);
    global_time = g_timer_new();
    partial_time = g_timer_new();


    /* create the database object */
    db_data = kcdbnew();
    db_hashes = kcdbnew();

    /* open the database */
    if (!kcdbopen(db_data, DATA_NAME, KCOWRITER | KCOCREATE)) {
        fprintf(stderr, "open error: %s\n", kcecodename(kcdbecode(db_data)));
        exit(-1);
    }
    if (!kcdbopen(db_hashes, HASHES_NAME, KCOWRITER | KCOCREATE)) {
        fprintf(stderr, "open error: %s\n", kcecodename(kcdbecode(db_hashes)));
        exit(-1);
    }


    GArray* import_segment[SEGMENTS];
    for(int j=0;j<SEGMENTS;j++) {
        import_segment[j] = g_array_new(FALSE,FALSE,sizeof(index_stmt *));
    }
    import_data idata = { .counter = 0, .import_segment = import_segment };

    printf("import_segment init\n");
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

    raptor_free_world(world);

    pool = g_thread_pool_new(import_worker, NULL, THREAD_POOL_SIZE, FALSE, NULL);
    for(int j=0;j<SEGMENTS;j++) {
        g_thread_pool_push(pool, idata.import_segment[j], NULL);
    }
    g_thread_pool_free(pool,FALSE,TRUE);

    g_timer_stop (global_time);
    g_timer_stop (partial_time);


    /* close the database */
    if (!kcdbclose(db_data)) {
    fprintf(stderr, "close error: %s\n", kcecodename(kcdbecode(db_data)));
    }
    if (!kcdbclose(db_hashes)) {
    fprintf(stderr, "close error: %s\n", kcecodename(kcdbecode(db_hashes)));
    }
    /* delete the database object */
    kcdbdel(db_data);
    kcdbdel(db_hashes);

    counter = idata.counter;
    printf("done indexing %.2lf kT rate %.2lf kT/s!\n",counter/1e3, (counter/g_timer_elapsed(global_time,NULL))/1e3);
    printf ("time = %.2lf secs\n",g_timer_elapsed(global_time,NULL));
    int i;
    for (i=0;i<SEGMENTS;i++)
        printf(" %d ",segments[i]);
    printf("\n");
}
