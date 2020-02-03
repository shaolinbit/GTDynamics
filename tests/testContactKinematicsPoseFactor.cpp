/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  testContactKinematicsPoseFactor.cpp
 * @brief test contact kinematics pose factor.
 * @Author: Alejandro Escontrela
 */

#include <ContactKinematicsPoseFactor.h>

#include <RobotModels.h>

#include <math.h>

#include <gtsam/base/numericalDerivative.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/factorTesting.h>

#include <CppUnitLite/TestHarness.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>

#include <iostream>

using gtsam::assert_equal;

/**
 * Test the evaluateError method with various link twists.
 **/
TEST(ContactKinematicsPoseFactor, error) {
  using simple_urdf::my_robot;

  gtsam::noiseModel::Gaussian::shared_ptr cost_model =
      gtsam::noiseModel::Gaussian::Covariance(gtsam::I_1x1);

  gtsam::LabeledSymbol pose_key = gtsam::LabeledSymbol('p', 0, 0);

  // Transform from the robot com to the link end.
  gtsam::Pose3 leTcom = my_robot.links()[0]->leTl_com();

  robot::ContactKinematicsPoseFactor factor(
      pose_key, cost_model, leTcom,
      (gtsam::Vector(3) << 0, 0, -9.8).finished());

  std::cout << leTcom << std::endl;

  // Leg oriented upwards with contact away from the ground.
  EXPECT(assert_equal(factor.evaluateError(gtsam::Pose3(
                          gtsam::Rot3(), gtsam::Point3(0., 0., 2.))),
                      (gtsam::Vector(1) << 3).finished()));

  // Leg oriented down with contact 1m away from the ground.
  EXPECT(assert_equal(factor.evaluateError(gtsam::Pose3(
                          gtsam::Rot3::Rx(M_PI), gtsam::Point3(0., 0., 2.))),
                      (gtsam::Vector(1) << 1).finished()));

  // Contact touching the ground.
  EXPECT(assert_equal(factor.evaluateError(gtsam::Pose3(
                          gtsam::Rot3::Rx(M_PI), gtsam::Point3(0., 0., 1.))),
                      (gtsam::Vector(1) << 0).finished()));

  // Check that Jacobian computation is correct by comparison to finite
  // differences.

  // Rotation and translation.
  gtsam::Values values_a;
  values_a.insert(
      pose_key,
      gtsam::Pose3(gtsam::Rot3::RzRyRx(M_PI / 8.0, M_PI / 12.0, 5 * M_PI / 6.0),
                   gtsam::Point3(4., 3., 3.)));
  EXPECT_CORRECT_FACTOR_JACOBIANS(
      factor, values_a,
      1e-7,  // Step used when computing numerical derivative jacobians.
      1e-3);   // Tolerance.

  // Pure translation.
  gtsam::Values values_b;
  values_b.insert(pose_key,
                  gtsam::Pose3(gtsam::Rot3(), gtsam::Point3(4., 3., 3.)));
  EXPECT_CORRECT_FACTOR_JACOBIANS(
      factor, values_b,
      1e-7,  // Step used when computing numerical derivative jacobians.
      1e-3);   // Tolerance.
}

/**
 * Test the optimization of a link twist to ensure zero
 * velocity at the contact point.
 **/
TEST(ContactKinematicsPoseFactor, optimization) {
  using simple_urdf::my_robot;

  gtsam::noiseModel::Gaussian::shared_ptr cost_model =
      gtsam::noiseModel::Constrained::All(1);

  gtsam::LabeledSymbol pose_key = gtsam::LabeledSymbol('p', 0, 0);

  // Transform from the robot com to the link end.
  gtsam::Pose3 leTcom = my_robot.links()[0]->leTl_com();

  robot::ContactKinematicsPoseFactor factor(
      pose_key, cost_model, leTcom,
      (gtsam::Vector(3) << 0, 0, -9.8).finished());

  // Initial link pose.
  gtsam::Pose3 link_pose_init = gtsam::Pose3(
      gtsam::Pose3(gtsam::Rot3::Rx(3 * M_PI / 4), gtsam::Point3(0., 0., 5.)));

  gtsam::NonlinearFactorGraph graph;
  graph.add(factor);
  gtsam::Values init_values;
  init_values.insert(pose_key, link_pose_init);

  gtsam::LevenbergMarquardtParams params;
  params.setVerbosity("ERROR");
  params.setAbsoluteErrorTol(1e-12);

  // Optimize the initial link twist to ensure contact touches the ground.
  gtsam::LevenbergMarquardtOptimizer optimizer(graph, init_values, params);
  optimizer.optimize();

  gtsam::Values results = optimizer.values();
  gtsam::Pose3 link_pose_optimized = results.at(pose_key).cast<gtsam::Pose3>();

  std::cout << "Initial Pose: " << std::endl;
  std::cout << link_pose_init << std::endl;

  std::cout << "Initial Pose Error: " << std::endl;
  std::cout << factor.evaluateError(link_pose_init) << std::endl;

  std::cout << "Optimized Pose: " << std::endl;
  std::cout << link_pose_optimized << std::endl;

  std::cout << "Optimized Pose Error: " << std::endl;
  std::cout << factor.evaluateError(link_pose_optimized) << std::endl;

  EXPECT(assert_equal(factor.evaluateError(link_pose_optimized),
                      (gtsam::Vector(1) << 0).finished(), 1e-3));
}

int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}