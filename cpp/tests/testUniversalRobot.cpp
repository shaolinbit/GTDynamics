/**
 * @file testUniversalRobot.cpp
 * @brief test UniversalRobot instance methods and integration test with various URDF configurations
 * @Author Frank Dellaert, Mandy Xie, and Alejandro Escontrela
 */

#include <UniversalRobot.h>
#include <utils.h>

#include <gtsam/base/Testable.h>
#include <gtsam/base/TestableAssertions.h>
#include <gtsam/linear/VectorValues.h>

#include <CppUnitLite/TestHarness.h>

using namespace std;
using namespace robot;
using namespace gtsam;

// Constructs RobotLink and RobotJoint objects from an input urdf:ModelInterface
// pointer and checks that constructed values have correct parents, children,
// and transforms.
TEST(UniversalRobot, test_extract_structure_from_urdf) {
  // Obtain urdf::ModelInterfaceSharedPtr from sample urdf file.
  std::string simple_urdf_str = manipulator::load_file_into_string("../../../urdfs/test/simple_urdf.urdf");
  auto simple_urdf = manipulator::get_urdf(simple_urdf_str);

  // Obtain RobotLink and JointBody objects from ModelInterfaceSharedPtr.
  std::vector<robot::RobotJointParams> joint_params;
  robot::RobotJointParams j1_params;
  j1_params.name = "j1";
  j1_params.jointEffortType = robot::RobotJoint::JointEffortType::Actuated;
  joint_params.push_back(j1_params);

  RobotRobotJointPair urdf_bodies_and_joints = extract_structure_from_urdf(simple_urdf, 
    boost::optional<std::vector<robot::RobotJointParams>>(joint_params));
  std::vector<robot::RobotLinkSharedPtr> linkBodies = urdf_bodies_and_joints.first;
  std::vector<robot::RobotJointSharedPtr> RobotJoints = urdf_bodies_and_joints.second;

  EXPECT(assert_equal(2, linkBodies.size()));
  EXPECT(assert_equal(1, RobotJoints.size()));

  // Ensure that link l1 has link l2 listed as a child link and j1 listed as
  // a child joint. Ensure that link l2 has link l1 lsited as a parent link
  // and j1 listed as a parent joint.
  auto l1_it = std::find_if(linkBodies.begin(), linkBodies.end(), [=] (const robot::RobotLinkSharedPtr & link) {
    return (link->name() == "l1");
  });
  auto l2_it = std::find_if(linkBodies.begin(), linkBodies.end(), [=] (const robot::RobotLinkSharedPtr & link) {
    return (link->name() == "l2");
  });

  EXPECT(assert_equal(0, l1_it == linkBodies.end()));
  EXPECT(assert_equal(0, (*l1_it)->getParentLinks().size()));
  EXPECT(assert_equal(0, (*l1_it)->getParentJoints().size()));
  EXPECT(assert_equal(1, (*l1_it)->getChildLinks().size()));
  EXPECT(assert_equal(1, (*l1_it)->getChildJoints().size()));
  EXPECT(assert_equal(1, (*l1_it)->getJoints().size()));

  EXPECT(assert_equal(0, l2_it == linkBodies.end()));
  EXPECT(assert_equal(1, (*l2_it)->getParentLinks().size()));
  EXPECT(assert_equal(1, (*l2_it)->getParentJoints().size()));
  EXPECT(assert_equal(1, (*l1_it)->getChildLinks().size()));
  EXPECT(assert_equal(0, (*l2_it)->getChildJoints().size()));
  EXPECT(assert_equal(1, (*l2_it)->getJoints().size()));

  robot::RobotLinkWeakPtr l2_weak = (*l1_it)->getChildLinks()[0];
  EXPECT(assert_equal("l2", l2_weak.lock()->name()));
  EXPECT(assert_equal("l1", ((*l2_it)->getParentLinks()[0])->name()));

  auto j1_it = std::find_if(RobotJoints.begin(), RobotJoints.end(), [=] (const robot::RobotJointSharedPtr & joint) {
    return (joint->name() == "j1");
  });
  EXPECT(assert_equal(0, j1_it == RobotJoints.end()));
  EXPECT(assert_equal("j1", (*j1_it)->name()));
  EXPECT(assert_equal(0, j1_params.jointEffortType != (*j1_it)->jointEffortType()));
}

