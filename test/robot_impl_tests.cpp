#include <gtest/gtest.h>

#include <cstring>

#include <robot_impl.h>

#include "helpers.h"
#include "mock_server.h"

using namespace std::chrono_literals;
using namespace research_interface::robot;

using franka::ControlException;
using franka::NetworkException;

class Robot : public ::franka::Robot {
 public:
  using ::franka::Robot::Impl;
};

TEST(RobotImpl, CanReceiveRobotState) {
  RobotState sent_robot_state;
  randomRobotState(sent_robot_state);

  MockServer server;

  Robot::Impl robot("127.0.0.1");

  server.onSendRobotState([&]() { return sent_robot_state; }).spinOnce();

  auto received_robot_state = robot.update();
  testRobotStatesAreEqual(sent_robot_state, received_robot_state);
}

TEST(RobotImpl, ThrowsTimeoutIfNoRobotStateArrives) {
  RobotState sent_robot_state;
  randomRobotState(sent_robot_state);

  MockServer server;

  Robot::Impl robot("127.0.0.1", kCommandPort, 200ms);

  EXPECT_THROW(robot.update(), NetworkException);
}

TEST(RobotImpl, StopsIfControlConnectionClosed) {
  std::unique_ptr<Robot::Impl> robot;
  {
    MockServer server;

    robot.reset(new Robot::Impl("127.0.0.1", kCommandPort, 200ms));

    server.sendEmptyRobotState().spinOnce();

    testRobotStateIsZero(robot->update());
  }

  EXPECT_THROW(robot->update(), NetworkException);
}

TEST(RobotImpl, CanStartMotion) {
  MockServer server;
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};

  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kJointPosition;
        robot_state.controller_mode = ControllerMode::kJointPosition;
      })
      .spinOnce()
      .waitForCommand<Move>([=](const Move::Request& request) {
        EXPECT_EQ(Move::MotionGeneratorMode::kJointPosition, request.motion_generator_mode);
        EXPECT_EQ(Move::ControllerMode::kJointPosition, request.controller_mode);
        EXPECT_EQ(maximum_path_deviation, request.maximum_path_deviation);
        EXPECT_EQ(maximum_goal_pose_deviation, request.maximum_goal_pose_deviation);
        return Move::Response(Move::Status::kMotionStarted);
      })
      .spinOnce();

  EXPECT_NO_THROW(robot.startMotion(Move::ControllerMode::kJointPosition,
                                    Move::MotionGeneratorMode::kJointPosition,
                                    maximum_path_deviation, maximum_goal_pose_deviation));
  EXPECT_TRUE(robot.motionGeneratorRunning());
  EXPECT_FALSE(robot.controllerRunning());

  // Test exceptions if wrong update() overload is called
  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kJointPosition;
        robot_state.controller_mode = ControllerMode::kJointPosition;
      })
      .spinOnce();
  EXPECT_NO_THROW(robot.update(nullptr, nullptr));

  ControllerCommand control_command{};
  MotionGeneratorCommand motion_command{};
  EXPECT_THROW(robot.update(nullptr, &control_command), ControlException);
  EXPECT_THROW(robot.update(&motion_command, &control_command), ControlException);

  server.sendEmptyRobotState()
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();
  EXPECT_NO_THROW(robot.update(&motion_command, nullptr));
}

TEST(RobotImpl, CanStartMotionWithController) {
  MockServer server;
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};

  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .waitForCommand<Move>([=](const Move::Request& request) {
        EXPECT_EQ(Move::MotionGeneratorMode::kCartesianPosition, request.motion_generator_mode);
        EXPECT_EQ(Move::ControllerMode::kExternalController, request.controller_mode);
        EXPECT_EQ(maximum_path_deviation, request.maximum_path_deviation);
        EXPECT_EQ(maximum_goal_pose_deviation, request.maximum_goal_pose_deviation);
        return Move::Response(Move::Status::kMotionStarted);
      })
      .spinOnce();

  EXPECT_NO_THROW(robot.startMotion(Move::ControllerMode::kExternalController,
                                    Move::MotionGeneratorMode::kCartesianPosition,
                                    maximum_path_deviation, maximum_goal_pose_deviation));
  EXPECT_TRUE(robot.motionGeneratorRunning());
  EXPECT_TRUE(robot.controllerRunning());

  // Test exceptions if wrong update() overload is called
  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce();
  EXPECT_NO_THROW(robot.update(nullptr, nullptr));

  ControllerCommand control_command{};
  MotionGeneratorCommand motion_command{};
  EXPECT_THROW(robot.update(nullptr, &control_command), ControlException);
  EXPECT_THROW(robot.update(&motion_command, nullptr), ControlException);
}

