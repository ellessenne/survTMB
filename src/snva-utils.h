#ifndef SNVA_UTILS_H
#define SNVA_UTILS_H

#include "gaus-hermite.h"
#include "pnorm-log.h"
#include "utils.h"
#include "gamma-to-nu.h"

namespace GaussHermite {
namespace SNVA {

/*
 Compute psi function w/ adaptive Gauss–Hermite quadrature. That is,

 \begin{align*}
 f(\sigma^2) &= \int 2 \phi(z; \sigma^2)\Phi(z)\log \Phi(z)dz \\
 &= \frac 2{\sqrt{2\pi\sigma^2}}
 \int \exp\left(-\frac {z^2}{2\sigma^2}\right)\Phi(z)\log \Phi(z)dz \\
 &= \frac 2{\sqrt{2\pi\sigma^2}}
 \int \exp\left(-\frac {z^2}
 {2\gamma^2\sigma^2/(\gamma^2 + \sigma^2)}\right)
 \underbrace{\exp\left(\frac {z^2}{2\gamma^2}\right)
 \Phi(z)\log \Phi(z)}_{f(z;\gamma)}dz \\
 &\overset{z = s \sqrt{2}\gamma\sigma/\sqrt{\gamma^2 + \sigma^2}}{=}
 \frac 2{\sigma\sqrt{2\pi}}
 \frac {\sqrt 2\sigma\gamma}{\sqrt{\gamma^2 + \sigma^2}}
 \int \exp\left(-s^2\right)
 f(s \sqrt{2}\gamma\sigma/\sqrt{\gamma^2 + \sigma^2};\gamma)ds \\
 &\approx
 \frac {2\gamma}{\sqrt{\pi(\gamma^2 + \sigma^2)}}
 \sum_{i = 1}^n
 w_i f(x_i \sqrt{2}\gamma\sigma/\sqrt{\gamma^2 + \sigma^2})
 \end{align*}

 with \gamma(\sigma) = 1.
*/
template<class Type>
Type entropy_term
  (Type const sigma_sq, HermiteData<Type> const &hd){
  unsigned const n_nodes = hd.x.size();
  Type const gamma(1.),
          gamma_sq = gamma * gamma,
               two(2.);

  Type out(0.);
  Type const
    mult_sum(Type(M_2_SQRTPI) * gamma / sqrt(sigma_sq + gamma_sq)),
        mult(mult_sum * sqrt(sigma_sq) / Type(sqrt(M_2_PI)));

  for(unsigned i = 0; i < n_nodes; ++i){
    Type const xi = hd.x[i] * mult;
    out +=
      hd.w[i] * exp(xi * xi / two / gamma_sq) * pnorm(xi) * pnorm_log(xi);
  }

  return mult_sum * out;
}

/* Computes the approximate mode of the skew-normal distribution and the
 * Hessian at the approximation */
template<class Type>
struct mode_n_Hess {
  Type mode, Hess;
};

template<class Type>
mode_n_Hess<Type> get_SNVA_mode_n_Hess
  (Type const mu, Type const sigma, Type const rho){
  Type const one(1.),
             two(2.),
            zero(0.),
             eps(std::numeric_limits<double>::epsilon()),
           alpha = sigma * rho,
          a_sign = CppAD::CondExpLe(alpha, zero, -one, one),
              nu = Type(sqrt(M_2_PI)) * alpha / sqrt(1. + alpha * alpha),
           nu_sq = nu * nu,
           gamma = Type((4. - M_PI) / 2.) * nu_sq * nu /
             pow(one - nu_sq, Type(3./2.)),
            mode = mu + sigma * (
              nu - gamma * sqrt(one - nu_sq) / two -
                a_sign / two * exp(- Type(2. * M_PI) / (
                    a_sign * alpha + eps))),
               z = rho * (mode - mu),
             phi = dnorm(z, zero, one),
             Phi = pnorm(z),
            Hess = - one / sigma / sigma - rho * rho * phi * (
              z * Phi + phi) / (Phi * Phi + eps);

  return { mode, Hess };
}

/* Makes an approximation of
 l(\mu,\sigma, \rho) =\frac{2}{\sigma\sqrt{2\pi}}
 \int \exp\left(-\frac{(z - \mu)^2}{2\sigma^2}\right)
 \Phi(\rho(z - \mu))
 \log(1 + \exp(z)) dz

 and the mode of the random effect density. The approximation seems to
 poor if there is a large skew.
 */
template<class Type>
Type mlogit_integral
  (Type const mu, Type const sigma, Type const rho,
   HermiteData<Type> const &hd){
  /* compute xi and lambda and assign constants */
  auto const dvals = get_SNVA_mode_n_Hess(mu, sigma, rho);
  Type const one(1.),
             two(2.),
        too_large(30.),
         sigma_sq = sigma * sigma,
               xi = dvals.mode,
           lambda = one / sqrt(-dvals.Hess);

  /* compute approximate integral with Gauss–Hermite quadrature */
  Type out(0.);
  Type const
    mult_sum(lambda / sigma * Type(M_2_SQRTPI)),
    mult    (Type(M_SQRT2) * lambda);

  for(unsigned i = 0; i < hd.x.size(); ++i){
    Type const o = hd.x[i],
              xo = xi + mult * o,
         xo_diff = (xo - mu),
             fac = CppAD::CondExpGe(
               xo, too_large, xo, log(one + exp(xo)));

    out +=
      hd.w[i] * exp(o * o - xo_diff * xo_diff / two / sigma_sq) *
      pnorm(rho * (xo - mu)) * fac;
  }

  return mult_sum * out;
}

template<class Type>
Type mlogit_integral
  (Type const mu, Type const sigma, Type const rho, Type const log_k,
   HermiteData<Type> const &hd){
  return mlogit_integral(mu + log_k, sigma, rho, hd);
}

/* Makes an approximation of
 l(\mu,\sigma, \rho) =
 2\int\phi(z;\mu,\sigma^2)
 \Phi(\rho(z - \mu))
 (-\log \Phi(z)) dz

 and the mode of the random effect density. The approximation seems to
 poor if there is a large skew.
 */
template<class Type>
Type probit_integral
  (Type const mu, Type const sigma, Type const rho,
   HermiteData<Type> const &hd){
  /* compute xi and lambda and assign constants */
  auto const dvals = get_SNVA_mode_n_Hess(mu, sigma, rho);
  Type const one(1.),
             two(2.),
        sigma_sq = sigma * sigma,
              xi = dvals.mode,
          lambda = one / sqrt(-dvals.Hess);

  /* compute approximate integral with Gauss–Hermite quadrature */
  Type out(0.);
  Type const
    mult_sum(lambda / sigma * Type(M_2_SQRTPI)),
    mult    (Type(M_SQRT2) * lambda);

  for(unsigned i = 0; i < hd.x.size(); ++i){
    Type const o = hd.x[i],
              xo = xi + mult * o,
         xo_diff = (xo - mu);

    out +=
      hd.w[i] * exp(o * o - xo_diff * xo_diff / two / sigma_sq) *
      pnorm(rho * (xo - mu)) * (-pnorm_log(xo));
  }

  return mult_sum * out;
}

template<class Type>
Type probit_integral
  (Type const mu, Type const sigma, Type const rho, Type const k,
   HermiteData<Type> const &hd){
  return probit_integral(k - mu, sigma, -rho, hd);
}

/* The following functions maps from the input vector to the
 * parameterization used in
 * > Ormerod, J. T. (2011). Skew-normal variational approximations for
 * > Bayesian inference. Unpublished article. */
template<class Type>
struct SNVA_MD_input {
  std::vector<vector<Type> > va_mus,
                             va_rhos;
  std::vector<matrix<Type> > va_lambdas;
};

template<class Type>
class get_gamma{
  Type const c1 = Type(0.99527),
             c2 = Type(2.) * c1,
            one = Type(1.);

public:
  /* inverse of 2 * c1 * logit(gamma) - c1 */
  Type operator()(Type const gtrans) const {
    return c2 / (one + exp(-gtrans)) - c1;
  }
};

/* maps from direct parameters vector to direct parameters. See
 * get_vcov_from_trian. */
template<class Type>
SNVA_MD_input<Type> SNVA_MD_theta_DP_to_DP
  (vector<Type> const &theta_VA, unsigned const rng_dim){
  using survTMB::get_vcov_from_trian;
  using vecT = vector<Type>;
  using std::move;
  unsigned const n_groups = theta_VA.size() / (
    rng_dim * 2L + (rng_dim * (rng_dim + 1L)) / 2L);

  SNVA_MD_input<Type> out;
  std::vector<vecT > &va_mus = out.va_mus,
                     &va_rhos = out.va_rhos;
  std::vector<matrix<Type> > &va_lambdas = out.va_lambdas;

  va_mus    .reserve(n_groups);
  va_rhos   .reserve(n_groups);
  va_lambdas.reserve(n_groups);

  Type const *t = &theta_VA[0];
  for(unsigned g = 0; g < n_groups; ++g){
    /* insert new mu vector */
    vecT mu_vec(rng_dim);
    for(unsigned i = 0; i < rng_dim; ++i)
      mu_vec[i] = *t++;
    va_mus.emplace_back(move(mu_vec));

    /* insert new lambda matrix */
    va_lambdas.emplace_back(get_vcov_from_trian(t, rng_dim));
    t += (rng_dim * (rng_dim + 1L)) / 2L;

    /* insert new rho vector */
    vecT rho_vec(rng_dim);
    for(unsigned i = 0; i < rng_dim; ++i)
      rho_vec[i] = *t++;
    va_rhos.emplace_back(move(rho_vec));
  }

  return out;
}

/* maps from mean, Covariance matrix, and __transformed__
 * Pearson's moment coefficient of skewness to direct parameters. See
 * get_vcov_from_trian */
template<class Type>
SNVA_MD_input<Type> SNVA_MD_theta_CP_trans_to_DP
  (vector<Type> const &theta_VA, unsigned const rng_dim){
  using survTMB::get_vcov_from_trian;
  using vecT = vector<Type>;
  using std::move;
  unsigned const n_mu = rng_dim,
             n_lambda = (rng_dim * (rng_dim + 1L)) / 2L,
              n_per_g = n_mu + n_lambda + rng_dim,
             n_groups = theta_VA.size() / n_per_g;

  SNVA_MD_input<Type> out;
  std::vector<vecT > &va_mus = out.va_mus,
                     &va_rhos = out.va_rhos;
  std::vector<matrix<Type> > &va_lambdas = out.va_lambdas;

  va_mus    .reserve(n_groups);
  va_rhos   .reserve(n_groups);
  va_lambdas.reserve(n_groups);

  get_gamma<Type> trans_g;
  Type const *t = &theta_VA[0],
            one(1.),
            two(2.),
            Tpi(M_PI),
        sqrt_pi(sqrt(M_PI));

  /* intermediaries */
  vecT gamma(rng_dim),
          nu(rng_dim),
       omega(rng_dim);

  for(unsigned g = 0; g < n_groups; ++g, t += n_per_g){
    /* get gamma parameters */
    Type const *gi = t + n_mu + n_lambda;
    for(unsigned i = 0; i < rng_dim; ++i)
      gamma[i] = trans_g(*gi++);

    /* Compute intermediaries and rho */
    vecT rhos(rng_dim);
    matrix<Type> Sigma = get_vcov_from_trian(t + n_mu, rng_dim);
    for(unsigned i = 0; i < rng_dim; ++i){
      Type const &gv = gamma[i];
      nu[i]    = gamma_to_nu(gv);
      omega[i] = sqrt(Sigma(i, i) / (one - nu[i] * nu[i]));
      rhos[i]  =
        sqrt_pi * nu[i] / omega[i] / sqrt(two - Tpi * nu[i] * nu[i]);

      /* replace nu by nu * omega */
      nu[i] *= omega[i];
    }
    va_rhos.emplace_back(move(rhos));

    /* assign mu and Lambda */
    vecT mu(rng_dim);
    Type const *mi = t;
    for(unsigned i = 0; i < rng_dim; ++i)
      mu[i] = *mi++;
    mu -= nu;
    va_mus.emplace_back(move(mu));

    auto const nu_vec = nu.matrix();
    Sigma += nu_vec * nu_vec.transpose();
    va_lambdas.emplace_back(move(Sigma));
  }

  return out;
}

} // namespace SNVA
} // namespace GaussHermite

#endif