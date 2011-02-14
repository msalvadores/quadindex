#include <stdio.h>
#include <stdarg.h>

#include <glib.h>
#include "error.h"


void rdf_error_log(int level,const char *func, int line, const char *s, ...) {
    va_list argp;
    va_start(argp, s);
    char *msg = g_strdup_vprintf(s, argp);

    g_log(NULL,level,"%s [%i]: %s",func,line,msg);
    
    g_free(msg);
}
