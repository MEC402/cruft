#ifndef minmax_h__
#define minmax_h__

#include <bd/io/buffer.h>
#include <bd/io/indexfile.h>
#include <bd/volume/volume.h>

#include <tbb/parallel_reduce.h>
#include <tbb/blocked_range.h>

#include <limits>
#include <vector>
#include <functional>

namespace bd
{

/// \brief Simply compute the min/max of the given blocked_range.
/// \note Use this with TBB's parallel_reduce.
template<typename Ty>
class ParallelMinMax
{
public:

  ////////////////////////////////////////////////////////////////////////////////
  ParallelMinMax(const Buffer<Ty>* b /*, const std::function<bool(Ty)> &isRelevant*/)
    : min_value{ std::numeric_limits<Ty>::max() }
    , max_value{ std::numeric_limits<Ty>::lowest() }
    , data{ b->ptr() }
  {
  }


  ////////////////////////////////////////////////////////////////////////////////
  ParallelMinMax(ParallelMinMax& x, tbb::split)
    : min_value{ std::numeric_limits<Ty>::max() }
    , max_value{ std::numeric_limits<Ty>::lowest() }
    , data{ x.data }
  {
  }


  ////////////////////////////////////////////////////////////////////////////////
  void
  operator()(tbb::blocked_range<size_t> const &r)
  {
    Ty const * const a{ data };

    for (size_t i{ r.begin() }; i!=r.end(); ++i) {
      Ty val{ a[i] };
      
      if (val<min_value) { min_value = val; }
      if (val>max_value) { max_value = val; }
    }
  }


  ////////////////////////////////////////////////////////////////////////////////
  void
  join(const ParallelMinMax& y)
  {
    // Reduce to a global minimum and maximum for the volume.
    if (y.min_value<min_value) {
      min_value = y.min_value;
    }
    if (y.max_value>max_value) {
      max_value = y.max_value;
    }
//    empty_voxels += y.empty_voxels;
  }


//  std::function<bool(Ty)> isRelevant;
  Ty min_value;
  Ty max_value;
//  uint64_t empty_voxels;

private:
  Ty const * const data;

}; // class ParallelMinMax

} // namespace bd

#endif // ! minmax_h__
