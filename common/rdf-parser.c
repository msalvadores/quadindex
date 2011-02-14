#include <raptor.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <kclangc.h>

#include "common/hash.h"
#include "common/error.h"
#include "common/params.h"
#include "common/rdf-parser.h"
#include "storage/rdf-kb.h"
#include <time.h>

#define STAT_BATCH 100000

//static int PARSER_INIT = FALSE;

static raptor_world* parser_world;

struct _rdf_parser_internal {
    GPtrArray **quads;
    int counter;
    GTimer *global_parse_time;
    GTimer *partial_parse_time;
    int trig;
    unsigned char *model;
    fs_rid g_rid;
    rdf_kb* kb;
    gchar* bnode_ts;
};

typedef struct _rdf_parser_internal rdf_parser_internal;

void rdf_parser_init() {
    //TODO: mutex, race condition ... more than 1 thread init  ??
    parser_world = raptor_new_world(); 
}

gchar *timestamp_hash()
{
  struct tm *tmp;
  time_t t;
 
  t = time(NULL);
  tmp = localtime(&t); /* or gmtime, if you want GMT^H^H^HUTC */

  return g_strdup_printf("%02d%02d%04d%02d%02d%02d", 
     tmp->tm_mday, tmp->tm_mon+1, tmp->tm_year+1900,
     tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
}

static raptor_uri *rdf_parser_path_to_uri(raptor_world* world,char* path) {
    unsigned char *uri_string;
    raptor_uri *uri;
    uri_string = raptor_uri_filename_to_uri_string(path);
    uri=raptor_new_uri(world,uri_string);
    raptor_free_memory(uri_string);
    return uri;
}

static fs_rid *rdf_parser_new_quad(fs_rid g,fs_rid s,fs_rid p,fs_rid o) {
    fs_rid *quad=NULL;
    quad = malloc(4 * sizeof(fs_rid));
    quad[0]=g;
    quad[1]=s;
    quad[2]=p;
    quad[3]=o;
    return quad;
}

static void rdf_parser_statement_handler(void* user_data, const raptor_statement* st) {
   raptor_term *g, *s, *p, *o;
  
   g = st->graph;
   s = st->subject;
   p = st->predicate;
   o = st->object;

   rdf_parser_internal *parser_obj = (rdf_parser_internal *) user_data;


   if (parser_obj->counter == 0)
       parser_obj->partial_parse_time = g_timer_new();
    
   parser_obj->counter++;

   /* init index logic */
   unsigned char *gc = NULL;
   fs_rid g_rid;
   if (parser_obj->trig) {
       gc = raptor_uri_as_string(g->value.uri);
       g_rid = fs_hash_uri((const char *)gc);
   } else {
       g_rid = parser_obj->g_rid ; 
       gc = parser_obj->model;
   }
   

   unsigned char *sc = NULL;
   if (s->type == RAPTOR_TERM_TYPE_URI)
        sc = raptor_uri_as_string(s->value.uri);
   else {
        sc = (unsigned char *) g_strdup_printf("bnode:b%s%s",s->value.blank.string+5,parser_obj->bnode_ts);
   }
 
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
        oc =  raptor_term_to_string(o);
   } else if (o->type == RAPTOR_TERM_TYPE_BLANK) {
        oc = (unsigned char *) g_strdup_printf("bnode:b%s%s",o->value.blank.string+5,parser_obj->bnode_ts);
        o_rid = fs_hash_uri((const char *) oc); 
   }
   
   int seg_id = s_rid % SEGMENTS;
   fs_rid *quad = rdf_parser_new_quad(g_rid,s_rid,p_rid,o_rid);
   g_ptr_array_add(parser_obj->quads[seg_id],quad);

    
   /* saves hashes into disk hash TODO looks for optimistions */
   rdf_kb *kb = parser_obj->kb;
   fs_rid hashes[4] = {g_rid,s_rid,p_rid,o_rid};
   //log_debug("%llx %llx %llx %llx", hashes[0], hashes[1], hashes[2], hashes[3]);

   //char tmp_rid[16+1];
   char tmp_rid[17];
   unsigned char *strings[4] = {gc,sc,pc,oc};
   int assigned_hash=0;
   for(int i=0;i<4;i++) {
       memset(tmp_rid,0,16);
       sprintf(tmp_rid,"%llx",hashes[i]);
       assigned_hash = hashes[i] % HASHES_NUM;
       //if (i > 0 && strlen(tmp_rid) < 15 )
            //printf("ERRRRO NUL %s %s\n", tmp_rid, strings[i]);
       kcdbset(kb->hash_stores[assigned_hash],(const char *) tmp_rid, 16 ,(const char *) strings[i], strlen((const char *) strings[i]));
   }
   /* end of saving hashes into disk */


   if (!(parser_obj->counter % STAT_BATCH)) {
        double kt = parser_obj->counter/1e3;
        log_debug("parsing progress %.0lf kT %.2lf kT/s %.2lf kT/s",kt,kt/g_timer_elapsed(parser_obj->global_parse_time,NULL), 
               (STAT_BATCH/1e3)/g_timer_elapsed(parser_obj->partial_parse_time,NULL));
        g_timer_start(parser_obj->partial_parse_time);
   }
}


