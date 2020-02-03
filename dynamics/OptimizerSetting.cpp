/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  OptimizerSetting.h
 * @brief Factor graph optimizer settings.
 * @Author: Mandy Xie
 */

#include "dynamics/OptimizerSetting.h"


namespace manipulator {

OptimizerSetting::OptimizerSetting()
    : total_step(120),
      total_time(12),
      bp_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 0.00001)),
      bv_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 0.00001)),
      ba_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 0.00001)),
      p_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 0.001)),
      v_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 1)),
      a_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 1)),
      f_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 1)),
      t_cost_model(gtsam::noiseModel::Isotropic::Sigma(1, 1)),
      q_cost_model(gtsam::noiseModel::Isotropic::Sigma(1, 0.001)),
      qv_cost_model(gtsam::noiseModel::Isotropic::Sigma(1, 0.001)),
      qa_cost_model(gtsam::noiseModel::Isotropic::Sigma(1, 0.001)),
      tf_cost_model(gtsam::noiseModel::Isotropic::Sigma(6, 0.001)),
      opt_type(LM),
      opt_verbosity(None),
      rel_thresh(1e-2),
      max_iter(50),
      epsilon(0.2) {}

void OptimizerSetting::setToolPoseCostModel(const double sigma) {
  tp_cost_model = gtsam::noiseModel::Isotropic::Sigma(6, sigma);
}

void OptimizerSetting::setJointLimitCostModel(const double sigma) {
  jl_cost_model = gtsam::noiseModel::Isotropic::Sigma(1, sigma);
}

void OptimizerSetting::setQcModel(const gtsam::Matrix &Qc) {
  Qc_model = gtsam::noiseModel::Gaussian::Covariance(Qc);
}

void OptimizerSetting::setQcModelPose3(const gtsam::Matrix &Qc) {
  Qc_model_pose3 = gtsam::noiseModel::Gaussian::Covariance(Qc);
}
}  // namespace manipulator