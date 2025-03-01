/*
    4store - a clustered RDF storage and query engine

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
 *  Copyright (C) 2006 Steve Harris for Garlik
 */

#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "umac.h"
#include "hash.h"

#include "rdf-constants.h"

/* hash function headers */
fs_rid fs_hash_uri_umac(const char *str);
fs_rid fs_hash_literal_umac(const char *str, fs_rid attr);

fs_rid (*fs_hash_uri)(const char *str) = NULL;
fs_rid (*fs_hash_literal)(const char *str, fs_rid attr) = NULL;


struct fs_globals fs_c;

void fs_hash_init()
{
    fs_hash_uri = fs_hash_uri_umac;
    fs_hash_literal = fs_hash_literal_umac;

    fs_c.empty = 0LL;
    fs_c.xsd_string = fs_hash_uri(XSD_STRING);
    fs_c.xsd_integer = fs_hash_uri(XSD_INTEGER);
    fs_c.xsd_float = fs_hash_uri(XSD_FLOAT);
    fs_c.xsd_double = fs_hash_uri(XSD_DOUBLE);
    fs_c.xsd_decimal = fs_hash_uri(XSD_DECIMAL);
    fs_c.xsd_boolean = fs_hash_uri(XSD_BOOLEAN);
    fs_c.xsd_datetime = fs_hash_uri(XSD_DATETIME);
    fs_c.xsd_pinteger = fs_hash_uri(XSD_NAMESPACE "positiveInteger");
    fs_c.xsd_ninteger = fs_hash_uri(XSD_NAMESPACE "negativeInteger");
    fs_c.xsd_npinteger = fs_hash_uri(XSD_NAMESPACE "nonPositiveInteger");
    fs_c.xsd_nninteger = fs_hash_uri(XSD_NAMESPACE "nonNegativeInteger");
    fs_c.xsd_long = fs_hash_uri(XSD_NAMESPACE "long");
    fs_c.xsd_int = fs_hash_uri(XSD_NAMESPACE "int");
    fs_c.xsd_short = fs_hash_uri(XSD_NAMESPACE "short");
    fs_c.xsd_byte = fs_hash_uri(XSD_NAMESPACE "byte");
    fs_c.xsd_ulong = fs_hash_uri(XSD_NAMESPACE "unsignedLong");
    fs_c.xsd_uint = fs_hash_uri(XSD_NAMESPACE "unsignedInt");
    fs_c.xsd_ushort = fs_hash_uri(XSD_NAMESPACE "unsignedShort");
    fs_c.xsd_ubyte = fs_hash_uri(XSD_NAMESPACE "unsignedByte");
    fs_c.lang_en = fs_hash_literal("en", 0);
    fs_c.lang_fr = fs_hash_literal("fr", 0);
    fs_c.lang_de = fs_hash_literal("de", 0);
    fs_c.lang_es = fs_hash_literal("es", 0);
    fs_c.rdf_type = fs_hash_uri(RDF_NAMESPACE "type");
    fs_c.default_graph = fs_hash_uri(FS_DEFAULT_GRAPH);
    fs_c.system_config = fs_hash_uri(FS_SYSTEM_CONFIG);
    fs_c.rdfs_label = fs_hash_uri(RDFS_LABEL);
    fs_c.fs_text_index = fs_hash_uri(FS_TEXT_INDEX);
    fs_c.fs_token = fs_hash_uri(FS_TEXT_TOKEN);
    fs_c.fs_dmetaphone = fs_hash_uri(FS_TEXT_DMETAPHONE);
    fs_c.fs_stem = fs_hash_uri(FS_TEXT_STEM);
}


fs_rid umac_wrapper(const char *str, fs_rid nonce_in)
{
    if (!str) return 0;

    long long __attribute__((aligned(16))) data;
    long long __attribute__((aligned(16))) nonce = nonce_in;

    static umac_ctx_t umac_data = NULL;
    if (!umac_data) {
	umac_data = umac_new("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");
    }
    const int slen = strlen(str);
    char *buffer = NULL;
    void *heap_buffer = NULL;
    char __attribute__((aligned(16))) stack_buffer[1024];
    if (slen < 1024) {
	buffer = stack_buffer;
    } else {
#ifdef __APPLE__
	heap_buffer = malloc(slen+1);
	buffer = heap_buffer;
#else
	int tmp = (slen + 1) & 31;
	void **ptr = &heap_buffer;
	if (posix_memalign(ptr, 16, slen + 33 - tmp)) {
            fprintf(stderr, "posix_memalign: %s\n", strerror(errno));
            /* XXX free memory here? return NULL? */
        }
	buffer = heap_buffer;
#endif
    }
    strncpy(buffer, str, slen+1);
    umac(umac_data, buffer, slen, (char *)&data, (char *)&nonce);
    if (heap_buffer) {
	free(heap_buffer);
    }

    return data;
}

fs_rid fs_hash_uri_umac(const char *str)
{
    uint64_t top;

    if (!str) {
        return 0;
    }
    if (strncmp(str, "bnode:b", 7) == 0) {
	fs_rid bnode_id = strtoll(str+7, NULL, 16);
	if (!bnode_id) {
	    return 0;
	}

	return FS_NUM_BNODE(bnode_id);
    } else if (!isalpha(str[0])) {
        return FS_RID_GONE;
    }

    top = umac_wrapper(str, 0);
    top |= 0xC000000000000000LL;

    return top;
}



fs_rid fs_hash_literal_umac(const char *str, fs_rid attr)
{
    uint64_t top;

    if (!str) {
        return 0;
    }

    top = umac_wrapper(str, attr);
    top &= 0x7FFFFFFFFFFFFFFFLL;

    return top;
}
/* vi:set ts=8 sts=4 sw=4: */
