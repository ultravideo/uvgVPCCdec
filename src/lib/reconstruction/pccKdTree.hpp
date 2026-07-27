#include "bitstreamParsing/gof.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"
#include "KDTreeVectorOfVectorsAdaptor.h"

using namespace uvgvpcc_dec;

// PCC nearest neighbor result
class pccNNResult {
    public:
        pccNNResult() = default;
        ~pccNNResult() {
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
    std::vector<size_t> indices_;
    std::vector<double> dist_;
};

class pccKdTree {
    public:
        pccKdTree();
        pccKdTree(const std::vector<Vector3<typeGeometryInput>>& geo_points);
        ~pccKdTree();
        void init(const std::vector<Vector3<typeGeometryInput>>& geo_points);
        void search(const Vector3<typeGeometryInput>& point, const size_t num_results, pccNNResult& nnResults);

    private:
        void clear();
        void* kdTree_;
};