// Constructs RobotLink and RobotJoint objects from an input urdf:ModelInterface
// pointer with a loop and checks that constructed values have correct parents,
// children, and transforms.
TEST(UniversalRobot, test_extract_structure_with_loop_from_urdf) {
  // Obtain urdf::ModelInterfaceSharedPtr from sample urdf file.
  std::string four_bar_urdf_str = manipulator::load_file_into_string(
    "../../../urdfs/test/four_bar_linkage.urdf");
  auto four_bar_urdf = manipulator::get_urdf(four_bar_urdf_str);

  // Obtain RobotLink and JointBody objects from ModelInterfaceSharedPtr.
  RobotRobotJointPair urdf_bodies_and_joints = extract_structure_from_urdf(four_bar_urdf);
  std::vector<robot::RobotLinkSharedPtr> linkBodies = urdf_bodies_and_joints.first;
  std::vector<robot::RobotJointSharedPtr> RobotJoints = urdf_bodies_and_joints.second;

  EXPECT(assert_equal(5, linkBodies.size()));
  EXPECT(assert_equal(5, RobotJoints.size()));

  // Grab all links.
  auto l0 = *std::find_if(linkBodies.begin(), linkBodies.end(), 
    [=] (const robot::RobotLinkSharedPtr & link) { return (link->name() == "l0"); });
  auto l1 = *std::find_if(linkBodies.begin(), linkBodies.end(),
    [=] (const robot::RobotLinkSharedPtr & link) { return (link->name() == "l1"); });
  auto l2 = *std::find_if(linkBodies.begin(), linkBodies.end(),
    [=] (const robot::RobotLinkSharedPtr & link) { return (link->name() == "l2"); });
  auto l3 = *std::find_if(linkBodies.begin(), linkBodies.end(),
    [=] (const robot::RobotLinkSharedPtr & link) { return (link->name() == "l3"); });
  auto l4 = *std::find_if(linkBodies.begin(), linkBodies.end(),
    [=] (const robot::RobotLinkSharedPtr & link) { return (link->name() == "l4"); });

  // Check link l1's parents and children.
  EXPECT(assert_equal(2, l1->getParentLinks().size()));
  EXPECT(assert_equal(2, l1->getParentJoints().size()));
  EXPECT(assert_equal(1, l1->getChildLinks().size()));
  EXPECT(assert_equal(1, l1->getChildJoints().size()));
  EXPECT(assert_equal(3, l1->getJoints().size()));

  // Check that l1's parent links are l0 and l4, its parent joints are j0 and j4, child
  // link is l2, and child joint is j1.
  EXPECT(assert_equal(
    1,
    ("l0" == l1->getParentLinks()[0]->name()) || ("l0" == l1->getParentLinks()[1]->name())
  ));
  EXPECT(assert_equal(
    1,
    ("l4" == l1->getParentLinks()[0]->name()) || ("l4" == l1->getParentLinks()[1]->name())
  ));
  EXPECT(assert_equal(
    1,
    "l2" == l1->getChildLinks()[0].lock()->name()
  ));
  EXPECT(assert_equal(
    1,
    ("j0" == l1->getParentJoints()[0]->name()) || ("j0" == l1->getParentJoints()[1]->name())
  ));
  EXPECT(assert_equal(
    1,
    ("j4" == l1->getParentJoints()[0]->name()) || ("j4" == l1->getParentJoints()[1]->name())
  ));
  EXPECT(assert_equal(
    1,
    "j1" == l1->getChildJoints()[0].lock()->name()
  ));


  // Check link l2's parents and children.
  EXPECT(assert_equal(1, l2->getParentLinks().size()));
  EXPECT(assert_equal(1, l2->getParentJoints().size()));
  EXPECT(assert_equal(1, l2->getChildLinks().size()));
  EXPECT(assert_equal(1, l2->getChildJoints().size()));
  EXPECT(assert_equal(2, l2->getJoints().size()));

  // Check that l2's parent link is l1, its parent joint is j1, child
  // link is l3, and child joint is j2.
  EXPECT(assert_equal(
    1,
    "l1" == l2->getParentLinks()[0]->name()
  ));
  
  EXPECT(assert_equal(
    1,
    "l3" == l2->getChildLinks()[0].lock()->name()
  ));
  EXPECT(assert_equal(
    1,
    "j1" == l2->getParentJoints()[0]->name()
  ));
  EXPECT(assert_equal(
    1,
    "j2" == l2->getChildJoints()[0].lock()->name()
  ));

  // Check link l3's parents and children.
  EXPECT(assert_equal(1, l3->getParentLinks().size()));
  EXPECT(assert_equal(1, l3->getParentJoints().size()));
  EXPECT(assert_equal(1, l3->getChildLinks().size()));
  EXPECT(assert_equal(1, l3->getChildJoints().size()));
  EXPECT(assert_equal(2, l3->getJoints().size()));

  // Check that l3's parent link is l2, its parent joint is j2, child
  // link is l4, and child joint is j3.
  EXPECT(assert_equal(
    1,
    "l2" == l3->getParentLinks()[0]->name()
  ));
  
  EXPECT(assert_equal(
    1,
    "l4" == l3->getChildLinks()[0].lock()->name()
  ));
  EXPECT(assert_equal(
    1,
    "j2" == l3->getParentJoints()[0]->name()
  ));
  EXPECT(assert_equal(
    1,
    "j3" == l3->getChildJoints()[0].lock()->name()
  ));

  // Check link l4's parents and children.
  EXPECT(assert_equal(1, l4->getParentLinks().size()));
  EXPECT(assert_equal(1, l4->getParentJoints().size()));
  EXPECT(assert_equal(1, l4->getChildLinks().size()));
  EXPECT(assert_equal(1, l4->getChildJoints().size()));
  EXPECT(assert_equal(2, l4->getJoints().size()));

  // Check that l4's parent link is l3, its parent joint is j3, child
  // link is l1, and child joint is j4.
  EXPECT(assert_equal(
    1,
    "l3" == l4->getParentLinks()[0]->name()
  ));
  
  EXPECT(assert_equal(
    1,
    "l1" == l4->getChildLinks()[0].lock()->name()
  ));
  EXPECT(assert_equal(
    1,
    "j3" == l4->getParentJoints()[0]->name()
  ));
  EXPECT(assert_equal(
    1,
    "j4" == l4->getChildJoints()[0].lock()->name()
  ));
}

