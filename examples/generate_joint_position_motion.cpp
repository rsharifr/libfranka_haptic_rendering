// Copyright (c) 2017 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#include <cmath>
#include <iostream>

#include <franka/active_control.h>
#include <franka/active_motion_generator.h>
#include <franka/exception.h>
#include <franka/robot.h>
#include "examples_common.h"
/**
 * @example generate_joint_position_motion.cpp
 * An example showing how to generate a joint position motion.
 *
 * @warning Before executing this example, make sure there is enough space in front of the robot.
 */

int main(int argc, char** argv) {
  bool use_external_control_loop = false;
  if (argc != 2 && argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <robot-hostname> optional: <use_external_control_loop>"
              << std::endl;
    return -1;
  } else if (argc == 3) {
    use_external_control_loop = !std::strcmp(argv[2], "true");
  }

  try {
    franka::Robot robot(argv[1]);
    setDefaultBehavior(robot);

    // First move the robot to a suitable joint configuration
    std::array<double, 7> q_goal = {{0, -M_PI_4, 0, -3 * M_PI_4, 0, M_PI_2, M_PI_4}};
    MotionGenerator motion_generator(0.5, q_goal);
    std::cout << "WARNING: This example will move the robot! "
              << "Please make sure to have the user stop button at hand!" << std::endl
              << "Press Enter to continue..." << std::endl;
    std::cin.ignore();
    robot.control(motion_generator);
    std::cout << "Finished moving to initial joint configuration." << std::endl;

    // Set additional parameters always before the control loop, NEVER in the control loop!
    // Set collision behavior.
    robot.setCollisionBehavior(
        {{20.0, 20.0, 18.0, 18.0, 16.0, 14.0, 12.0}}, {{20.0, 20.0, 18.0, 18.0, 16.0, 14.0, 12.0}},
        {{20.0, 20.0, 18.0, 18.0, 16.0, 14.0, 12.0}}, {{20.0, 20.0, 18.0, 18.0, 16.0, 14.0, 12.0}},
        {{20.0, 20.0, 20.0, 25.0, 25.0, 25.0}}, {{20.0, 20.0, 20.0, 25.0, 25.0, 25.0}},
        {{20.0, 20.0, 20.0, 25.0, 25.0, 25.0}}, {{20.0, 20.0, 20.0, 25.0, 25.0, 25.0}});

    std::array<double, 7> initial_position;
    double time = 0.0;
    auto control_callback = [&initial_position, &time](
                                const franka::RobotState& robot_state,
                                franka::Duration period) -> franka::JointPositions {
      time += period.toSec();

      if (time == 0.0) {
        initial_position = robot_state.q_d;
      }

      double delta_angle = M_PI / 8.0 * (1 - std::cos(M_PI / 2.5 * time));

      franka::JointPositions output = {{initial_position[0], initial_position[1],
                                        initial_position[2], initial_position[3] + delta_angle,
                                        initial_position[4] + delta_angle, initial_position[5],
                                        initial_position[6] + delta_angle}};

      if (time >= 5.0) {
        std::cout << std::endl << "Finished motion, shutting down example" << std::endl;
        return franka::MotionFinished(output);
      }
      return output;
    };

    if (use_external_control_loop) {
      bool motion_finished = false;
      auto active_control = robot.startJointPositionControl(
          research_interface::robot::Move::ControllerMode::kJointImpedance);
      while (!motion_finished) {
        auto [robot_state, duration] = active_control->readOnce();
        auto joint_positions = control_callback(robot_state, duration);
        motion_finished = joint_positions.motion_finished;
        active_control->writeOnce(joint_positions);
      }
    } else {
      robot.control(control_callback);
    }

  } catch (const franka::Exception& e) {
    std::cout << e.what() << std::endl;
    return -1;
  }

  return 0;
}
