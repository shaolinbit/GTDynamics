/* ----------------------------------------------------------------------------
 * GTDynamics Copyright 2020, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * See LICENSE for the license information
 * -------------------------------------------------------------------------- */

/**
 * @file DynamicsGraphBuilder.h
 * @brief Builds a dynamics graph from a UniversalRobot object.
 * @author Yetong Zhang, Alejandro Escontrela
 */

#include "dynamics/DynamicsGraph.h"

#include <ContactDynamicsMomentFactor.h>
#include <ContactKinematicsAccelFactor.h>
#include <ContactKinematicsPoseFactor.h>
#include <ContactKinematicsTwistFactor.h>
#include <PoseFactor.h>
#include <TorqueFactor.h>
#include <TwistAccelFactor.h>
#include <TwistFactor.h>
#include <WrenchEquivalenceFactor.h>
#include <WrenchFactors.h>
#include <WrenchPlanarFactor.h>

#include <gtsam/base/numericalDerivative.h>
#include <gtsam/nonlinear/DoglegOptimizer.h>
#include <gtsam/nonlinear/ExpressionFactorGraph.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/expressions.h>
#include <gtsam/slam/PriorFactor.h>

#include <utils.h>

#include <iostream>
#include <vector>

using gtsam::Double_;
using gtsam::NonlinearFactorGraph;
using gtsam::Pose3;
using gtsam::PriorFactor;
using gtsam::Vector6;

