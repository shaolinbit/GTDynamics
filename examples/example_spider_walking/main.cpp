/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file  main.cpp
 * @brief Spider trajectory optimization with pre-specified footholds.
 * @author: Alejandro Escontrela, Stephanie McCormick, Disha Das, Tarushree
 * Gandhi, Varun Agrawal
 */

#include <CppUnitLite/TestHarness.h>
#include <gtdynamics/dynamics/DynamicsGraph.h>
#include <gtdynamics/dynamics/OptimizerSetting.h>
#include <gtdynamics/factors/MinTorqueFactor.h>
#include <gtdynamics/universal_robot/Robot.h>
#include <gtdynamics/universal_robot/sdf.h>
#include <gtdynamics/utils/DynamicsSymbol.h>
#include <gtdynamics/utils/Phase.h>
#include <gtdynamics/utils/Trajectory.h>
#include <gtdynamics/utils/WalkCycle.h>
#include <gtdynamics/utils/initialize_solution_utils.h>
#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtParams.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>

#include <algorithm>
#include <boost/algorithm/string/join.hpp>
#include <boost/optional.hpp>
#include <fstream>
#include <iostream>
#include <utility>

#define GROUND_HEIGHT -1.75  //-1.75

using std::string;
using std::vector;

using gtdynamics::ContactPoint;
using gtdynamics::ContactPoints;
using gtdynamics::Phase;
using gtdynamics::PhaseKey;
using gtdynamics::Robot;
using gtdynamics::ZeroValues;
using gtdynamics::internal::JointAccelKey;
using gtdynamics::internal::JointAngleKey;
using gtdynamics::internal::JointVelKey;
using gtdynamics::internal::PoseKey;
using gtdynamics::internal::TorqueKey;
using gtdynamics::internal::TwistAccelKey;
using gtdynamics::internal::TwistKey;

using gtsam::Vector6;
using gtsam::noiseModel::Isotropic;

// Returns a Trajectory object for a single spider walk cycle.
gtdynamics::Trajectory getTrajectory(vector<string> links, Robot robot,
                                     size_t repeat = 3) {
  gtdynamics::Phase stationary(robot, 40);
  stationary.addContactPoints(links, gtsam::Point3(0, 0.19, 0), GROUND_HEIGHT);

  gtdynamics::Phase odd(robot, 20);
  odd.addContactPoints(
      {{"tarsus_1_L1", "tarsus_3_L3", "tarsus_5_R4", "tarsus_7_R2"}},
      gtsam::Point3(0, 0.19, 0), GROUND_HEIGHT);

  gtdynamics::Phase even(robot, 20);
  even.addContactPoints(
      {{"tarsus_2_L2", "tarsus_4_L4", "tarsus_6_R3", "tarsus_8_R1"}},
      gtsam::Point3(0, 0.19, 0), GROUND_HEIGHT);

  gtdynamics::WalkCycle walk_cycle;
  walk_cycle.addPhase(stationary);
  walk_cycle.addPhase(even);
  walk_cycle.addPhase(stationary);
  walk_cycle.addPhase(odd);

  gtdynamics::Trajectory trajectory(walk_cycle, repeat);
  return trajectory;
}

