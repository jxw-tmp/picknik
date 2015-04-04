/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2012, Willow Garage, Inc.
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
 *   * Neither the name of Willow Garage nor the names of its
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

/* Author: Ioan Sucan, Dave Coleman */

#include <boost/math/constants/constants.hpp>
#include <moveit/trajectory_processing/trajectory_tools.h>
#include <moveit/robot_state/conversions.h>
#include <picknik_main/fix_state_bounds.h>

namespace picknik_main
{

FixStateBounds::FixStateBounds()
  : nh_("~")
{
    if (!nh_.getParam(BOUNDS_PARAM_NAME, bounds_dist_))
    {
      bounds_dist_ = 0.05;
      ROS_WARN_STREAM_NAMED("fix_state_bounds","Param '" << BOUNDS_PARAM_NAME 
                            << "' was not set. Using default value: " << bounds_dist_);
    }
    else
      ROS_INFO_STREAM_NAMED("fix_state_bounds","Param '" << BOUNDS_PARAM_NAME << "' was set to " << bounds_dist_);

    if (!nh_.getParam(DT_PARAM_NAME, max_dt_offset_))
    {
      max_dt_offset_ = 0.5;
      ROS_WARN_STREAM_NAMED("fix_state_bounds","Param '" << DT_PARAM_NAME 
                            << "' was not set. Using default value: " << max_dt_offset_);
    }
    else
      ROS_INFO_STREAM_NAMED("fix_state_bounds","Param '" << DT_PARAM_NAME << "' was set to " << max_dt_offset_);
}

bool FixStateBounds::fixBounds(robot_state::RobotState& robot_state,
                               const moveit::core::JointModelGroup* jmg)
{
  ROS_INFO_STREAM_NAMED("fix_state_bounds","Fixing bounds");

  const std::vector<const robot_model::JointModel*> &jmodels = jmg->getJointModels();

  bool change_req = false;
  for (std::size_t i = 0 ; i < jmodels.size() ; ++i)
  {
    // Check if we have a revolute, continuous joint. If we do, then we only need to make sure
    // it is within de model's declared bounds (usually -Pi, Pi), since the values wrap around.
    // It is possible that the encoder maintains values outside the range [-Pi, Pi], to inform
    // how many times the joint was wrapped. Because of this, we remember the offsets for continuous
    // joints, and we un-do them when the plan comes from the planner

    const robot_model::JointModel* jm = jmodels[i];
    if (jm->getType() == robot_model::JointModel::REVOLUTE)
    {
      if (static_cast<const robot_model::RevoluteJointModel*>(jm)->isContinuous())
      {
        double initial = robot_state.getJointPositions(jm)[0];
        robot_state.enforceBounds(jm);
        double after = robot_state.getJointPositions(jm)[0];
        if (fabs(initial - after) > std::numeric_limits<double>::epsilon())
          change_req = true;
      }
    }
    else
      // Normalize yaw; no offset needs to be remembered
      if (jm->getType() == robot_model::JointModel::PLANAR)
      {
        const double *p = robot_state.getJointPositions(jm);
        double copy[3] = {p[0], p[1], p[2]};
        if (static_cast<const robot_model::PlanarJointModel*>(jm)->normalizeRotation(copy))
        {
          robot_state.setJointPositions(jm, copy);
          change_req = true;
        }
      }
      else
        // Normalize quaternions
        if (jm->getType() == robot_model::JointModel::FLOATING)
        {
          const double *p = robot_state.getJointPositions(jm);
          double copy[7] = {p[0], p[1], p[2], p[3], p[4], p[5], p[6]};
          if (static_cast<const robot_model::FloatingJointModel*>(jm)->normalizeRotation(copy))
          {
            robot_state.setJointPositions(jm, copy);
            change_req = true;
          }
        }
  }

  // pointer to a prefix state we could possibly add, if we detect we have to make changes
  robot_state::RobotStatePtr prefix_state;
  for (std::size_t i = 0 ; i < jmodels.size() ; ++i)
  {
    if (!robot_state.satisfiesBounds(jmodels[i]))
    {
      if (robot_state.satisfiesBounds(jmodels[i], bounds_dist_))
      {
        if (!prefix_state)
          prefix_state.reset(new robot_state::RobotState(robot_state));
        robot_state.enforceBounds(jmodels[i]);
        change_req = true;
        ROS_INFO_NAMED("fix_state_bounds","Starting state is just outside bounds (joint '%s'). Assuming within bounds.", jmodels[i]->getName().c_str());
      }
      else
      {
        std::stringstream joint_values;
        std::stringstream joint_bounds_low;
        std::stringstream joint_bounds_hi;
        const double *p = robot_state.getJointPositions(jmodels[i]);
        for (std::size_t k = 0 ; k < jmodels[i]->getVariableCount() ; ++k)
          joint_values << p[k] << " ";
        const robot_model::JointModel::Bounds &b = jmodels[i]->getVariableBounds();
        for (std::size_t k = 0 ; k < b.size() ; ++k)
        {
          joint_bounds_low << b[k].min_position_ << " ";
          joint_bounds_hi << b[k].max_position_ << " ";
        }
        ROS_WARN_STREAM_NAMED("fix_state_bounds","Joint '" << jmodels[i]->getName() << "' from the starting state is outside bounds by a significant margin: [ " << joint_values.str() << "] should be in the range [ " << joint_bounds_low.str() <<
                              "], [ " << joint_bounds_hi.str() << "] but the error above the ~" << BOUNDS_PARAM_NAME << " parameter (currently set to " << bounds_dist_ << ")");
      }
    }
  }

  if (change_req)
    ROS_INFO_STREAM_NAMED("fix_state_bounds","Change was made");

  return change_req;
}

}