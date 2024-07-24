#include "uvgvpccdec/uvgvpccdec.hpp"
#include "uvgvpccdec/data_structures.hpp"

#include "nanoflann/KDTreeVectorOfVectorsAdaptor.h"

struct PCCNNQuery3 {
    point3d point;
    double     radius;
    size_t     nearestNeighborCount;
};

class PCCNNResult {
    public:
    PCCNNResult() = default;
    ~PCCNNResult() {
        indices_.clear();
        dist_.clear();
    }
    inline void resize( const size_t size ) {
        indices_.resize( size );
        dist_.resize( size );
    }
    inline void reserve( const size_t size ) {
        indices_.reserve( size );
        dist_.reserve( size );
    }
    inline size_t size() const {
        assert( indices_.size() == dist_.size() );
        return indices_.size();
    }
    inline size_t  count() const { return size(); }
    inline size_t& indices( size_t index ) { return indices_[index]; }
    inline double& dist( size_t index ) { return dist_[index]; }
    inline size_t* indices() { return indices_.data(); }
    inline double* dist() { return dist_.data(); }
    inline void    pushBack( const std::pair<size_t, double>& value ) {
        indices_.push_back( value.first );
        dist_.push_back( value.second );
    }
    inline void popBack() {
        indices_.pop_back();
        dist_.pop_back();
    }

    private:
        std::vector<size_t> indices_;
        std::vector<double> dist_;
};

class PCCKdTree {
    public:
    PCCKdTree();
    PCCKdTree( const point_cloud_frame& pointCloud );
    ~PCCKdTree();
    void init( const point_cloud_frame& pointCloud );
    void search( const point3d& point, const size_t num_results, PCCNNResult& results ) const;
    void searchRadius( const point3d& point,
                        const size_t      num_results,
                        const double      radius,
                        PCCNNResult&      results ) const;

    private:
    void  clear();
    void* kdtree_;
};