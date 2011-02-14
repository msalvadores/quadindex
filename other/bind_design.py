import new
import pdb


def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    obj = type('Enum', (), enums)
    obj.__len__ = new.instancemethod(lambda s: len(sequential), obj, obj.__class__)
    return obj() 

def filter_quads(quads_f,grp=-1,sbj=-1,prd=-1,obj=-1):
    return filter(lambda q: (grp is -1 or grp==q[0]) and (sbj is -1 or sbj==q[1]) and 
    (prd is -1 or prd==q[2]) and (obj is -1 or obj==q[3]),quads_f)

def build_matrix(quads,uris_enum):
    #we do P,O and S,G
    matrix = list()
    uris = range(len(uris_enum))
    print uris
    for p in uris
        quads_p=filter_quads(quads,prd=p)
        _po = list()
        for o in uris:
            quads_o=filter_quads(quads_p,obj=o)
            _sg = list()
            for s in uris:
                quads_s=filter_quads(quads_o,sbj=s)
                _g=list()
                for g in uris:
                    quads_g=filter_quads(quads_s,grp=g)
                    _g.append(1 if quads_g else 0) 
                _sg.append(_g)
            _po.append(_sg) 
        matrix.append(_po)
    return matrix

if __name__ == "__main__":
    S = enum('ms8','gc3','icm','hg','knows','born','spain','uk','italy','graph0','graph1')
    quads = [
        (S.graph0,S.ms8,S.knows,S.gc3),
        (S.graph0,S.ms8,S.born,S.spain),
        (S.graph0,S.gc3,S.knows,S.icm),
        (S.graph0,S.gc3,S.born,S.italy),
        (S.graph1,S.icm,S.born,S.uk),
        (S.graph1,S.hg,S.born,S.uk)
    ]
    q_matrix = build_matrix(quads,S)
    
    query = [
        
        
    ]

    #bind(quad_matrix,bind_matrix)