TEST(RobotImpl, CanStartController) {
  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](research_interface::robot::RobotState& robot_state) {
        robot_state.controller_mode =
            research_interface::robot::ControllerMode::kExternalController;
      })
      .spinOnce()
      .waitForCommand<SetControllerMode>([](const SetControllerMode::Request& request) {
        EXPECT_EQ(SetControllerMode::ControllerMode::kExternalController, request.mode);
        return SetControllerMode::Response(SetControllerMode::Status::kSuccess);
      })
      .spinOnce();

  EXPECT_NO_THROW(robot.startController());

  EXPECT_FALSE(robot.motionGeneratorRunning());
  EXPECT_TRUE(robot.controllerRunning());

  // Test exceptions if wrong update() overload is called
  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce();
  EXPECT_NO_THROW(robot.update(nullptr, nullptr));

  ControllerCommand control_command{};
  MotionGeneratorCommand motion_command{};
  EXPECT_THROW(robot.update(&motion_command, nullptr), ControlException);
  EXPECT_THROW(robot.update(&motion_command, &control_command), ControlException);

  server.sendEmptyRobotState()
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();
  EXPECT_NO_THROW(robot.update(nullptr, &control_command));
}

TEST(RobotImpl, CanNotStartMultipleMotions) {
  MockServer server;
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};

  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kJointVelocity;
        robot_state.controller_mode = ControllerMode::kJointImpedance;
      })
      .spinOnce()
      .waitForCommand<Move>(
          [](const Move::Request&) { return Move::Response(Move::Status::kMotionStarted); })
      .spinOnce();

  EXPECT_NO_THROW(robot.startMotion(Move::ControllerMode::kJointImpedance,
                                    Move::MotionGeneratorMode::kJointVelocity,
                                    maximum_path_deviation, maximum_goal_pose_deviation));
  EXPECT_THROW(robot.startMotion(Move::ControllerMode::kJointPosition,
                                 Move::MotionGeneratorMode::kJointPosition, maximum_path_deviation,
                                 maximum_goal_pose_deviation),
               ControlException);
}

TEST(RobotImpl, CanNotStartMultipleControllers) {
  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .waitForCommand<SetControllerMode>([](const SetControllerMode::Request&) {
        return SetControllerMode::Response(SetControllerMode::Status::kSuccess);
      })
      .spinOnce();

  robot.startController();
  EXPECT_THROW(robot.startController(), ControlException);
}

TEST(RobotImpl, CanSendMotionGeneratorCommand) {
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};
  const uint32_t message_id = 682;

  RobotCommand sent_command;
  randomRobotCommand(sent_command);
  sent_command.motion.motion_generation_finished = false;

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([=](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kJointVelocity;
        robot_state.controller_mode = ControllerMode::kJointImpedance;
        robot_state.message_id = message_id;
      })
      .spinOnce()
      .waitForCommand<Move>(
          [](const Move::Request&) { return Move::Response(Move::Status::kMotionStarted); })
      .spinOnce();

  robot.startMotion(Move::ControllerMode::kJointImpedance,
                    Move::MotionGeneratorMode::kJointVelocity, maximum_path_deviation,
                    maximum_goal_pose_deviation);

  server
      .onSendRobotState([=](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kJointVelocity;
        robot_state.controller_mode = ControllerMode::kJointImpedance;
        robot_state.message_id = message_id + 1;
      })
      .spinOnce()
      .onReceiveRobotCommand([=](const RobotCommand& command) {
        EXPECT_EQ(message_id, command.message_id);
        testMotionGeneratorCommandsAreEqual(sent_command.motion, command.motion);
      })
      .spinOnce();

  EXPECT_NO_THROW(robot.update(&sent_command.motion));
}

