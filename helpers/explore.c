#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <kclangc.h>


typedef unsigned long long int rid;

//cc -Wall -g -std=gnu99 -O2 `pkg-config --cflags glib-2.0 gthread-2.0 kyotocabinet` -multiply_defined suppress  `pkg-config --libs glib-2.0 kyotocabinet`  explore.c -o explore
//cc -Wall -g -std=gnu99 -O2 `pkg-config --cflags glib-2.0 gthread-2.0 kyotocabinet` -rdynamic  `pkg-config --libs glib-2.0 kyotocabinet`  explore.c -o explore

void print_guint_arr(guint *a,guint l) {
    for(guint i=0;i<l;i++)
        printf("\t[%u] %u\n",i,a[i]);
}

char *rdf_kb_rid_unhash(KCDB *hash_st, rid rid) {
     char tmp_rid[16+1];
     char *vbuf;
     size_t vsiz;
     sprintf(tmp_rid,"%llx",rid);
     vbuf = kcdbget(hash_st, tmp_rid , 16 * sizeof(char), &vsiz);
     //log_debug("%llx %s", rid, vbuf);
     return vbuf;
}

void print_rid_arr(rid *a,guint l,KCDB *hash_st) {
    for(guint i=0;i<l;i++) {
        gchar *val=rdf_kb_rid_unhash(hash_st,a[i]);
        printf("\t[%u] [%llx] %s\n",i,a[i],val);
        free(val);
    }
}

static KCDB* rdf_kb_open_hash_storage(gchar *hashfile) {
    KCDB *hash_st = kcdbnew();
    if (!kcdbopen(hash_st, hashfile , KCOWRITER | KCOCREATE)) {
        printf("ERROR OPENINING HASH %s\n",hashfile);
        exit(-1);
     }
     return hash_st;
}



size_t print_crs(unsigned char *data,KCDB *hash_st) {
 
        guint *counters = (guint *) data;
        if (!counters[3])
            return 0;
        printf("==============================================================\n");
        printf("offset COLIND %u\n",counters[0]);
        printf("offset ROWLIST %u\n",counters[1]);
        printf("offset ROWPTR %u\n",counters[2]);
        printf("colind size %u\n",counters[3]);
        printf("rowlist size %u\n",counters[4]);
        printf("rowptr size %u\n",counters[5]);
        
     printf("colind values\n");
        print_rid_arr((rid *)(data + counters[0]),counters[3],hash_st);
     printf("rowlist values\n");
        print_rid_arr((rid *)(data + counters[1]),counters[4],hash_st);
     printf("rowptr values\n");
        print_guint_arr((guint *)(data + counters[2]),counters[5]);

        printf("==============================================================\n");
        return counters[2] + (counters[5] * sizeof(guint)) ;
}
int main(int argc,char** args) {
  
    int fd;
    unsigned char *data;
    struct stat sb;

    printf("opening %s\n",args[1]);
    
    KCDB* hash = rdf_kb_open_hash_storage(args[2]);

    fd = open(args[1], O_RDONLY);
    fstat(fd,&sb);

    printf("file size %llu\n",sb.st_size);
    data = mmap(0, sb.st_size, PROT_READ , MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("close");
        close(fd);
    }
    printf("data %p\n",data); 
    if (g_str_has_suffix(args[1],"index")) {
        printf("index file\n");
        print_crs(data,hash);
    } else if(g_str_has_suffix(args[1],"bin")) {
        printf("bin file\n");
        unsigned char *cursor = data;
        size_t x = 0;
        while((cursor - data) < sb.st_size) {
            x = print_crs(cursor,hash);
            if (!x)
                 cursor = sb.st_size + data + 1;
            else 
                cursor += x;
        }
    }
    munmap(data, sb.st_size);
    close(fd);
    exit(1);
}
