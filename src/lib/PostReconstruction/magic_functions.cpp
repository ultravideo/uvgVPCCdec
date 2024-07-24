#include "magic_functions.hpp"
#include "nanoflann/nanoflann.hpp"

#include "nanoflann/KDTreeVectorOfVectorsAdaptor.h"

typedef KDTreeVectorOfVectorsAdaptor<point_cloud_frame, uint16_t, float, 3, metric_L2_Simple_2, size_t> KdTreeAdaptor;

PCCKdTree::PCCKdTree() : kdtree_( nullptr ) {}

PCCKdTree::PCCKdTree( const point_cloud_frame& pointCloud ) : kdtree_( nullptr ) { init( pointCloud ); }

PCCKdTree::~PCCKdTree() { clear(); }
void PCCKdTree::clear() {
  if ( kdtree_ != nullptr ) {
    delete ( static_cast<KdTreeAdaptor*>( kdtree_ ) );
    kdtree_ = nullptr;
  }
}

void PCCKdTree::init( const point_cloud_frame& pointCloud ) {
  clear();
  kdtree_ = new KdTreeAdaptor( 3, pointCloud, 10 );
}

void PCCKdTree::search( const point3d& point, const size_t num_results, PCCNNResult& results ) const {
  if ( num_results != results.size() ) { results.resize( num_results ); }
  auto retSize = ( static_cast<KdTreeAdaptor*>( kdtree_ ) )
                     ->index->knnSearch( &point.data_[0], num_results, results.indices(), results.dist() );
  assert( retSize == results.size() );
}

void PCCKdTree::searchRadius( const point3d& point,
                              const size_t      num_results,
                              const double      radius,
                              PCCNNResult&      results ) const {
  std::vector<std::pair<size_t, double> > ret;
  nanoflann::SearchParams                 params;
  size_t retSize = ( static_cast<KdTreeAdaptor*>( kdtree_ ) )->index->radiusSearch( &point.data_[0], radius, ret, params );
  if ( retSize > num_results ) { retSize = num_results; }
  ret.resize( retSize );
  results.reserve( retSize );
  for ( const auto& result : ret ) { results.pushBack( result ); }
}