TEST(RobotImpl, CanSendControllerCommand) {
  const uint32_t message_id = 684;

  RobotCommand sent_command;
  randomRobotCommand(sent_command);

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([=](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kExternalController;
        robot_state.message_id = message_id;
      })
      .spinOnce()
      .waitForCommand<SetControllerMode>([](const SetControllerMode::Request&) {
        return SetControllerMode::Response(SetControllerMode::Status::kSuccess);
      })
      .spinOnce();

  robot.startController();

  server
      .onSendRobotState([=](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kExternalController;
        robot_state.message_id = message_id + 1;
      })
      .spinOnce()
      .onReceiveRobotCommand([=](const RobotCommand& command) {
        EXPECT_EQ(message_id, command.message_id);
        testControllerCommandsAreEqual(sent_command.control, command.control);
      })
      .spinOnce();

  EXPECT_NO_THROW(robot.update(nullptr, &sent_command.control));
}

TEST(RobotImpl, CanSendMotionGeneratorAndControlCommand) {
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};
  const uint32_t message_id = 687;

  RobotCommand sent_command;
  randomRobotCommand(sent_command);
  sent_command.motion.motion_generation_finished = false;

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([=](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kExternalController;
        robot_state.message_id = message_id;
      })
      .spinOnce()
      .waitForCommand<Move>(
          [](const Move::Request&) { return Move::Response(Move::Status::kMotionStarted); })
      .spinOnce();

  robot.startMotion(Move::ControllerMode::kExternalController,
                    Move::MotionGeneratorMode::kCartesianPosition, maximum_path_deviation,
                    maximum_goal_pose_deviation);

  server
      .onSendRobotState([=](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kExternalController;
        robot_state.message_id = message_id + 1;
      })
      .spinOnce()
      .onReceiveRobotCommand([=](const RobotCommand& command) {
        EXPECT_EQ(message_id, command.message_id);
        testMotionGeneratorCommandsAreEqual(sent_command.motion, command.motion);
        testControllerCommandsAreEqual(sent_command.control, command.control);
      })
      .spinOnce();

  EXPECT_NO_THROW(robot.update(&sent_command.motion, &sent_command.control));
}

TEST(RobotImpl, CanReceiveMotionGenerationError) {
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};

  RobotCommand sent_command;
  randomRobotCommand(sent_command);
  sent_command.motion.motion_generation_finished = false;

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianVelocity;
        robot_state.controller_mode = ControllerMode::kMotorPD;
      })
      .spinOnce()
      .waitForCommand<Move>(
          [](const Move::Request&) { return Move::Response(Move::Status::kMotionStarted); })
      .spinOnce();

  robot.startMotion(Move::ControllerMode::kMotorPD, Move::MotionGeneratorMode::kCartesianVelocity,
                    maximum_path_deviation, maximum_goal_pose_deviation);
  EXPECT_TRUE(robot.motionGeneratorRunning());

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kMotorPD;
      })
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();

  robot.update(&sent_command.motion);

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kIdle;
        robot_state.controller_mode = ControllerMode::kCartesianImpedance;
      })
      .spinOnce()
      .sendResponse<Move::Response>([]() { return Move::Response(Move::Status::kRejected); })
      .spinOnce();

  EXPECT_THROW(robot.update(&sent_command.motion), ControlException);
  EXPECT_FALSE(robot.motionGeneratorRunning());
}

