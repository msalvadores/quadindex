/* File: quad_storage.i*/
%module quad_storage 
%{
#include "quad_storage.h"
#include <glib.h>
%}


%include "quad_storage.h"

%extend QuadIndex {
    QuadIndex(char *path,int create=0) {
        QuadIndex *i = (QuadIndex *) malloc(sizeof(QuadIndex));
        GError *error = NULL; 
        i->kb = rdf_kb_open(path,create,&error);
        if (error) {
            printf("[%s] kb not created\n",path);
             i->kb == NULL;
         }
        else
            printf("[%s] kb opened\n",path);
        return i;
    }
    void importFile(char *uri,char *model,char *format) {
        //printf("importing %s in graph %s [%s]\n",uri,model,format);
        GError *error = NULL; 
        rdf_kb_import_data_from_file($self->kb,uri,model,format,&error);
        if (error) 
            printf("error importing\n");
        else 
            printf("import ok\n");
    }

    void query(char *json,int count=0) {
        GError *error = NULL;
        rdf_kb_json_bind($self->kb,json,count,0,&error);
        if (!error)
            printf("error binding\n");
    }

    void close() {
        if ($self->kb != NULL) {
            GError *error = NULL; 
            rdf_kb_close($self->kb,&error);
            $self->kb == NULL;
            printf("kb closed\n");
         }
    }
    unsigned int size() {
        GError *error=NULL;
        unsigned int s = rdf_kb_size($self->kb,&error);
        if (error) {
            printf("error getting KB size");
            return 0;
        }
        return s;
    }
    int isOpen() {
		return $self->kb != NULL;
	}
    char *__str__() {
	    static char temp[256];
		sprintf(temp,"QuadIndex[kb=%s]", $self->kb->name);
		return &temp[0];
	}
    ~QuadIndex() {

	}
};
