/* ----------------------------------------------------------------------------
 * Copyright 2020 The Ambitious Folks of the MRG
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file   DCMixtureFactor.h
 * @brief  DC Mixture factor
 * @author Kevin Doherty, kdoherty@mit.edu
 * @date   December 2021
 */

#pragma once

#include <gtsam/hybrid/DCGaussianMixtureFactor.h>
#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/nonlinear/Symbol.h>

#include <algorithm>
#include <boost/format.hpp>
#include <cmath>
#include <limits>
#include <vector>

#include "DCFactor.h"

namespace gtsam {

/**
 * @brief Implementation of a discrete conditional mixture factor. Implements a
 * joint discrete-continuous factor where the discrete variable serves to
 * "select" a mixture component corresponding to a NonlinearFactor type
 * of measurement.
 */
template <class NonlinearFactorType>
class DCMixtureFactor : public DCFactor {
  using FactorDecisionTree =
      DecisionTree<Key, boost::shared_ptr<NonlinearFactorType>>;

 private:
  FactorDecisionTree factors_;
  bool normalized_;

 public:
  using Base = DCFactor;
  using shared_ptr = boost::shared_ptr<DCMixtureFactor>;

  DCMixtureFactor() = default;

  DCMixtureFactor(const KeyVector& keys, const DiscreteKeys& discreteKeys,
                  const FactorDecisionTree& factors, bool normalized = false)
      : Base(keys, discreteKeys),
        discreteKeys_(discreteKeys),
        factors_(factors),
        normalized_(normalized) {}

  /**
   * @brief Convenience constructor that generates the underlying factor
   * decision tree for us.
   *
   * Here it is important that the vector of discrete keys and the vector of
   * factors have a 1-to-1 mapping so that the decision tree is constructed
   * accordingly.
   *
   * @param keys Vector of keys for continuous factors.
   * @param discreteKeys Vector of discrete keys.
   * @param factors Vector of factors. Each factor should map to its
   * corresponding assigned discrete key.
   * @param normalized Flag indicating if the factor error is already
   * normalized.
   */
  DCMixtureFactor(const KeyVector& keys, const DiscreteKeys& discreteKeys,
                  const vector<NonlinearFactorType>& factors,
                  bool normalized = false)
      : Base(keys, discreteKeys),
        discreteKeys_(discreteKeys),
        normalized_(normalized) {
    // Generate the decision tree based on the discreteKey to factor mapping.
    factors_ = DecisionTree(discreteKeys, factors);
  }

  DCMixtureFactor& operator=(const DCMixtureFactor& rhs) {
    Base::operator=(rhs);
    this->factors_ = rhs.factors_;
  }

  ~DCMixtureFactor() = default;

  double error(const Values& continuousVals,
               const DiscreteValues& discreteVals) const override {
    /********************************************************/
    // Retrieve the factor corresponding to the assignment in discreteVals.
    auto factor = factors_(discreteVals);
    // Compute the error for the selected factor
    const double factorError = factor->error(continuousVals);

    if (normalized_) return factorError;
    return factorError + this->nonlinearFactorLogNormalizingConstant(
                             this->factors_[assignment], continuousVals);
  }

  size_t dim() const override {
    // TODO(kevin) Need to modify this? Maybe we take discrete vals as parameter
    // and DCContinuousFactor will pass this in as needed.
    return (factors_.size() > 0) ? factors_[0].dim() : 0;
  }

  /// Testable
  /// @{

  /// print to stdout
  void print(
      const std::string& s = "DCMixtureFactor",
      const KeyFormatter& formatter = DefaultKeyFormatter) const override {
    std::cout << (s.empty() ? "" : s + " ");
    std::cout << "(";
    for (Key key : keys()) {
      std::cout << " " << formatter(key);
    }
    std::cout << "; " << formatter(discreteKey().first) << " ) {\n";
    for (size_t i = 0; i < factors_.size(); i++) {
      auto t = boost::format("component %1%: ") % i;
      factors_[i].print(t.str());
    }
    std::cout << "}\n";
  }

