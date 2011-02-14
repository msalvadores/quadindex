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

int do_dump(const char *kb_name,const char *format,char *out) {

    GError *error=NULL;
    rdf_kb *kb = rdf_kb_open(kb_name,FALSE,&error);
    if (!kb) {
        if (error)
           log_warn("%s",error->message);
        return -1;
    }
    
    rdf_kb_dump(kb,format,out,NULL);

#if 0
{ printf("to close. press enter\n");
char foo;
read(0, &foo, 1); }
#endif


    rdf_kb_close(kb,&error);
    if (error) { 
        log_warn("%s",error->message); 
        g_error_free (error);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    
    char* format = "ntriples";
    char* output = NULL; /* TODO: if NULL then stdout */
    char* kb_name = NULL;
    int c, opt_index = 0;

    char *optstring = "o:f:d:";
    static struct option long_options[] = {
        { "output", 1, 0, 'm' },
        { "format", 1, 0, 'f' },
        { "database", 1, 0, 'd' },
    };

    
    while ((c = getopt_long (argc, argv, optstring, long_options, &opt_index)) != -1) {
        if (c == 'd') {
            kb_name = optarg;
        } else if (c == 'f') {
            format = optarg;
            if (!strcmp(format,"ntriples") || strcmp(format,"nquads")) {
                fprintf(stderr, "error: dump only accepts 'nquads' or 'ntriples' as output format\n");
                exit(1);
            }
        } else if (c == 'o') {
            output = optarg;
        } else {
            fprintf(stderr, "Unknown parameter '%c' \n", c);
            fprintf(stderr, "Usage: %s -d database_path -f [ntriples|nquads] -o [file_to_dump_to] \n", argv[0]);
            fprintf(stderr, " -d --database\n");
            fprintf(stderr, " -o --output\n");
            fprintf(stderr, " -f --format\n");
            exit(1);        
        }
    }

    if (!kb_name || !output) {
        fprintf(stderr,"Use %s -d kb_name -o file_out -f format\n",argv[0]);
        exit(-1);
    }
    do_dump(kb_name,format,output);
    exit(0);
}