// Initialize a UniversalRobot with "urdfs/test/simple_urdf.urdf" and make sure
// that all transforms, link/joint properties, etc. are correct.
TEST(UniversalRobot, instantiate_from_urdf) {
    // Load urdf file into urdf::ModelInterfacePtr
    string simple_urdf_str = manipulator::load_file_into_string("../../../urdfs/test/simple_urdf.urdf");
    auto simple_urdf = manipulator::get_urdf(simple_urdf_str);

    RobotRobotJointPair urdf_bodies_and_joints = extract_structure_from_urdf(simple_urdf);

    // Initialize UniversalRobot instance using RobotLink and RobotJoint instances.
    UniversalRobot simple_robot = UniversalRobot(urdf_bodies_and_joints);

    // Check that number of links and joints in the UniversalRobot instance is correct.
    EXPECT(assert_equal(2, simple_robot.links().size()));
    EXPECT(assert_equal(1, simple_robot.joints().size()));

    // This robot has a single screw axis (at joint j1).
    map<string, Vector6> screw_axes = simple_robot.screwAxes();
    EXPECT(assert_equal(1, screw_axes.size()));

    EXPECT(assert_equal(
      "l1",
      simple_robot.getLinkByName("l1")->name()
    ));
    EXPECT(assert_equal(
      "l2",
      simple_robot.getLinkByName("l2")->name()
    ));
    EXPECT(assert_equal(
      "j1",
      simple_robot.getJointByName("j1")->name()
    ));

    // Test joint limit utility methods.
    map<string, double> joint_lower_limits = simple_robot.jointLowerLimits();
    map<string, double> joint_upper_limits = simple_robot.jointUpperLimits();
    map<string, double> joint_limit_thresholds = simple_robot.jointLimitThresholds();

    EXPECT(assert_equal(1, joint_lower_limits.size()));
    EXPECT(assert_equal(1, joint_upper_limits.size()));
    EXPECT(assert_equal(1, joint_limit_thresholds.size()));

    EXPECT(assert_equal(-1.57, joint_lower_limits["j1"]));
    EXPECT(assert_equal(1.57, joint_upper_limits["j1"]));
    EXPECT(assert_equal(0.0, joint_limit_thresholds["j1"]));

    // Check link transforms at rest.
    map<string, map<string, Pose3>> rest_link_transforms = simple_robot.linkTransforms();
    EXPECT(assert_equal(2,rest_link_transforms.size()));
    EXPECT(assert_equal(0,rest_link_transforms["l1"].size()));
    EXPECT(assert_equal(1,rest_link_transforms["l2"].size()));

    EXPECT(assert_equal(
      Pose3(Rot3(I_3x3), Point3(0, 0, 2)),
      rest_link_transforms["l2"]["l1"]
    ));

    // Check link transforms with joint angle.
    map<string, double> joint_name_to_angle = { {"j1", M_PI / 4} };
    map<string, map<string, Pose3>> link_transforms = simple_robot.linkTransforms(
      joint_name_to_angle);

    EXPECT(assert_equal(
      Pose3(Rot3::Rx(M_PI / 4), Point3(0, 0, 2)),
      link_transforms["l2"]["l1"]
    ));

    // Check cTpCOM: transform from parent link COM frame to child link COM
    // frame in parent link COM frame.
    Pose3 l2Tl1COM_rest = simple_robot.cTpCOM("j1");

    EXPECT(assert_equal(
      Pose3(Rot3(), Point3(0, 0, -2)),
      l2Tl1COM_rest
    ));

    // Check cTpCOM with joint angle value.
    Pose3 l2Tl1COM = simple_robot.cTpCOM("j1", -M_PI / 4);

    EXPECT(assert_equal(
      Pose3(Rot3::Rx(M_PI / 4), Point3(0, 0.7071, -1.7071)),
      l2Tl1COM, 1e-4
    ));

    // Check cTpCOM map at rest.
    map<string, map<string, Pose3>> rest_cTp_COMs = simple_robot.cTpCOMs();

    EXPECT(assert_equal(1, rest_cTp_COMs.size()));
    EXPECT(assert_equal(1, rest_cTp_COMs["l2"].size()));
    EXPECT(assert_equal(l2Tl1COM_rest, rest_cTp_COMs["l2"]["l1"]));

    // Check cTpCOM map with joint angle value.
    map<string, double> joint_name_to_angle_2 = { {"j1", -M_PI / 4} };
    map<string, map<string, Pose3>> cTp_COMs = simple_robot.cTpCOMs(joint_name_to_angle_2);

    EXPECT(assert_equal(1, cTp_COMs.size()));
    EXPECT(assert_equal(1, cTp_COMs["l2"].size()));
    EXPECT(assert_equal(l2Tl1COM, cTp_COMs["l2"]["l1"]));

    // Test jTi transforms at rest.
    // map<string, map<string, Pose3>> rest_jTi_transforms = simple_robot.jTiTransforms();
    // EXPECT(assert_equal(1, rest_jTi_transforms.size()));
    // EXPECT(assert_equal(1, rest_jTi_transforms["l2"].size()));
    // EXPECT(assert_equal(
    //   Pose3(Rot3(), Point3(0, 0, -2)),
    //   rest_jTi_transforms["l2"]["l1"]
    // ));

    // // Check jTi transforms with joint angle value.
    // map<string, map<string, Pose3>> jTi_transforms = simple_robot.jTiTransforms(
    //   joint_name_to_angle_2);
    // EXPECT(assert_equal(1, jTi_transforms.size()));
    // EXPECT(assert_equal(1, jTi_transforms["l2"].size()));
    // EXPECT(assert_equal(
    //   Pose3(Rot3::Rx(M_PI / 4), Point3(0, 0.7071, -1 - 0.7071)),
    //   jTi_transforms["l2"]["l1"], 1e-3
    // ));

}

int main() {
  TestResult tr;
  return TestRegistry::runAllTests(tr);
}