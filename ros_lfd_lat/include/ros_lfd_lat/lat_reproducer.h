/*
 * lat_reproducer.h
 *
 *  Created on: 07.01.2013
 *      Author: Benjamin Reiner
 */

#ifndef LAT_REPRODUCER_H_
#define LAT_REPRODUCER_H_

#define BOOST_FILESYSTEM_VERSION 3

#include "ros/ros.h"
#include "sensor_msgs/JointState.h"
#include "actionlib/client/simple_action_client.h"
#include "actionlib/client/simple_client_goal_state.h"
#include "pr2_controllers_msgs/JointTrajectoryAction.h"
#include "katana_msgs/JointMovementAction.h"
#include "tf/transform_listener.h"
#include "geometry_msgs/PointStamped.h"
#include "ar_track_alvar/AlvarMarkers.h"

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <deque>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

#include "../leatra/lfd.hh"
#include "../leatra/stringhelp.hh"
#include "ros_lfd_lat/helpers.h"
#include "ros_lfd_lat/LatConstants.h"

typedef actionlib::SimpleActionClient< pr2_controllers_msgs::JointTrajectoryAction > TrajClient;
typedef actionlib::SimpleActionClient<katana_msgs::JointMovementAction> MoveClient;

/**
 * Returns a vector with available trajectory names.
 *
 * @param trajectoryDir Directory where the trajectories are located. With USE_USER_HOME_STRING the users home dir is
 * taken automatically.
 * @return Vector with the trajectory names.
 */
std::vector<std::string> getAvailableTrajectories(std::string trajectoryDir = USE_USER_HOME_STRING);

/**
 * Returns a vector with all the joint names of the robot.
 * Subscribes shortly for the /joint_state topic.
 *
 * @param node Current node
 * @return vector with the joint names
 */
std::vector<std::string> getJointNames(bool inSimulation);

/**
 * Callback for the /joint_state topic. Unsubscribes immediately after first call.
 */
void jointStateCallback(const sensor_msgs::JointStateConstPtr& jointState);

/**
 * Moves the robot to the first position of the given trajectory.
 *
 * @param trajectroy The trajectory to its first point the robot should move.
 * @param inSimulutaion Flag that determines, whether the program is launched in Gazebo or not.
 */
void moveRobotToStartPos(const std::deque<std::deque<double> >& trajectory, bool inSimulation);

/**
 * Moves the robot to a stable home position.
 */
void moveRobotToHomePos();

/**
 * Interactive selection of the trajectory.
 * Lists the existing trajectories to the screen and prompts the user to enter the number of the trajectory.
 */
std::string readTrajectoryFromUser(std::string trajectoryDir = USE_USER_HOME_STRING);

/**
 * Prints a help message.
 */
void printHelpMessage();

/**
 * Callback for the object tracking topic.
 */
void objectTrackerCallback(const ar_track_alvar::AlvarMarkersConstPtr& markers);

/**
 * Returns the number of the step that is being executed at the moment.
 */
unsigned int getCurrentStepNo();

/**
 * Determines whether the given object is under a constraint at that moment or not.
 *
 * @param objectId Use this object.
 * @param step at that time step
 * @param constraints in these constraints
 * @return Is this object under a constraint at that time.
 */
bool objectUnderConstraint(int objectId, unsigned int step, const std::deque<int>& constraints);

/**
 * Determines whether the given object has been under a constraint before that moment or not.
 *
 * @param objectId Use this object.
 * @param step at that time step
 * @param constraints in these constraints
 * @return Has this object been under a constraint before that time.
 */
bool objectAfterConstraint(int objectId, unsigned int step, const std::deque<int>& constraints);

/**
 * Determines whether the given object has been under a constraint before that moment or not.
 *
 * @param objectId Use this object.
 * @param startStep Start searching at this step.
 * @param constraints Use these constraints
 * @return Number of steps till the given object has a constraint. Zero if the object has passed its constraint or is
 * under constraint. If the startStep is greater than the number of steps in the constraints zero is returned, also.
 */
unsigned int getStepsTillConstraint(int objectId, unsigned int startStep, const std::deque<int>& constraints);

/**
 * Creates a joint trajectory goal from the given trajectory.
 *
 * @param trajectory The trajectory with the computed values
 * @param inSimulation Determines whether the node runs in Gazebo or on the real robot
 * @return The filled goal
 */
pr2_controllers_msgs::JointTrajectoryGoal createGoal(
		const std::deque<std::deque<double> >& trajectory, bool inSimulation);

/**
 * Creates a joint trajectory goal for the gripper from the given trajectory.
 *
 * This function is only used in simulation.
 *
 * @param trajectory The trajectory with the computed values
 * @return The filled gripper goal
 */
pr2_controllers_msgs::JointTrajectoryGoal createGripperGoal(const std::deque<std::deque<double> >& trajectory);

/**
 * Creates a trajectory goal, so that the currently performing trajectory is altered smoothly.
 *
 * The new trajectory starts in the future and alters the currently running trajectory.
 * See this link http://wiki.ros.org/robot_mechanism_controllers/JointTrajectoryActionController
 *
 * @param newTrajectory The trajectory with the updated object positions.
 * @param oldTrajectory The trajectory with the old object positions.
 * @param inSimulation Flag that shows if the node runs in simulation or not.
 * @return A trajectory goal that is ready to send to the arm.
 */
pr2_controllers_msgs::JointTrajectoryGoal createUpdatedGoal(
		const std::deque<std::deque<double> >& newTrajectory,
		const std::deque<std::deque<double> >& oldTrajectory,
		bool inSimulation
		);

/**
 * Creates a gripper trajectory goal, so that the currently performing trajectory is altered smoothly.
 *
 * The new trajectory starts in the future and alters the currently running trajectory.
 * See this link http://wiki.ros.org/robot_mechanism_controllers/JointTrajectoryActionController
 *
 * This function is only used in simulation.
 *
 * @param newTrajectory The trajectory with the updated object positions.
 * @param oldTrajectory The trajectory with the old object positions.
 * @return A trajectory goal that is ready to send to the gripper.
 */
pr2_controllers_msgs::JointTrajectoryGoal createUpdatedGripperGoal(
		const std::deque<std::deque<double> >& newTrajectory,
		const std::deque<std::deque<double> >& oldTrajectory
		);

/**
 * Returns the maximum distance between the two trajectories in the given interval.
 *
 * @param newTrajectory The trajectory with the updated object positions.
 * @param oldTrajectory The trajectory with the old object positions.
 * @param startStep Left boundary of the interval
 * @param endStep Right boundary of the interval
 * @return absolute maximum distance in joint space
 */
double getMaximumDistance(
		const std::deque<std::deque<double> >& newTrajectory,
		const std::deque<std::deque<double> >& oldTrajectory,
		unsigned int startStep,
		unsigned int endStep
		);

/**
 * Block till the robots velocity is zero.
 */
void waitTillRobotStoped();

int main(int argc, char **argv);

#endif /* LAT_REPRODUCER_H_ */
