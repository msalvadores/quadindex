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

int do_query(const char *kb_name,char *out) {
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
#if 1
{ printf("to close. press enter\n");
char foo;
read(0, &foo, 1); }
#endif

    rdf_kb_close(kb,&error);

#if 1
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
        fprintf(stderr,"Use rdf-query kb_name query\n");
        exit(-1);
    }
    char *kb_name = argv[1];
    char *out_file = argv[2];
    do_query(kb_name,out_file);
    exit(0);
}