int main(int argc, char **argv) {
  // Load Stephanie's spider robot.
  auto spider = gtdynamics::CreateRobotFromFile(
      gtdynamics::kSdfPath + string("/spider_alt.sdf"), "spider");

  double sigma_dynamics = 1e-5;    // std of dynamics constraints.
  double sigma_objectives = 1e-6;  // std of additional objectives.
  double sigma_joints = 1.85e-4;   // 1.85e-4

  // Noise models.
  auto dynamics_model_6 = Isotropic::Sigma(6, sigma_dynamics),
       dynamics_model_3 = Isotropic::Sigma(3, sigma_dynamics),
       dynamics_model_1 = Isotropic::Sigma(1, sigma_dynamics),
       dynamics_model_1_2 = Isotropic::Sigma(1, sigma_joints),
       objectives_model_6 = Isotropic::Sigma(6, sigma_objectives),
       objectives_model_3 = Isotropic::Sigma(3, sigma_objectives),
       objectives_model_1 = Isotropic::Sigma(1, sigma_objectives);

  auto opt = gtdynamics::OptimizerSetting(sigma_dynamics);
  auto graph_builder = gtdynamics::DynamicsGraph(opt);

  // Env parameters.
  gtsam::Vector3 gravity(0, 0, -9.8);
  double mu = 1.0;

  vector<string> links = {"tarsus_1_L1", "tarsus_2_L2", "tarsus_3_L3",
                          "tarsus_4_L4", "tarsus_5_R4", "tarsus_6_R3",
                          "tarsus_7_R2", "tarsus_8_R1"};
  auto spider_trajectory = getTrajectory(links, spider);

  // Get phase information
  vector<ContactPoints> phase_cps = spider_trajectory.phaseContactPoints();
  vector<int> phase_durations = spider_trajectory.phaseDurations();
  vector<Robot> robots = spider_trajectory.phaseRobotModels();

  // Define noise to be added to initial values, desired timestep duration,
  // vector of link name strings, robot model for each phase, and
  // phase transition initial values.
  double gaussian_noise = 1e-5;
  double dt_des = 1. / 240;
  vector<gtsam::Values> transition_graph_init =
      spider_trajectory.transitionPhaseInitialValues(gaussian_noise);

  // Get final time step.
  int t_f = spider_trajectory.getEndTimeStep(spider_trajectory.numPhases() -
                                             1);  // Final timestep.

  // Collocation scheme.
  auto collocation = gtdynamics::CollocationScheme::Euler;

  // Graphs for transition between phases + their initial values.
  vector<gtsam::NonlinearFactorGraph> transition_graphs =
      spider_trajectory.getTransitionGraphs(graph_builder, mu);

  // Construct the multi-phase trajectory factor graph.
  // TODO: Pass Trajectory here
  std::cout << "Creating dynamics graph" << std::endl;
  auto graph = graph_builder.multiPhaseTrajectoryFG(
      robots, phase_durations, transition_graphs, collocation, phase_cps, mu);

  // Build the objective factors.
  gtsam::NonlinearFactorGraph objective_factors;
  auto base_link = spider.link("body");

  std::map<string, gtdynamics::LinkSharedPtr> link_map;
  for (auto &&link : links)
    link_map.insert(std::make_pair(link, spider.link(link)));

  // Previous contact point goal.
  std::map<string, gtsam::Point3> prev_cp =
      spider_trajectory.initContactPointGoal();

  // Distance to move contact point per time step during swing.
  auto contact_offset = gtsam::Point3(0, 0.02, 0);

  // Add contact point objectives to factor graph.
  for (int p = 0; p < spider_trajectory.numPhases(); p++) {
    // if(p <2) contact_offset /=2 ;
    // Phase start and end timesteps.
    int t_p_i = spider_trajectory.getStartTimeStep(p);
    int t_p_f = spider_trajectory.getEndTimeStep(p);

    // Obtain the contact links and swing links for this phase.
    vector<string> phase_contact_links =
        spider_trajectory.getPhaseContactLinks(p);
    vector<string> phase_swing_links = spider_trajectory.getPhaseSwingLinks(p);

    for (int t = t_p_i; t <= t_p_f; t++) {
      // Normalized phase progress.
      double t_normed = (double)(t - t_p_i) / (double)(t_p_f - t_p_i);

      for (auto &&pcl : phase_contact_links)
        objective_factors.add(spider_trajectory.pointGoalFactor(
            pcl, t, Isotropic::Sigma(3, 1e-7),  // 1e-7
            gtsam::Point3(prev_cp[pcl].x(), prev_cp[pcl].y(),
                          GROUND_HEIGHT - 0.05)));

      double h =
          GROUND_HEIGHT + std::pow(t_normed, 1.1) * std::pow(1 - t_normed, 0.7);

      for (auto &&psl : phase_swing_links)
        objective_factors.add(spider_trajectory.pointGoalFactor(
            psl, t, Isotropic::Sigma(3, 1e-7),
            gtsam::Point3(prev_cp[psl].x(), prev_cp[psl].y(), h)));

      // Update the goal point for the swing links.
      for (auto &&psl : phase_swing_links)
        prev_cp[psl] = prev_cp[psl] + contact_offset;
    }
  }

  // Add base goal objectives to the factor graph.
  for (int t = 0; t <= t_f; t++) {
    objective_factors.add(gtsam::PriorFactor<gtsam::Pose3>(
        PoseKey(base_link->id(), t),
        gtsam::Pose3(gtsam::Rot3(), gtsam::Point3(0, 0.0, 0.5)),  // 0.5
        Isotropic::Sigma(6, 5e-5)));  // 6.2e-5 //5e-5
    objective_factors.add(gtsam::PriorFactor<Vector6>(
        TwistKey(base_link->id(), t), Vector6::Zero(),
        Isotropic::Sigma(6, 5e-5)));
  }

  // Add link boundary conditions to FG.
  for (auto &&link : spider.links()) {
    // Initial link pose, twists.
    objective_factors.add(gtsam::PriorFactor<gtsam::Pose3>(
        PoseKey(link->id(), 0), link->wTcom(), dynamics_model_6));
    objective_factors.add(gtsam::PriorFactor<Vector6>(
        TwistKey(link->id(), 0), Vector6::Zero(), dynamics_model_6));

    // Final link twists, accelerations.
    objective_factors.add(gtsam::PriorFactor<Vector6>(
        TwistKey(link->id(), t_f), Vector6::Zero(), objectives_model_6));
    objective_factors.add(gtsam::PriorFactor<Vector6>(
        TwistAccelKey(link->id(), t_f), Vector6::Zero(), objectives_model_6));
  }

  // Add joint boundary conditions to FG.
  for (auto &&joint : spider.joints()) {
    // Add priors to joint angles
    for (int t = 0; t <= t_f; t++) {
      if (joint->name().find("hip2") == 0)
        objective_factors.add(gtsam::PriorFactor<double>(
            JointAngleKey(joint->id(), t), 2.5, dynamics_model_1_2));
    }
    objective_factors.add(gtsam::PriorFactor<double>(
        JointVelKey(joint->id(), 0), 0.0, dynamics_model_1));
    objective_factors.add(gtsam::PriorFactor<double>(
        JointVelKey(joint->id(), t_f), 0.0, objectives_model_1));
    objective_factors.add(gtsam::PriorFactor<double>(
        JointAccelKey(joint->id(), t_f), 0.0, objectives_model_1));
  }

  // Add prior factor constraining all Phase keys to have duration of 1 / 240.
  for (int phase = 0; phase < spider_trajectory.numPhases(); phase++)
    objective_factors.add(gtsam::PriorFactor<double>(
        PhaseKey(phase), dt_des,
        gtsam::noiseModel::Isotropic::Sigma(1, 1e-30)));

  // Add min torque objectives.
  for (int t = 0; t <= t_f; t++) {
    for (auto &&joint : spider.joints())
      objective_factors.add(gtdynamics::MinTorqueFactor(
          TorqueKey(joint->id(), t),
          gtsam::noiseModel::Gaussian::Covariance(gtsam::I_1x1)));
  }
  graph.add(objective_factors);

  // TODO: Pass Trajectory here
  // Initialize solution.
  gtsam::Values init_vals;
  init_vals = gtdynamics::MultiPhaseZeroValuesTrajectory(
      robots, phase_durations, transition_graph_init, dt_des, gaussian_noise,
      phase_cps);

  // Optimize!
  gtsam::LevenbergMarquardtParams params;
  params.setVerbosityLM("SUMMARY");
  params.setlambdaInitial(1e0);
  params.setlambdaLowerBound(1e-7);
  params.setlambdaUpperBound(1e10);
  gtsam::LevenbergMarquardtOptimizer optimizer(graph, init_vals, params);
  auto results = optimizer.optimize();

  // Write results to traj file
  vector<string> jnames;
  for (auto &&joint : spider.joints()) jnames.push_back(joint->name());
  std::cout << jnames.size() << std::endl;
  string jnames_str = boost::algorithm::join(jnames, ",");
  std::ofstream traj_file;

  traj_file.open("forward_traj.csv");
  // angles, vels, accels, torques, time.
  traj_file << jnames_str << "," << jnames_str << "," << jnames_str << ","
            << jnames_str << ",t"
            << "\n";
  for (int phase = 0; phase < spider_trajectory.numPhases(); phase++)
    spider_trajectory.writePhaseToFile(traj_file, results, phase);

  // Write the last 4 phases to disk n times
  for (int i = 0; i < 10; i++) {
    for (int phase = 4; phase < phase_durations.size(); phase++)
      spider_trajectory.writePhaseToFile(traj_file, results, phase);
  }
  traj_file.close();
  return 0;
}