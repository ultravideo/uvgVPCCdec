#include "pccKdTree.hpp"

typedef KDTreeVectorOfVectorsAdaptor<std::vector<Vector3<typeGeometryInput>>, typeGeometryInput, float, 3, metric_L2_Simple_2, size_t> KdTreeAdaptor;

pccKdTree::pccKdTree() : kdTree_(nullptr) {}

pccKdTree::pccKdTree(const std::vector<Vector3<typeGeometryInput>>& geo_points) : kdTree_(nullptr) {
    init(geo_points);
}

pccKdTree::~pccKdTree() {
    clear();
}
void pccKdTree::clear() {
    if (kdTree_ != nullptr) {
        delete(static_cast<KdTreeAdaptor*>(kdTree_));
        kdTree_ = nullptr;
    }
}

void pccKdTree::init(const std::vector<Vector3<typeGeometryInput>>& geo_points) {
    clear();
    kdTree_ = new KdTreeAdaptor(3, geo_points, 10);
}

void pccKdTree::search(const Vector3<typeGeometryInput>& point, const size_t num_results, pccNNResult& nnResults) {
    if (num_results != nnResults.size()) {
        nnResults.resize(num_results);
    }
    size_t retSize = (static_cast<KdTreeAdaptor*>(kdTree_))->index->knnSearch(
        &point[0], num_results, nnResults.indices(), nnResults.dist());
}