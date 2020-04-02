#define INCLUDE_RCPP
#include "tmb_includes.h"
#include "utils.h"
#include "snva.h"
#include "gva.h"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {
/* redefine the macros used by TMB */
template<class Type>
vector<Type> get_vec(SEXP obj){
  Rcpp::NumericVector org(obj);
  std::size_t const n = org.size();
  vector<Type> out(n);

  double const *o = &org(0);
  for(unsigned i = 0; i < n; i++, o++)
    out[i] = Type(*o);

  return out;
}

template<class Type>
matrix<Type> get_mat(SEXP obj){
  Rcpp::NumericMatrix org(obj);
  std::size_t const n = org.nrow(),
                    m = org.ncol();
  matrix<Type> out(n, m);

  double const *o = &org(0, 0);
  for(unsigned j = 0; j < m; j++)
    for(unsigned i = 0; i < n; i++, o++)
      out(i, j) = Type(*o);

  return out;
}

#ifdef DATA_STRING
#undef DATA_STRING
#endif
#define DATA_STRING(name)                                      \
  std::string name = data["" #name ""];

#ifdef DATA_VECTOR
#undef DATA_VECTOR
#endif
#define DATA_VECTOR(name)                                      \
  vector<Type> name = get_vec<Type>(data["" #name ""]);

#ifdef DATA_MATRIX
#undef DATA_MATRIX
#endif
#define DATA_MATRIX(name)                                      \
  matrix<Type> name = get_mat<Type>(data["" #name ""]);

#ifdef DATA_IVECTOR
#undef DATA_IVECTOR
#endif
#define DATA_IVECTOR(name)                                     \
  vector<int> name = ([&](){                                   \
    Rcpp::IntegerVector org = data["" #name ""];               \
    std::size_t const n = org.size();                          \
    vector<int> out(n);                                        \
                                                               \
    int const *o = &org(0);                                    \
    for(unsigned i = 0; i < n; ++o, ++i)                       \
      out[i] = org(i);                                         \
                                                               \
    return out;                                                \
  })();

#ifdef DATA_INTEGER
#undef DATA_INTEGER
#endif
#define DATA_INTEGER(name)                                     \
  int name = data["" #name ""];

#ifdef PARAMETER
#undef PARAMETER
#endif
#define PARAMETER(name)                                        \
  double name = parameters["" #name ""];

#ifdef PARAMETER_VECTOR
#undef PARAMETER_VECTOR
#endif
#define PARAMETER_VECTOR(name)                                 \
  vector<Type> name =                                          \
    get_vec<Type>(parameters["" #name ""]);

template<typename Tout, typename Tin>
vector<Tout> get_args_va
  (Tin const &eps, Tin const &kappa, vector<Tin> const &b,
   vector<Tin> const &theta, vector<Tin> const &theta_VA) {
  std::size_t const n_b = b       .size(),
                    n_t = theta   .size(),
                    n_v = theta_VA.size();
  vector<Tout> out(2L + n_b + n_t + n_v);
  out[0] = eps;
  out[1] = kappa;
  Tout *o = &out[2];

  for(unsigned i = 0; i < n_b; ++i, ++o)
    *o = Tout(b[i]);
  for(unsigned i = 0; i < n_t; ++i, ++o)
    *o = Tout(theta[i]);
  for(unsigned i = 0; i < n_v; ++i, ++o)
    *o = Tout(theta_VA[i]);

  return out;
}

template<typename Tout>
vector<Tout> get_args_va(Rcpp::List parameters) {
  using Type = double;
  PARAMETER(eps);
  PARAMETER(kappa);
  PARAMETER_VECTOR(b);
  PARAMETER_VECTOR(theta);
  PARAMETER_VECTOR(theta_va);

  return get_args_va<Tout, Type>(eps, kappa, b, theta, theta_va);
}

} // namespace

/* define and declare functor to use */
template<class Type>
class VA_worker {
  Rcpp::List data, parameters;

  SETUP_DATA;
  PARAMETER_VECTOR(theta_VA);
  DATA_INTEGER(n_nodes);
  DATA_STRING(param_type);

  std::size_t const n_b = b       .size(),
                    n_t = theta   .size(),
                    n_v = theta_VA.size(),
                 n_para = 2L + n_b + n_t + n_v;

public:
  VA_worker(Rcpp::List data, Rcpp::List parameters):
  data(data), parameters(parameters) {
    SETUP_DATA_CHECK;

#ifdef _OPENMP
    omp_set_num_threads(n_threads);
#endif
  }

  template<typename Tout>
  vector<Tout> get_args_va() const {
    return ::get_args_va<Tout, Type>(eps, kappa, b, theta, theta_VA);
  }

  Type operator()(vector<Type> &args){
    if((unsigned)args.size() != n_para)
      error("VA_worker: invalid args length");
    Type eps = args[0],
       kappa = args[1];
    Type const *a = &args[2];

    vector<Type> b(n_b);
    for(unsigned i = 0; i < n_b; ++i, ++a)
      b[i] = *a;
    vector<Type> theta(n_t);
    for(unsigned i = 0; i < n_t; ++i, ++a)
      theta[i] = *a;
    vector<Type> theta_VA(n_v);
    for(unsigned i = 0; i < n_v; ++i, ++a)
      theta_VA[i] = *a;

    survTMB::accumulator_mock<Type> result;
    if(app_type =="GVA"){
      GVA(COMMON_CALL, theta_VA, n_nodes);
      return result;

    } else if(app_type == "SNVA"){
      SNVA(COMMON_CALL, theta_VA, n_nodes, param_type);
      return result;

    }

    error("VA_worker<Type>: approximation method '%s' is not implemented",
          app_type.c_str());
    return Type(0);
  }
};

struct VA_func {
  using ADd = CppAD::AD<double>;
  CppAD::ADFun<double> func;

  VA_func(Rcpp::List data, Rcpp::List parameters){
    VA_worker<ADd> w(data, parameters);
    auto args = w.get_args_va<ADd>();
    CppAD::Independent(args);
    vector<ADd> y(1);
    y[0] = w(args);
    func.Dependent(args, y);
    func.optimize();
  }
};

// [[Rcpp::export]]
SEXP get_VA_funcs
  (Rcpp::List data, Rcpp::List parameters){
  return Rcpp::XPtr<VA_func>(new VA_func(data, parameters));
}

// [[Rcpp::export]]
double VA_funcs_eval_lb
  (SEXP p, SEXP par){
  Rcpp::XPtr<VA_func > ptr(p);
  vector<double> parv = get_vec<double>(par);

  auto y = ptr->func.Forward(0, parv);
  return y[0];
}

// [[Rcpp::export]]
Rcpp::NumericVector VA_funcs_eval_grad
  (SEXP p, SEXP par){
  Rcpp::XPtr<VA_func > ptr(p);
  vector<double> parv = get_vec<double>(par);

  ptr->func.Forward(0, parv);
  vector<double> w(1);
  w[0] = 1;
  vector<double> grad = ptr->func.Reverse(1, w);

  std::size_t const n = grad.size();
  Rcpp::NumericVector out(n);
  for(unsigned i = 0; i < n; ++i)
    out[i] = grad[i];

  return out;
}
