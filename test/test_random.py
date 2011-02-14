import sys, random, os, uuid
import time

log = None
def run_command(cmd):
    global log
    log.write(cmd+'\n')
    log.flush()
    t0=time.time()
    os.system(cmd)
    return time.time() - t0

def run_random_test(code_test,nf,ng,ns,np,no):
    global log
    data_files=list()
    folder = './tmp/test%s/'%code_test
    os.makedirs(folder)
    data_folder = os.path.join(folder,'data')

    log = file(os.path.join(folder,"commands.log"),'w')
    total_triples = 0
    for f in range(nf):
        fname = os.path.join(folder,'n_%s.nq'%f) 
        data = file(fname,'w')
        ntriples = random.randint(2e1,2e1)
        total_triples += ntriples
        print "generating %s triples ... "%ntriples
        for x in range(ntriples):
            graph = "<http://www.ecs.soton.ac.uk/graph%s>"%random.randint(0,ng)
            subject = "<http://www.ecs.soton.ac.uk/subject%s>"%random.randint(0,ns)
            predicate = "<http://www.ecs.soton.ac.uk/predicate%s>"%random.randint(0,np)
            obj = "<http://www.ecs.soton.ac.uk/object%s>"%random.randint(0,no)
            data.write("%s %s %s %s .\n"%(subject,predicate,obj,graph))
        data.close()
        t=run_command("./storage/rdf-import -f nquads %s %s"%(data_folder,fname))
        print "#### import %s triples in %0.3f (%0.3f Kt/s)"%(ntriples,t,(ntriples/1000)/t)
        #run_command("find %s -name *.nq -exec cat {} \; >> %s/orig.quads"%(folder,folder))
        #run_command("cat %s/orig.quads | sort | uniq > %s/sort.quads"%(folder,folder))
        #t=run_command("./storage/rdf-dump -d %s -f nquads -o %s/dump.quads; cat %s/dump.quads | sort | uniq > %s/dump.sort.quads"%(data_folder,folder,folder,folder))
        #print "#### dump %s triples in %0.3f (%0.3f Kt/s)"%(total_triples,t,(total_triples/1000)/t)
        #run_command("cmp %s/sort.quads %s/dump.sort.quads > %s/errors 2>/dev/null"%(folder,folder,folder))
        #if os.path.getsize("%s/errors"%folder):
        #    print "ERROR CMP RETURN STUFF"
        #    for x in file("%s/errors"%folder).readlines():
        #        print x
        #    return
        #else:
        #    print "#########################"
        #    print "TEST %s OK"%code_test
        #    print "#########################"
    log.close()

if __name__ == "__main__":
    num_files = int(sys.argv[1])
    num_graphs = int(sys.argv[2])
    num_subjects = int(sys.argv[3])
    num_predicates = int(sys.argv[4])
    num_objects = int(sys.argv[5])
    #code = uuid.uuid1().hex[0:8]
    code = "000"
    run_random_test(code,num_files,num_graphs,num_subjects,num_predicates,num_objects)
