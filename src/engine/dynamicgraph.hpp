
#ifndef DEF_DYNAMIC_GRAPH
#define DEF_DYNAMIC_GRAPH

#include <algorithm>

#include "walks/randomwalk.hpp"
#include "engine/staticgraph.hpp"

struct EdgeLog{
    vid_t source;
    vid_t destination;
    bool isDelete;

    EdgeLog(vid_t s = 0, vid_t d = 0, bool isDel = 0)
    : source(s), destination(d), isDelete(isDel) {
    }

    bool operator<(const EdgeLog e) const
	{
		return this->source <= e.source;
	}
};

class DynamicGraph : public StaticGraph {
public:     
    EdgeLog **logs;
    eid_t *nlogs;
        
public:
        
    DynamicGraph(std::string _base_filename, uint16_t _blocksize, bid_t _nblocks, bid_t _nmblocks) 
            : StaticGraph(_base_filename, _blocksize, _nblocks, _nmblocks){
        logs = new EdgeLog*[nblocks];
        nlogs = new eid_t[nblocks];
        for(bid_t p = 0; p < nblocks; p++){
            logs[p] = new EdgeLog[LOG_BUFFER_SIZE];
            nlogs[p] = 0;
        }
    }
        
    virtual ~DynamicGraph() {
        for(bid_t p = 0; p < nblocks; p++){
            delete [] logs[p];
        }
        delete [] logs;
        delete [] nlogs;
    }

    void addEdge(vid_t s = 0, vid_t d = 0, bool isDel = 0){
        bid_t p = getblock( s );
        if(nlogs[p] >= LOG_BUFFER_SIZE){
            // compaction();
            nlogs[p] = 0;
        }
        EdgeLog *e = new EdgeLog(s, d, isDel);
        logs[p][nlogs[p]++] = *e;
    }

    std::vector<vid_t> getNeighbors(vid_t v){
        bid_t p = getblock(v);

        std::vector<vid_t> neighbors = StaticGraph::getNeighbors(v);
        
        for(eid_t i = 0; i < nlogs[p]; i++){
            if(logs[p][i].source == v){
                if(logs[p][i].isDelete){
                    for(auto it = neighbors.begin(); it != neighbors.end(); it++)
                        if(*it == logs[p][i].destination)
                            neighbors.erase(it);
                }else{
                    neighbors.push_back(logs[p][i].destination);
                }
            }
        }

        return neighbors;
    }

    void loadSubGraph(bid_t p, eid_t * &beg_pos, vid_t * &csr, vid_t *nverts, eid_t *nedges){
        
        //1. Load the CSR from disk
        StaticGraph::loadSubGraph(p, beg_pos, csr, nverts, nedges);

        if(nlogs[p] == 0) return;
        return;

        //2. Sort the edge logs by source
        std::sort(logs[p], logs[p] + nlogs[p]);

        //3. Allocate new space
        *nedges += nlogs[p];
        char * newcsr = (char*) malloc((*nedges)*sizeof(vid_t));
        char * newcsrptr = newcsr;
        char * newbeg_pos = (char*) malloc(*nverts*sizeof(eid_t));
        char * newbeg_posptr = newbeg_pos;
        vid_t * csrptr = csr;
        
        // write 0 in the beginning of newbeg_pos
        eid_t curpos = 0;
        *((eid_t*)newbeg_posptr) = curpos;
        newbeg_posptr += sizeof(eid_t);

        //4. Successively traverse each vertex and merge edge logs
        eid_t e = 0; // index of edge logs 
        for(vid_t v = 0; v < *nverts; v++){
            eid_t edge_count = beg_pos[v+1] - beg_pos[v];
            memcpy(newcsrptr, csrptr, edge_count*sizeof(vid_t));
            newcsrptr += edge_count*sizeof(vid_t);
            csrptr += edge_count;
            while (e < nlogs[p] && logs[p][e].source == blocks[p] + v){
                *((vid_t*)newcsrptr) = logs[p][e].destination;
                newcsrptr += sizeof(vid_t);
                edge_count++;
            }
            curpos += edge_count;
            *((eid_t*)newbeg_posptr) = curpos;
            newbeg_posptr += sizeof(eid_t);
            e++;
        }

        //5. Rewrite the CSR to disk
        flushBlock(base_filename, newcsr, newcsrptr, newbeg_pos, newbeg_posptr);

        //6. Free the old CSR from memory
        free(csr);
        free(beg_pos);
        nlogs[p] = 0;

        //7. point to new csr
        csr = (vid_t*)newcsr;
        beg_pos = (eid_t*)newbeg_pos;
    }

    void compaction(bid_t p){
        vid_t nverts = blocks[p+1] - blocks[p];
        eid_t nedges, *beg_pos = (eid_t*)malloc((nverts+1)*sizeof(eid_t));
        vid_t *csr = (vid_t*)malloc((size_t)blocksize*1024*1024);

        loadSubGraph(p, beg_pos, csr, &nverts, &nedges);

        free(csr);
        free(beg_pos);
    }

};

#endif