static raptor_parser *rdf_parser_parser_create(const char *resource_uri,const char *format) {
    raptor_parser *parser = NULL;
    if (strcmp(format, "auto")) {                                                                                                                                                               
       parser = raptor_new_parser(parser_world,format);
    } else if (strstr(resource_uri, ".n3") || strstr(resource_uri, ".ttl")) {
        parser = raptor_new_parser(parser_world,"turtle");
    } else if (strstr(resource_uri, ".nt")) {
        parser = raptor_new_parser(parser_world,"ntriples");
    } else {
        parser = raptor_new_parser(parser_world,"rdfxml");
    }
    return parser;
}

/* returns an array of fs_rid* of 4 elements (a quad)  or NULL if error */
GPtrArray** rdf_parser_parse_file_kb(rdf_kb *kb,const char *file_to_parse,const char *model,const char *format,GError **error) {

    GTimer *global_parse_time = g_timer_new();

    raptor_uri *uri = rdf_parser_path_to_uri(parser_world,(char *) file_to_parse);
    raptor_uri *base_uri = raptor_uri_copy(uri);

    raptor_parser *rdf_parser = rdf_parser_parser_create(file_to_parse, format);
    if (!rdf_parser) {
        if (error)
            g_set_error (error,RDF_PARSER,RDF_PARSER_ERROR_CREATE_PARSER, "Unable to create parser for %s format %s", file_to_parse, format);
        return NULL;
    }
    raptor_parser_set_option(rdf_parser, RAPTOR_OPTION_NO_NET, NULL, 1);

    rdf_parser_internal parser_obj;

    parser_obj.quads = malloc(SEGMENTS * sizeof(GPtrArray *));
    for (int i=0;i<SEGMENTS;i++) {
       parser_obj.quads[i] = g_ptr_array_new(); 
    }

    parser_obj.global_parse_time = global_parse_time;
    parser_obj.kb = kb;
    parser_obj.counter = 0;
    parser_obj.trig = !strcmp(format,"trig") || !strcmp(format,"nquads") ;
    parser_obj.g_rid = fs_hash_uri(model);
    parser_obj.model = (unsigned char *) model;
    parser_obj.bnode_ts = timestamp_hash();
    raptor_parser_set_statement_handler(rdf_parser, &parser_obj , (raptor_statement_handler) rdf_parser_statement_handler);
    raptor_parser_parse_start(rdf_parser, base_uri);
    
    FILE *stream = fopen(file_to_parse,"r");
    if (!stream) {
        if (error)
            g_set_error (error,RDF_PARSER,RDF_PARSER_UNABLE_TO_OPEN_FILE, "Unable to open file [%s] : %s", file_to_parse, g_strerror(errno));
        return NULL;
    }
    log_info("parsing %s ...",file_to_parse);

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
    free(buffer);
    fclose(stream);
    
    double kt = parser_obj.counter/1e3;
    log_info("file parsed %.0lf kT %.2lf kT/s",kt,kt/g_timer_elapsed(parser_obj.global_parse_time,NULL));

    g_timer_destroy(parser_obj.partial_parse_time);
    g_timer_destroy(parser_obj.global_parse_time);

    return parser_obj.quads;
}


void rdf_parser_close() {
    raptor_free_world(parser_world);
}
