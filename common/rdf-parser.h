#ifndef _RDF_KB_PARSER_H  /* duplication check */
#define _RDF_KB_PARSER_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "common/hash.h"

/* ERROR CODES */
#define RDF_PARSER 0xB0
#define RDF_PARSER_ERROR_CREATE_PARSER 0xB1
#define RDF_PARSER_UNABLE_TO_OPEN_FILE 0xB2


#ifndef INCLUDE_KB
#include "storage/rdf-kb.h"
#endif 

void rdf_parser_init();
void rdf_parser_close();

GPtrArray** rdf_parser_parse_file_kb(rdf_kb *kb,const char *uri_to_parse,const char *model,const char *format,GError **error);

#endif
