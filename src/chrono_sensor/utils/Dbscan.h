#ifndef __DBSCAN_H__
#define __DBSCAN_H__

#include <vector>
#include <queue>
#include <set>
#include <memory>
#include <functional>
#include "Kdtree.h"

/**
 * If big epsilon value and dense points, the kdtree becomes a bottleneck of performance (Han 5/18/2021)
 * 
*/

typedef unsigned int uint;

class DBSCAN final {
    enum ERROR_TYPE {
        SUCCESS = 0
        , FAILED
        , COUNT
    };
public:
    DBSCAN(){}
    ~DBSCAN(){}
    int Run(std::vector<vec3f>* V, const float eps, const uint min);
    std::vector<std::vector<uint>> getClusters(){return this->clusters;};


private:
    std::vector<uint> regionQuery(const uint pid) const;
    void              addToCluster(const uint pid, const uint cid);
    void              expandCluster(const uint cid, const std::vector<uint>& neighbors);
    void              addToBorderSet(const uint pid) { this->borderset.insert(pid); }
    void              addToBorderSet(const std::vector<uint>& pids) { for (uint pid : pids) this->borderset.insert(pid); }
    bool              isInBorderSet(const uint pid) const { return this->borderset.end() != this->borderset.find(pid); }
    void              buildKdtree(const std::vector<vec3f>* V);
    void              destroyKdtree();

private:
    std::vector<bool>   visited;
    std::vector<bool>   assigned;
    std::set<uint>      borderset;
    uint                datalen;
    uint                minpts;
    float               epsilon;
    kdtree*             tree;
    std::vector<vec3f>*     data;
    std::vector<std::vector<uint>>  clusters;
    std::vector<uint>               noise;


};

//int DBSCAN::Run(
//    std::vector<vec3f>*                V
//    , const float           eps
//    , const uint            min
//) {
//
//    // Validate
//    if (V->size() < 1) return ERROR_TYPE::FAILED;
//    if (min < 1) return ERROR_TYPE::FAILED;
//
//    // initialization
//    this->datalen = (uint)V->size();
//    this->visited = std::vector<bool>(this->datalen, false);
//    this->assigned = std::vector<bool>(this->datalen, false);
//    this->clusters.clear();
//    this->noise.clear();
//    this->minpts = min;
//    this->data = V;
//    this->epsilon = eps;
//
//    this->buildKdtree(this->data);
//
//    for (uint pid = 0; pid < this->datalen; ++pid) {
//        this->borderset.clear();
//        if (!this->visited[pid]) {
//            this->visited[pid] = true;
//
//            const std::vector<uint> neightbors = this->regionQuery(pid);
//            if (neightbors.size() < this->minpts) {
//                continue;
//            }
//            else {
//                uint cid = (uint)this->clusters.size();
//                this->clusters.push_back(std::vector<uint>());
//                this->addToBorderSet(pid);
//                this->addToCluster(pid, cid);
//                this->expandCluster(cid, neightbors);
//            }
//        }
//    }
//
//    for (uint pid = 0; pid < this->datalen; ++pid) {
//        if (!this->assigned[pid]) {
//            this->noise.push_back(pid);
//        }
//    }
//
//    this->destroyKdtree();
//    
//    return ERROR_TYPE::SUCCESS;
//
//}
//
//void DBSCAN::destroyKdtree() {
//    kd_free(this->tree);
//}
//
//void DBSCAN::buildKdtree(const std::vector<vec3f>* V)
//{
//    this->tree = kd_create();
//    std::shared_ptr<float[]> v(new float[3]);
//    for (uint r = 0; r < this->datalen; ++r) {
//        for (uint c = 0; c < 3; ++c) {
//            v[c] = (float)(*V)[r][c];
//        }
//        kd_insert(this->tree, v.get(), (void*)&(*V)[r]);
//    }
//}
//
//std::vector<uint> DBSCAN::regionQuery(const uint pid) const {
//
//    std::vector<uint> neighbors;
//
//    std::shared_ptr<float[]> v(new float[3]);
//    for (uint c = 0; c < 3; ++c) {
//        v[c] = (float)((*this->data)[pid][c]);
//    }
//
//    kdres* presults = kd_nearest_range(this->tree, v.get(), this->epsilon);
//    while (!kd_res_end(presults)) {
//        vec3f* pch = (vec3f*)kd_res_item(presults, v.get());
//        uint pnpid = (uint)(pch - &(*this->data)[0]);
//        if(pid != pnpid) neighbors.push_back(pnpid);
//        kd_res_next(presults);
//    }
//    kd_res_free(presults);
//
//    
//    return neighbors;
//}
//
//void DBSCAN::expandCluster(const uint cid, const std::vector<uint>& neighbors) {
//
//    std::queue<uint> border; 
//    for (uint pid : neighbors) border.push(pid); 
//    this->addToBorderSet(neighbors);
//    
//    while(border.size() > 0) { 
//        const uint pid = border.front(); border.pop();
//
//        if (!this->visited[pid]) {
//
//            this->visited[pid] = true;
//            const std::vector<uint> pidneighbors = this->regionQuery(pid);
//
//            if (pidneighbors.size() >= this->minpts) {
//                this->addToCluster(pid, cid);
//                for (uint pidnid : pidneighbors) { 
//                    if (!this->isInBorderSet(pidnid)) { 
//                        border.push(pidnid); 
//                        this->addToBorderSet(pidnid);
//                    }
//                }
//            }
//        }
//    }
//}
//
//void DBSCAN::addToCluster(const uint pid, const uint cid) {
//    this->clusters[cid].push_back(pid);
//    this->assigned[pid] = true;
//}
//
#endif //__DBSCAN_H__