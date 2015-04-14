/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2015, University of Colorado, Boulder
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Univ of CO, Boulder nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Dave Coleman <dave@dav.ee>
   Desc:   Interface between MoveIt! execution tools and PickNik
*/

#ifndef PICKNIK_MAIN__EXECUTION_INTERFACE
#define PICKNIK_MAIN__EXECUTION_INTERFACE

// ROS
#include <ros/ros.h>

// PickNik
#include <picknik_main/visuals.h>
#include <picknik_main/remote_control.h>
#include <picknik_main/manipulation_data.h>

// MoveIt
#include <moveit_grasps/grasp_data.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>

namespace plan_execution
{
MOVEIT_CLASS_FORWARD(PlanExecution);
}

namespace trajectory_execution_manager
{
MOVEIT_CLASS_FORWARD(TrajectoryExecutionManager);
}

namespace picknik_main
{
MOVEIT_CLASS_FORWARD(ExecutionInterface);

class ExecutionInterface
{
public:

  /**
   * \brief Constructor
   * \param verbose - run in debug mode
   */
  ExecutionInterface(bool verbose, RemoteControlPtr remote_control, VisualsPtr visuals, moveit_grasps::GraspDatas grasp_datas,
                     planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor, 
                     ManipulationDataPtr config, const std::string& package_path, moveit::core::RobotStatePtr current_state);

  /**
   * \brief Do a bunch of checks and send to low level controllers
   * \return true on success
   */
  bool executeTrajectory(moveit_msgs::RobotTrajectory &trajectory_msg, bool ignore_collision = false);

  /**
   * \brief Ensure controllers are ready and in correct state
   * \return true on success
   */
  bool checkExecutionManager();

private:
  
  bool checkTrajectoryController(ros::ServiceClient& service_client, const std::string& hardware_name, bool has_ee = false);

  bool saveTrajectory(const moveit_msgs::RobotTrajectory &trajectory_msg, const std::string &file_name);

  bool getFilePath(std::string &file_path, const std::string &file_name) const;

  moveit::core::RobotStatePtr getCurrentState();

  // A shared node handle
  ros::NodeHandle nh_;

  // Show more visual and console output, with general slower run time.
  bool verbose_;

  // File path to ROS package on drive
  std::string package_path_;

  // Trajectory execution
  trajectory_execution_manager::TrajectoryExecutionManagerPtr trajectory_execution_manager_;
  boost::shared_ptr<plan_execution::PlanExecution> plan_execution_;

  RemoteControlPtr remote_control_;
  planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor_;
  VisualsPtr visuals_;

  // Robot-specific data for generating grasps
  moveit_grasps::GraspDatas grasp_datas_;

  // Robot-sepcific data for the APC
  ManipulationDataPtr config_;

  // Check which controllers are loaded
  ros::ServiceClient zaber_list_controllers_client_;
  ros::ServiceClient kinova_list_controllers_client_;

  // Allocated memory for robot state
  moveit::core::RobotStatePtr current_state_;

}; // end class

} // end namespace

#endif
