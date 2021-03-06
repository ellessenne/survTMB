#include "snva-utils.h"
#include <array>
#include <memory>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace GaussHermite {
namespace SNVA {

template<class Type>
entropy_term_integral<Type>&
entropy_term_integral<Type>::get_cached(unsigned const n){
  using output_T = entropy_term_integral<Type>;

  constexpr std::size_t const n_cache = GaussHermiteDataCachedMaxArg();
  if(n > n_cache or n == 0l)
    throw std::invalid_argument(
        "entropy_term_integral<Type>::get_cached: invalid n (too large or zero)");

  static std::array<std::unique_ptr<output_T>, n_cache> cached_values;

  unsigned const idx = n - 1L;
  bool has_value = cached_values[idx].get();

  if(has_value)
    return *cached_values[idx];

#ifdef _OPENMP
  if(CppAD::thread_alloc::in_parallel())
    throw std::runtime_error("entropy_term_integral<Type>::get_cached called in parallel mode");
#endif

  cached_values[idx].reset(new output_T("entropy_term_integral<Type>", n));

  return *cached_values[idx];
}

template <class Type, class Fam>
integral_atomic<Type, Fam>&
integral_atomic<Type, Fam>::get_cached(unsigned const n){
  using output_T = integral_atomic<Type, Fam>;

  constexpr std::size_t const n_cache = GaussHermiteDataCachedMaxArg();
  if(n > n_cache or n == 0l)
    throw std::invalid_argument(
        "integral_atomic<Type, Fam>::get_cached: invalid n (too large or zero)");

  static std::array<std::unique_ptr<output_T>, n_cache> cached_values;

  unsigned const idx = n - 1L;
  bool has_value = cached_values[idx].get();

  if(has_value)
    return *cached_values[idx];

#ifdef _OPENMP
  if(CppAD::thread_alloc::in_parallel())
    throw std::runtime_error("integral_atomic<Type, Fam>::get_cached called in parallel mode");
#endif

  cached_values[idx].reset(new output_T("integral_atomic<Type, Fam>", n));

  return *cached_values[idx];
}

double const mlogit_fam::too_large = 30.;

using ADd   = CppAD::AD<double>;
using ADdd  = CppAD::AD<CppAD::AD<double> >;
using ADddd = CppAD::AD<CppAD::AD<CppAD::AD<double> > >;

template class entropy_term_integral<double>;
template class entropy_term_integral<ADd   >;
template class entropy_term_integral<ADdd  >;
template class entropy_term_integral<ADddd >;

template class integral_atomic<double, mlogit_fam>;
template class integral_atomic<ADd   , mlogit_fam>;
template class integral_atomic<ADdd  , mlogit_fam>;
template class integral_atomic<ADddd , mlogit_fam>;

template class integral_atomic<double, probit_fam>;
template class integral_atomic<ADd   , probit_fam>;
template class integral_atomic<ADdd  , probit_fam>;
template class integral_atomic<ADddd , probit_fam>;

} // namespace SNVA
} // namespace GaussHermite
