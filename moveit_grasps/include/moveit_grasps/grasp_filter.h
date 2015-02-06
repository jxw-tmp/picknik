/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, University of Colorado, Boulder
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

// Author: Dave Coleman
// Desc:   Filters grasps based on kinematic feasibility

#ifndef MOVEIT_GRASPS__GRASP_FILTER_
#define MOVEIT_GRASPS__GRASP_FILTER_

// ROS
#include <ros/ros.h>
#include <tf_conversions/tf_eigen.h>
#include <geometry_msgs/PoseArray.h>
#include <geometry_msgs/PoseStamped.h>
#include <moveit_msgs/Grasp.h>

// Grasping
#include <moveit_grasps/grasps.h>

// Rviz
#include <moveit_visual_tools/moveit_visual_tools.h>

// MoveIt
#include <moveit/robot_state/robot_state.h>
#include <moveit/kinematics_plugin_loader/kinematics_plugin_loader.h>
#include <moveit/planning_scene_monitor/planning_scene_monitor.h>

// C++
#include <boost/thread.hpp>
#include <math.h>
#define _USE_MATH_DEFINES

namespace moveit_grasps
{

/**
 * \brief Contains collected data for each potential grasp after it has been verified / filtered
 */
struct GraspSolution
{
  moveit_msgs::Grasp grasp_;
  std::vector<double> grasp_ik_solution_;
  std::vector<double> pregrasp_ik_solution_;
};

/**
 * \brief Struct for passing parameters to threads, for cleaner code
 */ 
struct IkThreadStruct
{
  IkThreadStruct(
    const std::vector<moveit_msgs::Grasp> &possible_grasps, // the input
    std::vector<GraspSolution> &filtered_grasps, // the result
    Eigen::Affine3d &link_transform,
    int grasps_id_start,
    int grasps_id_end,
    kinematics::KinematicsBaseConstPtr kin_solver,
    bool filter_pregrasp,
    std::string ee_parent_link,
    double timeout,
    boost::mutex *lock,
    int thread_id)
    : possible_grasps_(possible_grasps),
      filtered_grasps_(filtered_grasps),
      link_transform_(link_transform),
      grasps_id_start_(grasps_id_start),
      grasps_id_end_(grasps_id_end),
      kin_solver_(kin_solver),
      filter_pregrasp_(filter_pregrasp),
      ee_parent_link_(ee_parent_link),
      timeout_(timeout),
      lock_(lock),
      thread_id_(thread_id)
  {
  }
  const std::vector<moveit_msgs::Grasp> &possible_grasps_;
  std::vector<GraspSolution> &filtered_grasps_;
  Eigen::Affine3d link_transform_;
  int grasps_id_start_;
  int grasps_id_end_;
  kinematics::KinematicsBaseConstPtr kin_solver_;
  bool filter_pregrasp_;
  std::string ee_parent_link_;
  double timeout_;
  boost::mutex *lock_;
  int thread_id_;
};

// Class
class GraspFilter
{
private:
  // State of robot
  robot_state::RobotStatePtr robot_state_;

  // threaded kinematic solvers
  std::map<std::string, std::vector<kinematics::KinematicsBaseConstPtr> > kin_solvers_;

  // class for publishing stuff to rviz
  moveit_visual_tools::MoveItVisualToolsPtr visual_tools_;

  bool verbose_;

public:

  // Constructor
  GraspFilter( robot_state::RobotStatePtr robot_state, 
               moveit_visual_tools::MoveItVisualToolsPtr& visual_tools );

  // Destructor
  ~GraspFilter();

  // Of an array of grasps, choose just one for use
  bool chooseBestGrasp( std::vector<GraspSolution>& filtered_grasps,
                        GraspSolution& chosen );

  /**
   * \brief Choose the 1st grasp that is kinematically feasible
   * \param possible_grasps - all possible grasps that this will test
   * \param filtered_grasps - result - only the grasps that are kinematically feasible
   * \param whether to also check ik feasibility for the pregrasp position
   * \return true on success 
   */ 
  bool filterGrasps(const std::vector<moveit_msgs::Grasp>& possible_grasps,
                    std::vector<GraspSolution>& filtered_grasps,
                    bool filter_pregrasp, 
                    const std::string &ee_parent_link, 
                    const robot_model::JointModelGroup* jmg);

private:

  /**
   * \brief Thread for checking part of the possible grasps list
   * \param 
   */
  void filterGraspThread(IkThreadStruct ik_thread_struct);

public:

  /**
   * \brief Filter using collision checking. Run this after filterGrasps()
   * \param potential grasps - invalid ones will be removed
   * \param the planning scene containing the objects to collision check with
   * \return true on success
   */
  bool filterGraspsInCollision(std::vector<GraspSolution>& possible_grasps,
                               planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor,
                               const robot_model::JointModelGroup* jmg,
                               robot_state::RobotStatePtr robot_state,
                               bool verbose = false);

  bool filterGraspsInCollisionHelper(std::vector<GraspSolution>& possible_grasps,
                                     planning_scene_monitor::PlanningSceneMonitorPtr planning_scene_monitor,
                                     const robot_model::JointModelGroup* jmg,
                                     robot_state::RobotStatePtr robot_state,
                                     bool verbose);

  /**
   * \brief Display collision points in Rviz
   * \param state of robot
   * \param planning scene
   * \return true on success
   */
  bool publishContactPoints(const moveit::core::RobotStatePtr robot_state, planning_scene::PlanningScenePtr planning_scene);

}; // end of class

typedef boost::shared_ptr<GraspFilter> GraspFilterPtr;
typedef boost::shared_ptr<const GraspFilter> GraspFilterConstPtr;

} // namespace

#endif
