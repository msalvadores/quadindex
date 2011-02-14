#include <unistd.h>
#include <raptor.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <getopt.h>
#include <string.h>

#include "storage/rdf-kb.h"
#include "common/hash.h"
#include "common/error.h"


int do_bin(const char *kb_name,char *bind_json,int just_count) {
   
   
    GError *error=NULL;
    rdf_kb *kb = rdf_kb_open(kb_name,FALSE,&error);
    if (!kb) {
        if (error)
           log_warn("%s",error->message);
        return -1;
    }
   
    log_info("counting ntriples ...");
    guint size = rdf_kb_size(kb,&error);
    log_info("triples %u",size);
    GError *error_bind=NULL;
    rdf_kb_json_bind(kb,bind_json,just_count,1,&error_bind);

#if 0
{ printf("to run query. press enter\n");
char foo;
read(0, &foo, 1); }
#endif

    rdf_kb_close(kb,&error);

#if 0
{ printf("to exit. press enter\n");
char foo;
read(0, &foo, 1); }
#endif

    if (error) { 
        log_warn("%s",error->message); 
        g_error_free (error);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc<3) {
        fprintf(stderr,"Use test_bin kb_name bin_file\n");
        exit(-1);
    }
    char *kb_name = argv[1];
    char *bind_input = argv[2];
    int just_count = 0;
    if (argc > 3) 
        just_count = strcmp(argv[3],"-c") == 0;
    do_bin(kb_name,bind_input,just_count);
    exit(0);
}