TEST(RobotImpl, CanStopMotion) {
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};

  RobotCommand sent_command;
  randomRobotCommand(sent_command);
  sent_command.motion.motion_generation_finished = false;

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianVelocity;
        robot_state.controller_mode = ControllerMode::kMotorPD;
      })
      .spinOnce()
      .waitForCommand<Move>(
          [](const Move::Request&) { return Move::Response(Move::Status::kMotionStarted); })
      .spinOnce();

  robot.startMotion(Move::ControllerMode::kMotorPD, Move::MotionGeneratorMode::kCartesianVelocity,
                    maximum_path_deviation, maximum_goal_pose_deviation);
  EXPECT_TRUE(robot.motionGeneratorRunning());

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kMotorPD;
      })
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();

  robot.update(&sent_command.motion);

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kIdle;
        robot_state.controller_mode = ControllerMode::kMotorPD;
      })
      .sendResponse<Move::Response>([]() { return Move::Response(Move::Status::kSuccess); })
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand& command) {
        EXPECT_TRUE(command.motion.motion_generation_finished);
      })
      .spinOnce();

  robot.stopMotion();
  EXPECT_FALSE(robot.motionGeneratorRunning());
}

TEST(RobotImpl, CanStopMotionWithController) {
  Move::Deviation maximum_path_deviation{0, 1, 2};
  Move::Deviation maximum_goal_pose_deviation{3, 4, 5};

  RobotCommand sent_command;
  randomRobotCommand(sent_command);
  sent_command.motion.motion_generation_finished = false;

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianVelocity;
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .waitForCommand<Move>(
          [](const Move::Request&) { return Move::Response(Move::Status::kMotionStarted); })
      .spinOnce();

  robot.startMotion(Move::ControllerMode::kExternalController,
                    Move::MotionGeneratorMode::kCartesianVelocity, maximum_path_deviation,
                    maximum_goal_pose_deviation);
  EXPECT_TRUE(robot.motionGeneratorRunning());
  EXPECT_TRUE(robot.controllerRunning());

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kCartesianPosition;
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();

  robot.update(&sent_command.motion, &sent_command.control);

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kIdle;
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .sendResponse<Move::Response>([]() { return Move::Response(Move::Status::kSuccess); })
      .spinOnce();

  robot.stopMotion();
  EXPECT_FALSE(robot.motionGeneratorRunning());
  EXPECT_TRUE(robot.controllerRunning());

  server
      .onReceiveRobotCommand([](const RobotCommand& command) {
        EXPECT_TRUE(command.motion.motion_generation_finished);
      })
      .spinOnce();

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.motion_generator_mode = MotionGeneratorMode::kIdle;
        robot_state.controller_mode = ControllerMode::kJointImpedance;
      })
      .spinOnce()
      .waitForCommand<SetControllerMode>([](const SetControllerMode::Request& request) {
        EXPECT_EQ(SetControllerMode::ControllerMode::kJointImpedance, request.mode);
        return SetControllerMode::Response(SetControllerMode::Status::kSuccess);
      })
      .spinOnce();

  robot.stopController();

  server.onReceiveRobotCommand([](const RobotCommand&) {}).spinOnce();

  EXPECT_FALSE(robot.motionGeneratorRunning());
  EXPECT_FALSE(robot.controllerRunning());
}

TEST(RobotImpl, CanStopController) {
  RobotCommand sent_command;
  randomRobotCommand(sent_command);

  MockServer server;
  Robot::Impl robot("127.0.0.1");

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .waitForCommand<SetControllerMode>([](const SetControllerMode::Request&) {
        return SetControllerMode::Response(SetControllerMode::Status::kSuccess);
      })
      .spinOnce();

  robot.startController();
  EXPECT_TRUE(robot.controllerRunning());

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kExternalController;
      })
      .spinOnce()
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();

  robot.update(nullptr, &sent_command.control);

  server
      .onSendRobotState([](RobotState& robot_state) {
        robot_state.controller_mode = ControllerMode::kJointImpedance;
      })
      .spinOnce()
      .waitForCommand<SetControllerMode>([](const SetControllerMode::Request&) {
        return SetControllerMode::Response(SetControllerMode::Status::kSuccess);
      })
      .onReceiveRobotCommand([](const RobotCommand&) {})
      .spinOnce();

  robot.stopController();
  EXPECT_FALSE(robot.controllerRunning());
}