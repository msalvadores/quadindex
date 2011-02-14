import quad_storage

index = quad_storage.QuadIndex("./onekb",True) 
print index.size()

bind_param = """
[ 
    [ [], ["http://example.org/ns/s1"] , ["http://example.org/ns/p1"] , [] ]
]
"""
index.query(bind_param)

index.close()
#index.importFile("../../data/data.ttl","http://somemodel.org","turtle")
