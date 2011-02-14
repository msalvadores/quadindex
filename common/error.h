
#ifndef _RDF_KB_ERROR_H  /* duplication check */
#define _RDF_KB_ERROR_H

#include <glib.h>


#define log_debug(f...) (rdf_error_log(G_LOG_LEVEL_DEBUG, __func__, __LINE__,f))
#define log_warn(f...) (rdf_error_log(G_LOG_LEVEL_WARNING, __func__, __LINE__,f))
#define log_info(f...) (rdf_error_log(G_LOG_LEVEL_INFO, __func__, __LINE__, f))
#define log(l,f...) (rdf_error_log(l, __func__, __LINE__, f))


void rdf_error_log(int level,const char *func, int line, const char *s, ...)
__attribute__ ((format(printf, 4, 5)));


#endif