  /// Check equality
  bool equals(const DCFactor& other, double tol = 1e-9) const override {
    // We attempt a dynamic cast from DCFactor to DCMixtureFactor. If it fails,
    // return false.
    if (!dynamic_cast<const DCMixtureFactor*>(&other)) return false;

    // If the cast is successful, we'll properly construct a DCMixtureFactor
    // object from `other`
    const DCMixtureFactor& f(static_cast<const DCMixtureFactor&>(other));

    // Ensure that this DCMixtureFactor and `f` have the same number of
    // component factors in `factors_`.
    if (factors_.size() != f.factors_.size()) return false;

    // If the number of factors is the same, we compare them individually (they
    // should be in the same order!). If any fail to match, return false.
    for (size_t i = 0; i < factors_.size(); i++) {
      if (!factors_[i].equals(f.factors_[i])) return false;
    }

    // If everything above passes, and the keys_, discreteKeys_ and normalized_
    // member variables are identical, return true.
    return (std::equal(keys_.begin(), keys_.end(), f.keys().begin()) &&
            (discreteKeys_ == f.discreteKeys_) &&
            (normalized_ == f.normalized_));
  }

  /// @}

  /// Linearize specific nonlinear factors based on the assignment in
  /// discreteValues.
  GaussianFactor::shared_ptr linearize(
      const Values& continuousVals,
      const DiscreteValues& discreteVals) const override {
    // Retrieve the assignment to our discrete key.
    const size_t assignment = discreteVals.at(dk_.first);

    // `assignment` indexes the nonlinear factors we have stored to compute the
    // error.
    return factors_[assignment].linearize(continuousVals);
  }

  /// Linearize all the continuous factors to get a DCGaussianMixtureFactor.
  DCFactor::shared_ptr linearize(const Values& continuousVals) const override {
    std::vector<GaussianFactor::shared_ptr> linearized_factors;
    for (size_t i = 0; i < factors_.size(); i++) {
      auto linearized = factors_[i].linearize(continuousVals);
      linearized_factors.push_back(linearized);
    }
    return boost::make_shared<DCGaussianMixtureFactor>(keys_, dk_,
                                                       linearized_factors);
  }

  /**
   * If the component factors are not already normalized, we want to compute
   * their normalizing constants so that the resulting joint distribution is
   * appropriately computed. Remember, this is the _negative_ normalizing
   * constant for the measurement likelihood (since we are minimizing the
   * _negative_ log-likelihood).
   */
  double nonlinearFactorLogNormalizingConstant(
      const NonlinearFactorType& factor, const Values& values) const {
    // Information matrix (inverse covariance matrix) for the factor.
    Matrix infoMat;

    // NOTE: This is sloppy, is there a cleaner way?
    boost::shared_ptr<NonlinearFactorType> fPtr =
        boost::make_shared<NonlinearFactorType>(factor);
    boost::shared_ptr<NonlinearFactor> factorPtr(fPtr);

    // If this is a NoiseModelFactor, we'll use its noiseModel to
    // otherwise noiseModelFactor will be nullptr
    boost::shared_ptr<NoiseModelFactor> noiseModelFactor =
        boost::dynamic_pointer_cast<NoiseModelFactor>(factorPtr);
    if (noiseModelFactor) {
      // If dynamic cast to NoiseModelFactor succeeded, see if the noise model
      // is Gaussian
      noiseModel::Base::shared_ptr noiseModel = noiseModelFactor->noiseModel();

      boost::shared_ptr<noiseModel::Gaussian> gaussianNoiseModel =
          boost::dynamic_pointer_cast<noiseModel::Gaussian>(noiseModel);
      if (gaussianNoiseModel) {
        // If the noise model is Gaussian, retrieve the information matrix
        infoMat = gaussianNoiseModel->information();
      } else {
        // If the factor is not a Gaussian factor, we'll linearize it to get
        // something with a normalized noise model
        // TODO(kevin): does this make sense to do? I think maybe not in
        // general? Should we just yell at the user?
        boost::shared_ptr<GaussianFactor> gaussianFactor =
            factor.linearize(values);
        infoMat = gaussianFactor->information();
      }
    }

    // Compute the (negative) log of the normalizing constant
    return -(factor.dim() * log(2.0 * M_PI) / 2.0) -
           (log(infoMat.determinant()) / 2.0);
  }
};

}  // namespace gtsam
