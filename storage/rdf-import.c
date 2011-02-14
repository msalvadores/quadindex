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

int do_import(const char *kb_name,char **uri,char **model,char *format,int files) {

    GError *error=NULL;
    rdf_kb *kb = rdf_kb_open(kb_name,TRUE,&error);
    if (!kb) {
        if (error)
           log_warn("%s",error->message);
        return -1;
    }
    log_debug("asserting data into [%s] ...",kb_name);
    for (int i=0;i<files;i++) {
        if (error) {
            g_error_free (error);
            error = NULL;
        }
        rdf_kb_import_data_from_file(kb, uri[i], model[i], format, &error);
        if (error) 
            log_warn("%s",error->message); 
    }
    if (error) {
        g_error_free (error);
        error = NULL;
    }

    rdf_kb_close(kb,&error);
    if (error) { 
        log_warn("%s",error->message); 
        g_error_free (error);
    }
    return 0;
}

int main(int argc, char *argv[]) {

    char *format = "auto";
    char *optstring = "m:M:f:";
    int c, opt_index = 0;
    int files = 0; 
    char *kb_name = NULL;
    char *model[argc], *uri[argc];
    char *model_default = NULL;
    int help=0;

    static struct option long_options[] = {
        { "model", 1, 0, 'm' },
        { "model-default", 1, 0, 'M' },
        { "format", 1, 0, 'f' },
    };  
    
    for (int i= 0; i < argc; ++i) {
      model[i] = NULL; 
    } 
    
    while ((c = getopt_long (argc, argv, optstring, long_options, &opt_index)) != -1) {
        if (c == 'm') {
            model[files++] = optarg;
        } else if (c == 'M') {
            model_default = optarg;
        } else if (c == 'f') {
            format = optarg;
        } else {
            help = 1;
            fprintf(stderr, "Unknown parameter '%c' \n", c);
            fprintf(stderr, "Usage: %s <kbname> <rdf file/URI> ...\n", argv[0]);
            fprintf(stderr, " -m --model     specify a model URI for the next RDF file\n");
            fprintf(stderr, " -M --model-default specify a model URI for all RDF files\n");
            fprintf(stderr, " -f --format    specify an RDF syntax for the import\n");
            exit(1);        
        }
    }
    files = 0; 
    for (int k = optind; k < argc; ++k) {
        if (!kb_name) {
            kb_name = argv[k];
        } else {
            uri[files] = g_strdup(argv[k]);
            if (!model[files]) {
                if (!model_default) {
                    model[files] = uri[files];
                } else {
                    model[files] = model_default;
                }
            }
            files++;
        }
    }

    if (help || !kb_name || files == 0) {
        help = 1;
        fprintf(stderr, "Usage: %s <kbname> <rdf file/URI> ...\n", argv[0]);
        fprintf(stderr, " -m --model     specify a model URI for the next RDF file\n");
        fprintf(stderr, " -M --model-default specify a model URI for all RDF files\n");
        fprintf(stderr, " -f --format    specify an RDF syntax for the import\n");
        exit(1);   
    }
   
    if (!g_thread_get_initialized())
        g_thread_init(NULL);
	if (!fs_hash_uri)
		fs_hash_init();
   
    //g_mem_set_vtable(glib_mem_profiler_table);
    //g_atexit(g_mem_profile); 
    int res = do_import(kb_name,uri,model,format,files);
#if 0
printf("press enter\n");
char foo;
#endif
    exit(res);
}

