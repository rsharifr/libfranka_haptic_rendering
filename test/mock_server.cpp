#include "mock_server.h"

#include <iostream>

#include <Poco/Net/StreamSocket.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/ServerSocket.h>

#include <franka/robot_state.h>
#include <research_interface/constants.h>
#include <research_interface/rbk_types.h>

MockServer& MockServer::onConnect(ConnectCallbackT on_connect) {
  on_connect_ = on_connect;
  return *this;
}

MockServer& MockServer::onSendRobotState(SendRobotStateCallbackT on_send_robot_state) {
  on_send_robot_state_ = on_send_robot_state;
  return *this;
}

void MockServer::start() {
  server_thread_ = std::thread(&MockServer::serverThread, this);
  std::unique_lock<std::mutex> lock(mutex_);
  cv_.wait(lock);
}

void MockServer::serverThread() {
  Poco::Net::ServerSocket srv({"localhost", research_interface::kCommandPort}); // does bind + listen
  cv_.notify_one();

  Poco::Net::SocketAddress remote_address;
  Poco::Net::StreamSocket tcp_socket = srv.acceptConnection(remote_address);

  research_interface::ConnectRequest request;
  tcp_socket.receiveBytes(&request, sizeof(request));

  research_interface::ConnectReply reply;
  reply.version = 1;
  reply.status = research_interface::ConnectReply::Status::kSuccess;

  if (on_connect_) {
    on_connect_(request, reply);
  }

  tcp_socket.sendBytes(&reply, sizeof(reply));

  // Send robot state over UDP
  if (!on_send_robot_state_) {
    return;
  }

  Poco::Net::DatagramSocket udp_socket({std::string("localhost"), 0});
  franka::RobotState robot_state = on_send_robot_state_();
  research_interface::RobotState rbk_robot_state;

  static_assert(sizeof(rbk_robot_state) == sizeof(robot_state),
                "research_interface::RobotState size changed - adjust "
                "franka::RobotState?");
  std::copy(robot_state.q_start.cbegin(), robot_state.q_start.cend(),
            rbk_robot_state.q_start.begin());
  std::copy(robot_state.O_T_EE_start.cbegin(), robot_state.O_T_EE_start.cend(),
            rbk_robot_state.O_T_EE_start.begin());
  std::copy(robot_state.elbow_start.cbegin(), robot_state.elbow_start.cend(),
            rbk_robot_state.elbow_start.begin());
  std::copy(robot_state.tau_J.cbegin(), robot_state.tau_J.cend(),
            rbk_robot_state.tau_J.begin());
  std::copy(robot_state.dtau_J.cbegin(), robot_state.dtau_J.cend(),
            rbk_robot_state.dtau_J.begin());
  std::copy(robot_state.q.cbegin(), robot_state.q.cend(),
            rbk_robot_state.q.begin());
  std::copy(robot_state.dq.cbegin(), robot_state.dq.cend(),
            rbk_robot_state.dq.begin());
  std::copy(robot_state.q_d.cbegin(), robot_state.q_d.cend(),
            rbk_robot_state.q_d.begin());
  std::copy(robot_state.joint_contact.cbegin(),
            robot_state.joint_contact.cend(),
            rbk_robot_state.joint_contact.begin());
  std::copy(robot_state.cartesian_contact.cbegin(),
            robot_state.cartesian_contact.cend(),
            rbk_robot_state.cartesian_contact.begin());
  std::copy(robot_state.joint_collision.cbegin(),
            robot_state.joint_collision.cend(),
            rbk_robot_state.joint_collision.begin());
  std::copy(robot_state.cartesian_collision.cbegin(),
            robot_state.cartesian_collision.cend(),
            rbk_robot_state.cartesian_collision.begin());
  std::copy(robot_state.tau_ext_hat_filtered.cbegin(),
            robot_state.tau_ext_hat_filtered.cend(),
            rbk_robot_state.tau_ext_hat_filtered.begin());
  std::copy(robot_state.O_F_ext_hat_EE.cbegin(),
            robot_state.O_F_ext_hat_EE.cend(),
            rbk_robot_state.O_F_ext_hat_EE.begin());
  std::copy(robot_state.EE_F_ext_hat_EE.cbegin(),
            robot_state.EE_F_ext_hat_EE.cend(),
            rbk_robot_state.EE_F_ext_hat_EE.begin());

  udp_socket.sendTo(&rbk_robot_state, sizeof(rbk_robot_state), {remote_address.host(), request.udp_port});
}

MockServer::~MockServer() {
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
}
