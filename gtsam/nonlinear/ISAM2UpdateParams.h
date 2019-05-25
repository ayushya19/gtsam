/* ----------------------------------------------------------------------------
 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file    ISAM2UpdateParams.h
 * @brief   Class that stores extra params for ISAM2::update()
 * @author  Michael Kaess, Richard Roberts, Frank Dellaert, Jose Luis Blanco
 */

// \callgraph

#pragma once

#include <boost/optional.hpp>
#include <gtsam/base/FastList.h>
#include <gtsam/dllexport.h>             // GTSAM_EXPORT
#include <gtsam/inference/Key.h>         // Key, KeySet
#include <gtsam/nonlinear/ISAM2Result.h> //FactorIndices

namespace gtsam {

/**
 * @addtogroup ISAM2
 * This struct is used by ISAM2::update() to pass additional parameters to
 * give the user a fine-grained control on how factors and relinearized, etc.
 */
struct GTSAM_EXPORT ISAM2UpdateParams {
  ISAM2UpdateParams() = default;

  /** Indices of factors to remove from system (default: empty) */
  FactorIndices removeFactorIndices;

  /* An optional map of keys to group labels, such that a variable can be
   * constrained to a particular grouping in the BayesTree */
  boost::optional<FastMap<Key, int>> constrainedKeys{boost::none};

  /* An optional set of nonlinear keys that iSAM2 will hold at a constant
   * linearization point, regardless of the size of the linear delta */
  boost::optional<FastList<Key>> noRelinKeys{boost::none};

  /* An optional set of nonlinear keys that iSAM2 will re-eliminate, regardless
   * of the size of the linear delta. This allows the provided keys to be
   * reordered. */
  boost::optional<FastList<Key>> extraReelimKeys{boost::none};

  /* Relinearize any variables whose delta magnitude is sufficiently large
   * (Params::relinearizeThreshold), regardless of the relinearization
   * interval (Params::relinearizeSkip). */
  bool force_relinearize{false};
};

} // namespace gtsam