namespace robot {

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::qFactors(
    const UniversalRobot &robot, const int t) const {
  NonlinearFactorGraph graph;
  for (auto &&link : robot.links()) {
    int i = link->getID();
    if (link->isFixed()) {
      graph.add(PriorFactor<Pose3>(PoseKey(i, t), link->getFixedPose(),
                                   gtsam::noiseModel::Constrained::All(6)));
    }
  }

  for (auto &&joint : robot.joints()) {
    const auto &link_1 = joint->parentLink().lock();
    const auto &link_2 = joint->childLink().lock();
    int i1 = link_1->getID();
    int i2 = link_2->getID();
    int j = joint->getID();
    // add pose factor
    graph.add(manipulator::PoseFactor(PoseKey(i1, t), PoseKey(i2, t),
                                      JointAngleKey(j, t), opt_.p_cost_model,
                                      joint->transformTo(link_2), joint->screwAxis(link_2)));
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::vFactors(
    const UniversalRobot &robot, const int t) const {
  NonlinearFactorGraph graph;
  for (auto &&link : robot.links()) {
    int i = link->getID();
    if (link->isFixed()) {
      graph.add(PriorFactor<Vector6>(TwistKey(i, t), Vector6::Zero(),
                                     gtsam::noiseModel::Constrained::All(6)));
    }
  }

  for (auto &&joint : robot.joints()) {
    const auto &link_1 = joint->parentLink().lock();
    const auto &link_2 = joint->childLink().lock();
    int i1 = link_1->getID();
    int i2 = link_2->getID();
    int j = joint->getID();
    // add twist factor
    graph.add(manipulator::TwistFactor(TwistKey(i1, t), TwistKey(i2, t),
                                       JointAngleKey(j, t), JointVelKey(j, t),
                                       opt_.v_cost_model, joint->transformTo(link_2),
                                       joint->screwAxis(link_2)));
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::aFactors(
    const UniversalRobot &robot, const int t) const {
  NonlinearFactorGraph graph;
  for (auto &&link : robot.links()) {
    int i = link->getID();
    if (link->isFixed()) {
      graph.add(PriorFactor<Vector6>(TwistAccelKey(i, t), Vector6::Zero(),
                                     gtsam::noiseModel::Constrained::All(6)));
    }
  }

  for (auto &&joint : robot.joints()) {
    const auto &link_1 = joint->parentLink().lock();
    const auto &link_2 = joint->childLink().lock();
    int i1 = link_1->getID();
    int i2 = link_2->getID();
    int j = joint->getID();
    // add twist acceleration factor
    graph.add(manipulator::TwistAccelFactor(
        TwistKey(i2, t), TwistAccelKey(i1, t), TwistAccelKey(i2, t),
        JointAngleKey(j, t), JointVelKey(j, t), JointAccelKey(j, t),
        opt_.a_cost_model, joint->transformTo(link_2), joint->screwAxis(link_2)));
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::dynamicsFactors(
    const UniversalRobot &robot, const int t,
    const boost::optional<gtsam::Vector3> &gravity,
    const boost::optional<gtsam::Vector3> &planar_axis) const {
  NonlinearFactorGraph graph;
  for (auto &&link : robot.links()) {
    int i = link->getID();
    if (!link->isFixed()) {
      const auto &connected_joints = link->getJoints();
      if (connected_joints.size() == 0) {
        graph.add(WrenchFactor0(TwistKey(i, t), TwistAccelKey(i, t),
                                PoseKey(i, t), opt_.f_cost_model,
                                link->inertiaMatrix(), gravity));
      } else if (connected_joints.size() == 1) {
        graph.add(WrenchFactor1(TwistKey(i, t), TwistAccelKey(i, t),
                                WrenchKey(i, connected_joints[0].lock()->getID(), t),
                                PoseKey(i, t), opt_.f_cost_model,
                                link->inertiaMatrix(), gravity));
      } else if (connected_joints.size() == 2) {
        graph.add(WrenchFactor2(TwistKey(i, t), TwistAccelKey(i, t),
                                WrenchKey(i, connected_joints[0].lock()->getID(), t),
                                WrenchKey(i, connected_joints[1].lock()->getID(), t),
                                PoseKey(i, t), opt_.f_cost_model,
                                link->inertiaMatrix(), gravity));
      } else if (connected_joints.size() == 3) {
        graph.add(WrenchFactor3(TwistKey(i, t), TwistAccelKey(i, t),
                                WrenchKey(i, connected_joints[0].lock()->getID(), t),
                                WrenchKey(i, connected_joints[1].lock()->getID(), t),
                                WrenchKey(i, connected_joints[2].lock()->getID(), t),
                                PoseKey(i, t), opt_.f_cost_model,
                                link->inertiaMatrix(), gravity));
      } else if (connected_joints.size() == 4) {
        graph.add(WrenchFactor4(TwistKey(i, t), TwistAccelKey(i, t),
                                WrenchKey(i, connected_joints[0].lock()->getID(), t),
                                WrenchKey(i, connected_joints[1].lock()->getID(), t),
                                WrenchKey(i, connected_joints[2].lock()->getID(), t),
                                WrenchKey(i, connected_joints[3].lock()->getID(), t),
                                PoseKey(i, t), opt_.f_cost_model,
                                link->inertiaMatrix(), gravity));
      } else {
        throw std::runtime_error("Wrench factor not defined");
      }
    }
  }

  for (auto &&joint : robot.joints()) {
    const auto &link_1 = joint->parentLink().lock();
    const auto &link_2 = joint->childLink().lock();
    int i1 = link_1->getID();
    int i2 = link_2->getID();
    int j = joint->getID();
    // add wrench equivalence factor
    // if (!link_1->isFixed() && !link_2->isFixed())
    // {
    graph.add(WrenchEquivalenceFactor(WrenchKey(i1, j, t), WrenchKey(i2, j, t),
                                      JointAngleKey(j, t), opt_.f_cost_model,
                                      joint->transformTo(link_2), joint->screwAxis(link_2)));
    // }

    // add torque factor
    graph.add(manipulator::TorqueFactor(WrenchKey(i2, j, t), TorqueKey(j, t),
                                        opt_.t_cost_model, joint->screwAxis(link_2)));

    // add planar wrench factor
    if (planar_axis) {
      graph.add(WrenchPlanarFactor(WrenchKey(i2, j, t),
                                   gtsam::noiseModel::Constrained::All(3),
                                   *planar_axis));
    }
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::dynamicsFactorGraph(
    const UniversalRobot &robot, const int t,
    const boost::optional<gtsam::Vector3> &gravity,
    const boost::optional<gtsam::Vector3> &planar_axis,
    const boost::optional<std::vector<uint>> &contacts) const {
  NonlinearFactorGraph graph;
  graph.add(qFactors(robot, t));
  graph.add(vFactors(robot, t));
  graph.add(aFactors(robot, t));
  graph.add(dynamicsFactors(robot, t, gravity, planar_axis));
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::trajectoryFG(
    const UniversalRobot &robot, const int num_steps, const double dt,
    const DynamicsGraphBuilder::CollocationScheme collocation,
    const boost::optional<gtsam::Vector3> &gravity,
    const boost::optional<gtsam::Vector3> &planar_axis) const {
  NonlinearFactorGraph graph;
  for (int t = 0; t < num_steps + 1; t++) {
    graph.add(dynamicsFactorGraph(robot, t, gravity, planar_axis));
    if (t < num_steps) {
      graph.add(collocationFactors(robot, t, dt, collocation));
    }
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::multiPhaseTrajectoryFG(
    const std::vector<UniversalRobot> &robots,
    const std::vector<int> &phase_steps,
    const std::vector<gtsam::NonlinearFactorGraph> &transition_graphs,
    const CollocationScheme collocation,
    const boost::optional<gtsam::Vector3> &gravity,
    const boost::optional<gtsam::Vector3> &planar_axis) const {
  NonlinearFactorGraph graph;
  int num_phases = robots.size();

  // add dynamcis for each step
  int t = 0;
  graph.add(dynamicsFactorGraph(robots[0], t, gravity, planar_axis));

  for (int phase = 0; phase < num_phases; phase++) {
    // in-phase
    for (int phase_step = 0; phase_step < phase_steps[phase] - 1;
         phase_step++) {
      graph.add(dynamicsFactorGraph(robots[phase], ++t, gravity, planar_axis));
    }
    // transition
    if (phase == num_phases - 1) {
      graph.add(dynamicsFactorGraph(robots[phase], ++t, gravity, planar_axis));
    } else {
      t++;
      graph.add(transition_graphs[phase]);
    }
  }

  // add collocation factors
  t = 0;
  for (int phase = 0; phase < num_phases; phase++) {
    for (int phase_step = 0; phase_step < phase_steps[phase]; phase_step++) {
      graph.add(
          multiPhaseCollocationFactors(robots[phase], t++, phase, collocation));
    }
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::collocationFactors(
    const UniversalRobot &robot, const int t, const double dt,
    const CollocationScheme collocation) const {
  gtsam::ExpressionFactorGraph graph;
  for (auto &&joint : robot.joints()) {
    int j = joint->getID();
    Double_ q0_expr = Double_(JointAngleKey(j, t));
    Double_ q1_expr = Double_(JointAngleKey(j, t + 1));
    Double_ v0_expr = Double_(JointVelKey(j, t));
    Double_ v1_expr = Double_(JointVelKey(j, t + 1));
    Double_ a0_expr = Double_(JointAccelKey(j, t));
    Double_ a1_expr = Double_(JointAccelKey(j, t + 1));
    switch (collocation) {
      case CollocationScheme::Euler:
        graph.addExpressionFactor(q0_expr + dt * v0_expr - q1_expr, 0.0,
                                  gtsam::noiseModel::Constrained::All(1));
        graph.addExpressionFactor(v0_expr + dt * a0_expr - v1_expr, 0.0,
                                  gtsam::noiseModel::Constrained::All(1));
        break;
      case CollocationScheme::Trapezoidal:
        graph.addExpressionFactor(
            q0_expr + 0.5 * dt * v0_expr + 0.5 * dt * v1_expr - q1_expr,
            0.0, gtsam::noiseModel::Constrained::All(1));
        graph.addExpressionFactor(
            v0_expr + 0.5 * dt * a0_expr + 0.5 * dt * a1_expr - v1_expr,
            0.0, gtsam::noiseModel::Constrained::All(1));
        break;
      default:
        throw std::runtime_error(
            "runge-kutta and hermite-simpson not implemented yet");
        break;
    }
  }
  NonlinearFactorGraph nonlinear_graph;
  nonlinear_graph.add(graph);
  return nonlinear_graph;
}

// the * operator for doubles in expression factor does not work well yet
double multDouble(const double &d1, const double &d2,
                  gtsam::OptionalJacobian<1, 1> H1,
                  gtsam::OptionalJacobian<1, 1> H2) {
  if (H1) *H1 = gtsam::I_1x1 * d2;
  if (H2) *H2 = gtsam::I_1x1 * d1;
  return d1 * d2;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::multiPhaseCollocationFactors(
    const UniversalRobot &robot, const int t, const int phase,
    const CollocationScheme collocation) const {
  gtsam::ExpressionFactorGraph graph;
  Double_ phase_expr = Double_(PhaseKey(phase));
  for (auto &&joint : robot.joints()) {
    int j = joint->getID();
    Double_ q0_expr = Double_(JointAngleKey(j, t));
    Double_ q1_expr = Double_(JointAngleKey(j, t + 1));
    Double_ v0_expr = Double_(JointVelKey(j, t));
    Double_ v1_expr = Double_(JointVelKey(j, t + 1));
    Double_ a0_expr = Double_(JointAccelKey(j, t));
    Double_ a1_expr = Double_(JointAccelKey(j, t + 1));

    if (collocation == CollocationScheme::Euler) {
      Double_ v0dt(multDouble, phase_expr, v0_expr);
      Double_ a0dt(multDouble, phase_expr, a0_expr);
      graph.addExpressionFactor(q0_expr + v0dt - q1_expr, 0.0,
                                gtsam::noiseModel::Constrained::All(1));
      graph.addExpressionFactor(v0_expr + a0dt - v1_expr, 0.0,
                                gtsam::noiseModel::Constrained::All(1));
    } else if (collocation == CollocationScheme::Trapezoidal) {
      Double_ v0dt(multDouble, phase_expr, v0_expr);
      Double_ a0dt(multDouble, phase_expr, a0_expr);
      Double_ v1dt(multDouble, phase_expr, v1_expr);
      Double_ a1dt(multDouble, phase_expr, a1_expr);
      graph.addExpressionFactor(q0_expr + 0.5 * v0dt + 0.5 * v1dt - q1_expr,
                                0.0,
                                gtsam::noiseModel::Constrained::All(1));
      graph.addExpressionFactor(v0_expr + 0.5 * a0dt + 0.5 * a1dt - v1_expr,
                                0.0,
                                gtsam::noiseModel::Constrained::All(1));
    } else {
      throw std::runtime_error(
          "runge-kutta and hermite-simpson not implemented yet");
    }
  }
  NonlinearFactorGraph nonlinear_graph;
  nonlinear_graph.add(graph);
  return nonlinear_graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::forwardDynamicsPriors(
    const UniversalRobot &robot, const int t, const gtsam::Vector &joint_angles,
    const gtsam::Vector &joint_vels, const gtsam::Vector &torques) const {
  gtsam::NonlinearFactorGraph graph;
  auto joints = robot.joints();
  for (int idx = 0; idx < robot.numJoints(); idx++) {
    auto joint = joints[idx];
    int j = joint->getID();
    graph.add(
        gtsam::PriorFactor<double>(JointAngleKey(j, t), joint_angles[idx],
                                   gtsam::noiseModel::Constrained::All(1)));
    graph.add(
        gtsam::PriorFactor<double>(JointVelKey(j, t), joint_vels[idx],
                                   gtsam::noiseModel::Constrained::All(1)));
    graph.add(gtsam::PriorFactor<double>(
        TorqueKey(j, t), torques[idx], gtsam::noiseModel::Constrained::All(1)));
  }
  return graph;
}

gtsam::NonlinearFactorGraph DynamicsGraphBuilder::trajectoryFDPriors(
    const UniversalRobot &robot, const int num_steps,
    const gtsam::Vector &joint_angles, const gtsam::Vector &joint_vels,
    const std::vector<gtsam::Vector> &torques_seq) const {
  gtsam::NonlinearFactorGraph graph;
  auto joints = robot.joints();
  for (int idx = 0; idx < robot.numJoints(); idx++) {
    int j = joints[idx]->getID();
    graph.add(
        gtsam::PriorFactor<double>(JointAngleKey(j, 0), joint_angles[idx],
                                   gtsam::noiseModel::Constrained::All(1)));
    graph.add(
        gtsam::PriorFactor<double>(JointVelKey(j, 0), joint_vels[idx],
                                   gtsam::noiseModel::Constrained::All(1)));
  }
  for (int t = 0; t <= num_steps; t++) {
    for (int idx = 0; idx < robot.numJoints(); idx++) {
      int j = joints[idx]->getID();
      graph.add(
          gtsam::PriorFactor<double>(TorqueKey(j, t), torques_seq[t][idx],
                                     gtsam::noiseModel::Constrained::All(1)));
    }
  }

  return graph;
}

gtsam::Vector DynamicsGraphBuilder::jointAccels(const UniversalRobot &robot,
                                                const gtsam::Values &result,
                                                const int t) {
  gtsam::Vector joint_accels = gtsam::Vector::Zero(robot.numJoints());
  auto joints = robot.joints();
  for (int idx = 0; idx < robot.numJoints(); idx++) {
    auto joint = joints[idx];
    int j = joint->getID();
    joint_accels[idx] = result.atDouble(JointAccelKey(j, t));
  }
  return joint_accels;
}

gtsam::Vector DynamicsGraphBuilder::jointVels(const UniversalRobot &robot,
                                              const gtsam::Values &result,
                                              const int t) {
  gtsam::Vector joint_vels = gtsam::Vector::Zero(robot.numJoints());
  auto joints = robot.joints();
  for (int idx = 0; idx < robot.numJoints(); idx++) {
    auto joint = joints[idx];
    int j = joint->getID();
    joint_vels[idx] = result.atDouble(JointVelKey(j, t));
  }
  return joint_vels;
}

gtsam::Vector DynamicsGraphBuilder::jointAngles(const UniversalRobot &robot,
                                                const gtsam::Values &result,
                                                const int t) {
  gtsam::Vector joint_angles = gtsam::Vector::Zero(robot.numJoints());
  auto joints = robot.joints();
  for (int idx = 0; idx < robot.numJoints(); idx++) {
    auto joint = joints[idx];
    int j = joint->getID();
    joint_angles[idx] = result.atDouble(JointAngleKey(j, t));
  }
  return joint_angles;
}

gtsam::Values DynamicsGraphBuilder::zeroValues(const UniversalRobot &robot,
                                               const int t) {
  gtsam::Vector zero_twists = gtsam::Vector6::Zero(),
                zero_accels = gtsam::Vector6::Zero(),
                zero_wrenches = gtsam::Vector6::Zero(),
                zero_torque = gtsam::Vector1::Zero(),
                zero_q = gtsam::Vector1::Zero(),
                zero_v = gtsam::Vector1::Zero(),
                zero_a = gtsam::Vector1::Zero();
  gtsam::Values zero_values;
  for (auto &link : robot.links()) {
    int i = link->getID();
    zero_values.insert(PoseKey(i, t), link->Twcom());
    zero_values.insert(TwistKey(i, t), zero_twists);
    zero_values.insert(TwistAccelKey(i, t), zero_accels);
  }
  for (auto &joint : robot.joints()) {
    int j = joint->getID();
    auto parent_link = joint->parentLink().lock();
    auto child_link = joint->childLink().lock();
    zero_values.insert(WrenchKey(parent_link->getID(), j, t), zero_wrenches);
    zero_values.insert(WrenchKey(child_link->getID(), j, t), zero_wrenches);
    zero_values.insert(TorqueKey(j, t), zero_torque[0]);
    zero_values.insert(JointAngleKey(j, t), zero_q[0]);
    zero_values.insert(JointVelKey(j, t), zero_v[0]);
    zero_values.insert(JointAccelKey(j, t), zero_a[0]);
  }
  return zero_values;
}

gtsam::Values DynamicsGraphBuilder::zeroValuesTrajectory(
    const UniversalRobot &robot, const int num_steps, const int num_phases) {
  gtsam::Values zero_values;
  for (int t = 0; t <= num_steps; t++) {
    zero_values.insert(zeroValues(robot, t));
  }
  if (num_phases > 0) {
    for (int phase = 0; phase <= num_phases; phase++) {
      zero_values.insert(PhaseKey(phase), 0.0);
    }
  }
  return zero_values;
}

gtsam::Values DynamicsGraphBuilder::optimize(
    const gtsam::NonlinearFactorGraph &graph, const gtsam::Values &init_values,
    OptimizerType optim_type) {
  if (optim_type == OptimizerType::GaussNewton) {
    gtsam::GaussNewtonOptimizer optimizer(graph, init_values);
    optimizer.optimize();
    return optimizer.values();
  } else if (optim_type == OptimizerType::LM) {
    gtsam::LevenbergMarquardtOptimizer optimizer(graph, init_values);
    optimizer.optimize();
    return optimizer.values();
  } else if (optim_type == OptimizerType::PDL) {
    gtsam::DoglegOptimizer optimizer(graph, init_values);
    optimizer.optimize();
    return optimizer.values();
  } else {
    throw std::runtime_error("optimizer not implemented yet");
  }
}

void print_key(const gtsam::Key &key) {
  auto symb = gtsam::LabeledSymbol(key);
  char ch = symb.chr();
  int index = symb.label();
  int t = symb.index();
  if (ch == 'F') {
    std::cout << ch << int(index / 16) << index % 16 << "_" << t;
  } else if (ch == 't') {
    if (index == 0) {  // phase key
      std::cout << "dt" << t;
    } else if (index == 1) {  // time key
      std::cout << "t" << t;
    } else {  // time to open valve
      std::cout << "ti" << t;
    }
  } else {
    std::cout << ch << index << "_" << t;
  }
  std::cout << "\t";
}

// print the factors of the factor graph
void DynamicsGraphBuilder::print_values(const gtsam::Values &values) {
  for (auto &key : values.keys()) {
    print_key(key);
    std::cout << "\n";
    values.at(key).print();
    std::cout << "\n";
  }
}

// print the factors of the factor graph
void DynamicsGraphBuilder::print_graph(
    const gtsam::NonlinearFactorGraph &graph) {
  for (auto &factor : graph) {
    for (auto &key : factor->keys()) {
      print_key(key);
    }
    std::cout << "\n";
  }
}

}  // namespace robot