/*
 * lat_reproducer.cpp
 *
 *  Created on: 07.01.2013
 *      Author: Benjamin Reiner
 */
#include "ros_lfd_lat/lat_reproducer.h"

std::deque<object> objects;
std::vector<std::string> jointNames;
std::vector<std::string> gripperJointNames;
std::vector<double> jointPositions;			// updated constantly by jointStateCallback
std::vector<double> jointVelocities;
std::vector<double> gripperJointPositions;
bool allObjectsFound = false;
lfd lfd;
double objectShiftThreshold;
ros::Time trajectoryStartTime = ros::Time(0);

// reproduce this trajectory
std::string trajectoryName;

// specifies the folder where the trajectories are stored (default user_home)
std::string trajectoryDir;

// stores the id of the object that has moved
int movedObjectId;

std::deque<int> constraints;

// how many times has the trajectory been recalculated?
unsigned int recalculationCount = 0;

// triggers the recalculation of the trajectory if set to true
bool recalculateTrajectory = false;

// stores the object position when a shift is detected. This can be used to prevent position jumps
geometry_msgs::PointStamped* newObjectPosition = NULL;

// Points to the trajectory that the robot is performing at the moment.
std::deque<std::deque<double> >* currentTrajectory = NULL;

// getCurrentStepNo stores here current step. Should grow monotonically. Only recalculations are exceptions.
unsigned int recentStep = 0;

bool inSimulationGlob = false;

tf::TransformListener* tfListenerPtr;

std::vector<std::string> getAvailableTrajectories(std::string trajectoryDir)
{
	std::string filename;

	std::vector<std::string> trajectories;
	const std::string POSTFIX = ".tra";	//every trajectory directory ends with .tra

	boost::filesystem::path trajDir;

	if(trajectoryDir == USE_USER_HOME_STRING)
	{
		trajDir = getHomeDir();
	}
	else
	{
		trajDir = boost::filesystem::path(trajectoryDir);

		if(!boost::filesystem::is_directory(trajDir))
		{
			ROS_ERROR("%s is not a directory! (Will use home dir instead)", trajectoryDir.c_str());
			trajDir = getHomeDir();
		}
	}

	boost::filesystem::directory_iterator endIter;

	for (boost::filesystem::directory_iterator dirIter(trajDir); dirIter != endIter; ++dirIter)
	{
		if(boost::filesystem::is_directory(dirIter->path()))
		{
			if(boost::algorithm::ends_with(
					dirIter->path().filename().c_str(),
					POSTFIX))
			{
				filename = dirIter->path().filename().c_str();
				boost::algorithm::erase_tail(filename, POSTFIX.size());
				trajectories.push_back(filename);
			}
		}
	}

	return trajectories;
}

std::vector<std::string> getJointNames(bool inSimulation)
{
	while(jointNames.empty() || jointPositions.empty())
	{
		ros::Duration(0.001).sleep();
		ros::spinOnce();
	}

	if(inSimulation)
	{
		if(jointNames.size() < 2)
		{
			ROS_ERROR("not enough joint names!");
		}

		// copy the gripper names and joint positions
		if(gripperJointNames.empty() && jointNames.size() == 7)
		{
			gripperJointNames.push_back(jointNames.at(jointNames.size() - 2));
			gripperJointNames.push_back(jointNames.at(jointNames.size() - 1));
		}

		if(jointPositions.size() == 7)
		{
			gripperJointPositions.clear();

			gripperJointPositions.push_back(
				jointPositions.at(jointPositions.size() - 2)
			);

			gripperJointPositions.push_back(
				jointPositions.at(jointPositions.size() - 1)
			);
		}

		// in Gazebo both finger joints
		if(jointNames.size() == 7)
		{
			jointNames.pop_back();
			jointNames.pop_back();
		}

		if(jointPositions.size() == 7)
		{
			jointPositions.pop_back();
			jointPositions.pop_back();
		}
	}
	else
	{
		// in real life the katana_l_finger_joint has to be removed only
		if(jointPositions.size() == 7)
		{
			jointPositions.pop_back();
		}

		if(jointNames.size() == 7)
		{
			jointNames.pop_back();
		}
	}

	return jointNames;

	// with the destruction of jointStateListener the topic is automatically
	// unsubscribed
}

void jointStateCallback(const sensor_msgs::JointStateConstPtr& jointState)
{
	//ROS_INFO("In joint state callback seq: %u", jointState->header.seq);

	jointPositions = jointState->position;
	jointNames = jointState->name;
	jointVelocities = jointState->velocity;
}

void moveRobotToStartPos(const std::deque<std::deque<double> >& trajectory, bool inSimulation)
{
	unsigned int armJointCount = 6;
	if(inSimulation)
	{
		armJointCount = 5;
	}

	MoveClient moveClient("katana_arm_controller/joint_movement_action");
	TrajClient gripperClient("katana_arm_controller/gripper_joint_trajectory_action");
	moveClient.waitForServer();

	katana_msgs::JointMovementGoal moveGoal;
	pr2_controllers_msgs::JointTrajectoryGoal gripperGoal;
	moveGoal.jointGoal.name = getJointNames(inSimulation);

	if(inSimulation)
	{
		gripperGoal.trajectory.joint_names = gripperJointNames;
		gripperGoal.trajectory.points.resize(2);	// current joint state
													// and desired joint state

		// current state
		gripperGoal.trajectory.points[0].positions.resize(2);
		gripperGoal.trajectory.points[0].positions[0] = gripperJointPositions[0];
		gripperGoal.trajectory.points[0].positions[1] = gripperJointPositions[1];
		gripperGoal.trajectory.points[0].velocities.resize(2);
		gripperGoal.trajectory.points[0].velocities[0] = 0.0;
		gripperGoal.trajectory.points[0].velocities[1] = 0.0;
		gripperGoal.trajectory.points[0].time_from_start = ros::Duration(0.5);

		// desired initial position
		gripperGoal.trajectory.points[1].positions.resize(2);
		gripperGoal.trajectory.points[1].positions[0] = trajectory.at(5).at(0);
		gripperGoal.trajectory.points[1].positions[1] = trajectory.at(6).at(0);
		gripperGoal.trajectory.points[1].velocities.resize(2);
		gripperGoal.trajectory.points[1].velocities[0] = 0.0;
		gripperGoal.trajectory.points[1].velocities[1] = 0.0;
		gripperGoal.trajectory.points[1].time_from_start = ros::Duration(3.0);
	}

	moveGoal.jointGoal.position.resize(armJointCount);
	for (unsigned int jointIndex = 0; jointIndex < armJointCount; ++jointIndex) {
		moveGoal.jointGoal.position.at(jointIndex) = trajectory.at(jointIndex).at(0);
	}

	ROS_INFO("Moving arm to initial position.");
	moveClient.sendGoal(moveGoal);

	while(!moveClient.getState().isDone() && ros::ok())
	{
		ros::Duration(0.001).sleep();
		ros::spinOnce();
	}

	if(moveClient.getState().state_ == moveClient.getState().SUCCEEDED)
	{
		ROS_INFO("Arm movement finished successfully.");
	}
	else
	{
		ROS_WARN("Arm movement finished not successfully! (%s)",
				moveClient.getState().toString().c_str());
	}

	if (inSimulation)
	{
		// when to start the trajectory
		gripperGoal.trajectory.header.stamp = ros::Time::now() + ros::Duration(0.4);
		ROS_INFO("Moving gripper to initial position");
		gripperClient.sendGoal(gripperGoal);

		while(!gripperClient.getState().isDone() && ros::ok())
		{
			ros::Duration(0.001).sleep();
			ros::spinOnce();
		}

		if(gripperClient.getState().state_ == gripperClient.getState().SUCCEEDED)
		{
			ROS_INFO("Gripper trajectory finished successfully.");
		}
		else
		{
			ROS_WARN("Gripper trajectory finished not successfully! (%s)",
					gripperClient.getState().toString().c_str());
		}
	}
}

void moveRobotToHomePos()
{
	MoveClient moveClient("katana_arm_controller/joint_movement_action");
	TrajClient gripperClient("katana_arm_controller/gripper_joint_trajectory_action");
	moveClient.waitForServer();

	katana_msgs::JointMovementGoal moveGoal;
	moveGoal.jointGoal.name = getJointNames(inSimulationGlob);
	moveGoal.jointGoal.name.resize(5);
	moveGoal.jointGoal.position.resize(5);
	moveGoal.jointGoal.position.at(0) = 0;			// katana_motor1_pan_joint
	moveGoal.jointGoal.position.at(1) = 2.1131;		// katana_motor2_lift_joint
	moveGoal.jointGoal.position.at(2) = -2.1827;	// katana_motor3_lift_joint
	moveGoal.jointGoal.position.at(3) = -2.0114;	// katana_motor4_lift_joint
	moveGoal.jointGoal.position.at(4) = -0.0234;	// katana_motor5_wrist_roll_joint

	ROS_INFO("Moving arm to home position.");
	moveClient.sendGoal(moveGoal);

	while(!moveClient.getState().isDone() && ros::ok())
	{
		ros::Duration(0.001).sleep();
		ros::spinOnce();
	}

	if(moveClient.getState().state_ == moveClient.getState().SUCCEEDED)
	{
		ROS_INFO("Arm movement finished successfully.");
	}
	else
	{
		ROS_WARN("Arm movement finished not successfully! (%s)",
				moveClient.getState().toString().c_str());
	}
}

std::string readTrajectoryFromUser(std::string trajectoryDir)
{
	unsigned int traNumber = -2;
	std::string traNumberStr = "-1";
	std::string trajectoryName = "default_trajectory_name";

	ROS_INFO("Available trajectories:");
	std::vector<std::string> trajectories = getAvailableTrajectories(trajectoryDir);

	for (unsigned int i = 0; i < trajectories.size(); ++i) {
			ROS_INFO_STREAM(i << ") " << trajectories.at(i).c_str());
	}

	ROS_INFO("Which trajectory should be reproduced? (Select by number)");
	getline(std::cin, traNumberStr);

	try
	{
		traNumber = boost::lexical_cast<unsigned int>(traNumberStr);

		if (traNumber < trajectories.size())
		{
			trajectoryName = trajectories.at(traNumber);
		}
		else
		{
			ROS_WARN("Selection out of range!");
			trajectoryName = "out_of_range_selection";
		}
	}
	catch (boost::bad_lexical_cast &)
	{
		ROS_WARN("No valid input!");
		trajectoryName = "no_valid_selection";
	}
	catch (...)
	{
		ROS_ERROR("Unhandled exeption in readTrajectoryFromUser!");
		ROS_BREAK();
	}

	return trajectoryName;
}

void printHelpMessage()
{
	ROS_INFO(
		"Usage: ./lat_reproducer <object tracking topic> <trajectory name> <trajectory dir> <draw graph>"
		" <object shift threshold> <arm controller>"
		);
	ROS_INFO("or roslaunch ros_lfd_lat lat_reproducer [trajectory_name:=<trajectory name>]");
}

bool isObjectStored(const std::string objName)
{
	for (unsigned int objIdx = 0; objIdx < objects.size(); ++objIdx)
	{
		if(objects.at(objIdx).get_name() == objName)
		{
			return true;
		}
	}

	return false;
}

void objectTrackerCallback(const ar_track_alvar::AlvarMarkersConstPtr& markers)
{
	for (unsigned int markerIdx = 0; markerIdx < markers->markers.size(); ++markerIdx)
	{
		ar_track_alvar::AlvarMarker marker = markers->markers.at(markerIdx);
		if(marker.id == IKEA_CUP_SOLBRAEND_BLUE_ID || marker.id == COCA_COLA_CAN_250ML_ID)
		{
			// transform marker coordinates in correct frame
			geometry_msgs::PointStamped pointStampedIn, pointStampedOut;
			pointStampedIn.point = marker.pose.pose.position;
			pointStampedIn.header.frame_id = marker.header.frame_id;

			ros::Duration waitTimeout(1.0);

			tfListenerPtr->waitForTransform(OBJECT_TARGET_FRAME, pointStampedIn.header.frame_id,
					pointStampedIn.header.stamp, waitTimeout);

			tfListenerPtr->transformPoint(OBJECT_TARGET_FRAME, pointStampedIn, pointStampedOut);
			if(isObjectReachable(pointStampedOut))
			{
				if(!allObjectsFound)
				{
					ROS_DEBUG("not all objects found yet");
					// check if this objects was already stored
					if(!isObjectStored(OBJECT_NAMES[marker.id]))
					{
						// store object
						object obj = object();

						obj.set_name(OBJECT_NAMES[marker.id]);
						obj.add_coordinate(pointStampedOut.point.x);
						obj.add_coordinate(pointStampedOut.point.y);
						obj.add_coordinate(pointStampedOut.point.z);

						objects.push_back(obj);

						ROS_INFO("Added object %s on coordinate [%f, %f, %f]", obj.get_name().c_str(),
								obj.get_coordinate(0),
								obj.get_coordinate(1),
								obj.get_coordinate(2));

						// was this the last object to store?
						if(lfd.mandatory_objects(&objects, "/" + trajectoryName, trajectoryDir.c_str()))
						{
							allObjectsFound = true;
						}
					}
				}
				else	// allObjectsFound == true
				{
					// check if an object moved more than the threshold
					object obj;
					unsigned int objIdx;
					for(objIdx = 0; objIdx < objects.size(); ++objIdx)
					{
						if(objects.at(objIdx).get_name() == OBJECT_NAMES[marker.id])
						{
							obj = objects.at(objIdx);
							break;
						}
					}

					double movedDistance = sqrt(
							pow(pointStampedOut.point.x - obj.get_coordinate(0), 2)
							+ pow(pointStampedOut.point.y - obj.get_coordinate(1), 2)
							+ pow(pointStampedOut.point.z - obj.get_coordinate(2), 2)
							);

					// update object position
					if(movedDistance >= objectShiftThreshold)
					{
						// sometimes the object position jumps to a wrong position and then back to the correct one
						// these events should be avoided
						if(newObjectPosition != NULL)
						{
							double delta = fabs(pointStampedOut.point.x - newObjectPosition->point.x)
									+ fabs(pointStampedOut.point.y - newObjectPosition->point.y)
									+ fabs(pointStampedOut.point.z - newObjectPosition->point.z);

							if(delta < 0.012)	// no jump object really moved
							{
								ROS_INFO("delta: %f", delta);
								// update only if the objects is before its constraint
								// update positions if the trajectory has not started without triggering the recalculation
								if(trajectoryStartTime.isZero() ||
										(!objectUnderConstraint(objIdx, getCurrentStepNo(), constraints)
										&& !objectAfterConstraint(objIdx, getCurrentStepNo(), constraints))
									)
								{
									ROS_INFO("Object %s moved %fm to (%f/%f/%f) step: %u", obj.get_name().c_str(), movedDistance,
											pointStampedOut.point.x, pointStampedOut.point.y, pointStampedOut.point.z,
											getCurrentStepNo());

									std::deque<double> positions;
									positions.push_back(pointStampedOut.point.x);
									positions.push_back(pointStampedOut.point.y);
									positions.push_back(pointStampedOut.point.z);
									objects.at(objIdx).set_coordinates(positions);

									// before the trajectory started the recalculation does not have to triggered
									if(!trajectoryStartTime.isZero()
											&& getCurrentStepNo() < (currentTrajectory->at(0).size() - 5))
									{
										// trigger recalculation
										movedObjectId = (int)objIdx;
										recalculateTrajectory = true;
									}
								}
							}
							else
							{
								ROS_WARN("Object position jump detected!");
							}

							// clean up the old object
							delete newObjectPosition;

							newObjectPosition = NULL;
						}
						else	// newObjectPosition is NULL
						{		// store the new position
							newObjectPosition = new geometry_msgs::PointStamped();
							newObjectPosition->point = pointStampedOut.point;
						}

					} // end if(movedDistance >= objectShiftThreshold)
				}
			}
			else
			{
				//ROS_WARN("object not reachable");
			} // end of if(isObjectReachable(pointStampedOut))
		}
	}	// end of for loop

}

unsigned int getCurrentStepNo()
{
	unsigned int currentStepNo = 0;

	if(currentTrajectory == NULL)
	{
		ROS_ERROR("getCurrentStepNo was called before the trajectory was calculated!");
		return currentStepNo;
	}

	getJointNames(inSimulationGlob);		// fetch the real joint positions

	double minimumDelta = INFINITY;
	unsigned int minimumStep = 0;

	unsigned int startStep = 0;

	if(recentStep >= 10)
	{
		startStep = recentStep - 10;
	}

	for(unsigned int step = startStep; step < currentTrajectory->at(0).size(); ++step)
	{

		double delta = 0;
		for(unsigned int joint = 0; joint < jointPositions.size(); ++joint)
		// getJointNames removes one finger joint
		{
			double current = currentTrajectory->at(joint).at(step);
			double real = jointPositions.at(joint);
			delta += fabs(current - real);
		}

		if(inSimulationGlob)
		{
			delta += fabs(currentTrajectory->at(5).at(step) - gripperJointPositions.at(0));
			delta += fabs(currentTrajectory->at(6).at(step) - gripperJointPositions.at(1));
		}

		if(delta < minimumDelta)
		{
			minimumDelta = delta;
			minimumStep = step;
		}

		// avoid jumping to points in future
		ros::Duration duration = ros::Time::now() - trajectoryStartTime;
		unsigned int estimatedStep = static_cast<unsigned int>(duration.toSec() * REPRODUCE_HZ * 1.2);
		// add 20% tolerance
		if(step > estimatedStep)
		{
			break;
		}
	}

	currentStepNo = minimumStep;
	recentStep = currentStepNo;

	//ROS_INFO("Current step no: %u", currentStepNo);
	return currentStepNo;
}

bool objectUnderConstraint(int objectId, unsigned int step, const std::deque<int>& constraints)
{
	ROS_ASSERT_MSG(constraints.size() > 1, "objectUnderConstraint called with empty constraints");
	ROS_ASSERT_MSG(objectId >= 0, "only objectIds >= 0 are valid");
	ROS_INFO("constraints size: %zu, step: %u", constraints.size(), step * THINNING_FACTOR);
	bool underConstraint = false;
	if((step * THINNING_FACTOR) >= constraints.size())	// TODO get constraints directly in right size
	{
		ROS_WARN("Step is greater than constraints size in objectUnderConstraint. Returning false.");
		return false;
	}
	if(constraints.at(step * THINNING_FACTOR) == objectId)
	{
		underConstraint = true;
	}

	return underConstraint;
}

bool objectAfterConstraint(int objectId, unsigned int step, const std::deque<int>& constraints)
{
	ROS_ASSERT_MSG(constraints.size() > 1, "objectAfterConstraint called with empty constraints");
	ROS_ASSERT_MSG(objectId >= 0, "only objectIds >= 0 are valid");

	bool afterConstraint = false;

	if((step * THINNING_FACTOR) > constraints.size())
	{
		ROS_WARN("Step is greater than constraint size.");
		step = constraints.size();
	}

	step *= THINNING_FACTOR;

	for (unsigned int currentStep = 1; currentStep <= step; ++currentStep)
	{
		if(constraints.at(currentStep - 1) == objectId && constraints.at(currentStep) != objectId)
		{
			afterConstraint = true;
			ROS_INFO("after constraint set to true");
			break;
		}
	}

	return afterConstraint;
}

unsigned int getStepsTillConstraint(int objectId, unsigned int startStep, const std::deque<int>& constraints)
{
	ROS_ASSERT_MSG(constraints.size() > 1, "Empty constraints given.");
	ROS_ASSERT_MSG(objectId >= 0, "only objectIds >= 0 are valid");

	unsigned int steps = 0;

	if(startStep >= constraints.size())
	{
		ROS_WARN("startStep greater than the number of steps in the constraints. Returning zero.");
		return steps;		// return 0
	}

	if(objectUnderConstraint(objectId, startStep, constraints)
			|| objectAfterConstraint(objectId, startStep, constraints))
	{
		return steps;		// return 0
	}

	for(unsigned int step = startStep; step < constraints.size(); ++step)
	{
		if(constraints.at(step) == objectId)
		{
			break;		//constraint found
		}
		++steps;
	}

	// sample the steps down
	steps /= THINNING_FACTOR;

	return steps;
}

pr2_controllers_msgs::JointTrajectoryGoal createGoal(
		const std::deque<std::deque<double> >& trajectory, bool inSimulation)
{
	unsigned int armJointCount = ARM_JOINT_COUNT_NOT_YET_DEFINED;
	pr2_controllers_msgs::JointTrajectoryGoal goal;

	goal.trajectory.joint_names = getJointNames(inSimulation);
	goal.trajectory.points.resize(trajectory.at(0).size());

	// if the node runs in Gazebo the gripper has to be controlled
	// with the gripper_joint_trajectory_action
	if(inSimulation)
	{
		ROS_INFO("in simulation");

		armJointCount = 5;
	}
	else
	{
		ROS_INFO("On real robot");
		armJointCount = 6;
	}

	goal.trajectory.points.at(0).
		positions.resize(armJointCount);

	goal.trajectory.points.at(0).time_from_start =
			ros::Duration(1.0 / REPRODUCE_HZ * 0 * SLOW_DOWN_FACTOR + (TIME_FROM_START));

	for (unsigned int jointNo = 0; jointNo < armJointCount; ++jointNo)
	{
		goal.trajectory.points.at(0).positions.at(jointNo) =
				trajectory.at(jointNo).at(0);
	}

	// copy the waypoints
	for (unsigned int pointNo = 1; pointNo < trajectory[0].size(); ++pointNo)
	{
		goal.trajectory.points.at(pointNo).
			positions.resize(armJointCount);

		goal.trajectory.points.at(pointNo).time_from_start =
				ros::Duration(1.0 / REPRODUCE_HZ * (pointNo + 1) * SLOW_DOWN_FACTOR + (TIME_FROM_START));

		for (unsigned int jointNo = 0; jointNo < armJointCount; ++jointNo)
		{
			goal.trajectory.points.at(pointNo).positions.at(jointNo) =
					trajectory.at(jointNo).at(pointNo);
		}
	}

	// When to start the trajectory: 5s from now
	goal.trajectory.header.stamp = ros::Time::now() + ros::Duration(6);

	return goal;
}

pr2_controllers_msgs::JointTrajectoryGoal createGripperGoal(const std::deque<std::deque<double> >& trajectory)
{
	pr2_controllers_msgs::JointTrajectoryGoal gripperGoal;

	gripperGoal.trajectory.joint_names.push_back("katana_r_finger_joint");
	gripperGoal.trajectory.joint_names.push_back("katana_l_finger_joint");// = gripperJointNames;
	gripperGoal.trajectory.points.resize(trajectory[0].size());
	//ROS_INFO("gripperJointNames size: %zu, %s, %s", gripperJointNames.size(), gripperJointNames.at(0).c_str(), gripperJointNames.at(0).c_str());
	// copy the waypoints
	for (unsigned int pointNo = 0; pointNo < trajectory[0].size(); ++pointNo)
	{
		gripperGoal.trajectory.points.at(pointNo).positions.resize(GRIPPER_JOINT_COUNT);
		gripperGoal.trajectory.points.at(pointNo).velocities.resize(GRIPPER_JOINT_COUNT);// NEEDED! Gazebo crashes w/o

		gripperGoal.trajectory.points.at(pointNo).time_from_start =
			ros::Duration(1.0 / REPRODUCE_HZ * pointNo + (TIME_FROM_START));

		for (unsigned int jointNo = 0; jointNo < GRIPPER_JOINT_COUNT; ++jointNo)
		{
			gripperGoal.trajectory.points.at(pointNo).positions.at(jointNo) =
				trajectory.at(jointNo + 5).at(pointNo);		// gripper joints are number 5 and 6

			// velocity not needed, so set it to zero
			//gripperGoal.trajectory.points[pointNo].velocities[jointNo] = 0.0;
		}
	}

	// When to start the trajectory: 0.5s from now
	gripperGoal.trajectory.header.stamp = ros::Time::now() + ros::Duration(0.5);

	return gripperGoal;
}

pr2_controllers_msgs::JointTrajectoryGoal createUpdatedGoal(
		const std::deque<std::deque<double> >& newTrajectory,
		const std::deque<std::deque<double> >& oldTrajectory,
		bool inSimulation)
{
	pr2_controllers_msgs::JointTrajectoryGoal goal;
	unsigned int armJointCount = ARM_JOINT_COUNT_NOT_YET_DEFINED;

	goal.trajectory.joint_names = getJointNames(inSimulation);

	// if the node runs in Gazebo the gripper has to be controlled
	// with the gripper_joint_trajectory_action
	if(inSimulation)
	{
		armJointCount = 5;
	}
	else
	{
		armJointCount = 6;
	}

	unsigned int currentStep = getCurrentStepNo();

	unsigned int stepsTillConstraint = getStepsTillConstraint(movedObjectId, currentStep, constraints);

	ROS_INFO("createUpdatedGoal called. CurrentStep: %u stepsTillConstraint: %u", currentStep, stepsTillConstraint);

	if(oldTrajectory[0].size() < currentStep)
	{
		ROS_WARN("The trajectory has ended already!");
		return goal;
	}

	unsigned int newTrajectorySize = oldTrajectory[0].size() - currentStep;
	ROS_INFO("newTrajectorySize: %u, currentStep: %u, oldSize: %zu", newTrajectorySize, currentStep, oldTrajectory[0].size());
	if(stepsTillConstraint >= POINTS_IN_FUTURE)
	{
		ROS_INFO("insert new trajectory after %u steps", POINTS_IN_FUTURE);
		// insert new trajectory after POINTS_IN_FUTURE steps
		newTrajectorySize += TRANSITION_POINT_COUNT; 	// 4 points are added for the transition between old and new trajectory

		goal.trajectory.points.resize(newTrajectorySize);
		ros::spinOnce();
		// transition between old and new trajectory
		for (unsigned int transitionStep = 0; transitionStep < TRANSITION_POINT_COUNT; ++transitionStep)
		{
			goal.trajectory.points.at(transitionStep).positions.resize(armJointCount);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(transitionStep).positions.size(); ++joint)
			{
				if(inSimulation)
				{
					goal.trajectory.points.at(transitionStep).positions.at(joint) =
						oldTrajectory.at(joint).at(currentStep)
							* (1 - TRANSITION_ARRAY[transitionStep])
						+ newTrajectory.at(joint).at(currentStep)
							* TRANSITION_ARRAY[transitionStep];
				}
				else
				{
					goal.trajectory.points.at(transitionStep).positions.at(joint) =
						jointPositions.at(joint) * (1 - TRANSITION_ARRAY[transitionStep])
						+ newTrajectory.at(joint).at(currentStep)
							* TRANSITION_ARRAY[transitionStep];
					//ROS_INFO("joint %u: %f, current: %f", joint, goal.trajectory.points.at(transitionStep).positions.at(joint), jointPositions.at(joint));
				}

				goal.trajectory.points.at(transitionStep).time_from_start =
								ros::Duration(1.0 / REPRODUCE_HZ * transitionStep);
			}
		}

		// copy the waypoints
		for (unsigned int step = TRANSITION_POINT_COUNT; step < goal.trajectory.points.size();	++step)
		{
			goal.trajectory.points.at(step).positions.resize(armJointCount);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(step).positions.size(); ++joint)
			{
				//ROS_INFO("step: %u, joint: %u, newTraIdx: %u, newTraJoint: %zu",
				//		step, joint, currentStep + POINTS_IN_FUTURE + step, newTrajectory.size());
				goal.trajectory.points.at(step).positions.at(joint) =
					newTrajectory.at(joint).at(currentStep + step - TRANSITION_POINT_COUNT);

				goal.trajectory.points.at(step).time_from_start = ros::Duration(1.0 / REPRODUCE_HZ * step);
				//ROS_INFO("joint %u: %f", joint, goal.trajectory.points.at(step).positions.at(joint));
			}
		}
		/*for(unsigned int i = 0; i < 6; ++i)
		{
			ROS_INFO("Joint: %u: %f", i, goal.trajectory.points[0].positions[i]);
		}*/
		goal.trajectory.header.stamp = ros::Time::now()/* + ros::Duration( POINTS_IN_FUTURE / REPRODUCE_HZ)*/;
		ROS_INFO("Trajectory should start at %f", goal.trajectory.header.stamp.toSec());
	}
	else
	{
		ROS_INFO("Insert new trajectory in the past");
		// insert immediately
		newTrajectorySize += TRANSITION_POINT_COUNT; 	// TRANSITION_POINT_COUNT points are added for the
														// transition between old and new trajectory
		const unsigned int POINTS_IN_PAST = 5;
		newTrajectorySize += POINTS_IN_PAST;
		//ROS_INFO("goal size: %u", newTrajectorySize);
		goal.trajectory.points.resize(newTrajectorySize);

		// transition between old and new trajectory
		// same as above except POINTS_IN_FUTURE is missing
		for (unsigned int transitionStep = 0; transitionStep < TRANSITION_POINT_COUNT; ++transitionStep)
		{
			goal.trajectory.points.at(transitionStep).positions.resize(armJointCount);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(transitionStep).positions.size(); ++joint)
			{
				if(inSimulation)
				{
					goal.trajectory.points.at(transitionStep).positions.at(joint) =
						oldTrajectory.at(joint).at(currentStep - POINTS_IN_PAST)
							* (1 - TRANSITION_ARRAY[transitionStep])
						+ newTrajectory.at(joint).at(currentStep - POINTS_IN_PAST)
							* TRANSITION_ARRAY[transitionStep];
				}
				else
				{
					goal.trajectory.points.at(transitionStep).positions.at(joint) =
						jointPositions.at(joint) * (1 - TRANSITION_ARRAY[transitionStep])
						+ newTrajectory.at(joint).at(currentStep - POINTS_IN_PAST)
							* TRANSITION_ARRAY[transitionStep];
				}
				goal.trajectory.points.at(transitionStep).time_from_start =
													ros::Duration(1.0 / REPRODUCE_HZ * transitionStep);
			}
		}
		//ROS_INFO("newTrajectory size %zu", newTrajectory.at(0).size());
		// copy the waypoints
		for (unsigned int step = TRANSITION_POINT_COUNT; step < goal.trajectory.points.size(); ++step)
		{
			goal.trajectory.points.at(step).positions.resize(armJointCount);
			//ROS_INFO("step: %u, currentStep: %u, %u", step, currentStep, currentStep + step - POINTS_IN_PAST - TRANSITION_POINT_COUNT);
			for (unsigned int joint = 0; joint < goal.trajectory.points.at(step).positions.size(); ++joint)
			{
				goal.trajectory.points.at(step).positions.at(joint) =
						newTrajectory.at(joint).at(currentStep + step - POINTS_IN_PAST - TRANSITION_POINT_COUNT);

				goal.trajectory.points.at(step).time_from_start = ros::Duration(1.0 / REPRODUCE_HZ * step);
			}
		}

		goal.trajectory.header.stamp = ros::Time::now();
	}

	return goal;
}

pr2_controllers_msgs::JointTrajectoryGoal createUpdatedGripperGoal(
		const std::deque<std::deque<double> >& newTrajectory,
		const std::deque<std::deque<double> >& oldTrajectory)
{
	ROS_ASSERT_MSG(newTrajectory.size() == oldTrajectory.size(), "trajectories don't have the same dimensions!");
	ROS_ASSERT_MSG(newTrajectory[0].size() == oldTrajectory[0].size(), "trajectories don't have the same length");

	pr2_controllers_msgs::JointTrajectoryGoal goal;
	goal.trajectory.joint_names = gripperJointNames;

	unsigned int currentStep = getCurrentStepNo();
	unsigned int currentStepThinned = currentStep / THINNING_FACTOR;
	unsigned int stepsTillConstraint = getStepsTillConstraint(movedObjectId, currentStep, constraints);

	unsigned int newTrajectorySize = oldTrajectory[0].size() - currentStepThinned;

	if(stepsTillConstraint >= POINTS_IN_FUTURE)
	{
		// insert new trajectory after POINTS_IN_FUTURE steps
		newTrajectorySize -= POINTS_IN_FUTURE;	// the new trajectory starts after POINTS_IN_FUTURE steps
		newTrajectorySize += TRANSITION_POINT_COUNT;
			// TRANSITION_POINT_COUNT points are added for the transition between old and new trajectory

		ROS_INFO("newTrajectorySize: %u, oldTrajectory[0].size(): %zu", newTrajectorySize, oldTrajectory[0].size());
		goal.trajectory.points.resize(newTrajectorySize);

		// transition between old and new trajectory
		for (unsigned int transitionStep = 0; transitionStep < TRANSITION_POINT_COUNT; ++transitionStep)
		{
			goal.trajectory.points.at(transitionStep).positions.resize(GRIPPER_JOINT_COUNT);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(transitionStep).positions.size(); ++joint)
			{
				goal.trajectory.points.at(transitionStep).positions.at(joint) =
					oldTrajectory.at(joint + 5).at(currentStepThinned + POINTS_IN_FUTURE)
						* (1 - TRANSITION_ARRAY[transitionStep])
					+ newTrajectory.at(joint + 5).at(currentStepThinned + POINTS_IN_FUTURE)
						* TRANSITION_ARRAY[transitionStep];

				goal.trajectory.points.at(transitionStep).time_from_start =
								ros::Duration(1.0 / REPRODUCE_HZ * transitionStep);
			}
		}

		// copy the waypoints
		for (unsigned int step = TRANSITION_POINT_COUNT; step < goal.trajectory.points.size(); ++step)
		{
			goal.trajectory.points.at(step).positions.resize(GRIPPER_JOINT_COUNT);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(step).positions.size(); ++joint)
			{
				goal.trajectory.points.at(step).positions.at(joint) =
						newTrajectory.at(joint + 5).at(currentStepThinned + POINTS_IN_FUTURE + step - 4);

				goal.trajectory.points.at(step).time_from_start = ros::Duration(1.0 / REPRODUCE_HZ * step);
			}
		}

		goal.trajectory.header.stamp = ros::Time::now() + ros::Duration( POINTS_IN_FUTURE / REPRODUCE_HZ);
	}
	else
	{
		// insert immediately
		newTrajectorySize += TRANSITION_POINT_COUNT;
			// TRANSITION_POINT_COUNT points are added for the transition between old and new trajectory

		goal.trajectory.points.resize(newTrajectorySize);

		// transition between old and new trajectory
		// same as above except POINTS_IN_FUTURE is missing
		for (unsigned int transitionStep = 0; transitionStep < TRANSITION_POINT_COUNT; ++transitionStep)
		{
			goal.trajectory.points.at(transitionStep).positions.resize(GRIPPER_JOINT_COUNT);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(transitionStep).positions.size(); ++joint)
			{
				goal.trajectory.points.at(transitionStep).positions.at(joint) =
					oldTrajectory.at(joint + 5).at(currentStepThinned) * (1 - TRANSITION_ARRAY[transitionStep])
					+ newTrajectory.at(joint + 5).at(currentStepThinned) * TRANSITION_ARRAY[transitionStep];

				goal.trajectory.points.at(transitionStep).time_from_start =
												ros::Duration(1.0 / REPRODUCE_HZ * transitionStep);
			}
		}

		// copy the waypoints
		for (unsigned int step = TRANSITION_POINT_COUNT; step < goal.trajectory.points.size(); ++step)
		{
			goal.trajectory.points.at(step).positions.resize(GRIPPER_JOINT_COUNT);

			for (unsigned int joint = 0; joint < goal.trajectory.points.at(step).positions.size(); ++joint)
			{
				goal.trajectory.points.at(step).positions.at(joint) =
						newTrajectory.at(joint + 5).at(currentStepThinned + step - 4);

				goal.trajectory.points.at(step).time_from_start = ros::Duration(1.0 / REPRODUCE_HZ * step);
			}
		}

		goal.trajectory.header.stamp = ros::Time::now();
	}

	return goal;
}
double getMaximumDistance(
		const std::deque<std::deque<double> >& newTrajectory,
		const std::deque<std::deque<double> >& oldTrajectory,
		unsigned int startStep,
		unsigned int endStep
		)
{
	ROS_ASSERT_MSG(newTrajectory.size() == oldTrajectory.size(), "trajectories don't have the same dimensions!");
	ROS_ASSERT_MSG(newTrajectory[0].size() == oldTrajectory[0].size(), "trajectories don't have the same length");

	double maximumDistance = 0.0;

	for (unsigned int dimension = 0; dimension < newTrajectory.size(); ++dimension)
	{
		for (unsigned int step = startStep; step <= endStep; ++step)
		{
			double distance = fabs(newTrajectory.at(dimension).at(step) - oldTrajectory.at(dimension).at(step));
			if(distance > maximumDistance)
			{
				maximumDistance = distance;
			}
		}
	}
	return maximumDistance;
}

void waitTillRobotStoped()
{
	bool moving = false;

	while(moving && ros::ok())
	{
		double velSum = 0.0;
		for(unsigned int joint = 0; joint < jointVelocities.size(); ++joint)
		{
			velSum += fabs(jointVelocities.at(joint));
		}

		if(velSum < 0.005)
		{
			moving = true;
		}
		else
		{
			velSum = 0.0;
		}

		ros::spinOnce();
	}
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "lat_reproducer");
	ros::NodeHandle nodeHandle;

	if(argc != (NO_OF_ARGS + 1))	// +1 because argv[0] is the program name
	{
		ROS_ERROR("Not enough arguments provided!");
		printHelpMessage();

		return EXIT_SUCCESS;
	}

	ROS_INFO("lat_reproducer started");

	// flag that determines whether the program runs on gazebo or not
	bool inSimulation = false;

	//////////////////////////////////////////////////////////////////////
	// parse arguments
	// use this topic to receive object tracking data
	std::string objectTrackingTopic(argv[OBJECT_TRACKING_TOPIC_ARG_IDX]);

	// specifies the folder where the trajectories are stored (default user_home)
	trajectoryDir = argv[TRAJECOTRY_DIR_ARG_IDX];

	// reproduce this trajectory
	trajectoryName = argv[TRAJECTORY_NAME_ARG_IDX];
	if(trajectoryName == USE_USER_SELECT_STRING)
	{
		trajectoryName = readTrajectoryFromUser(trajectoryDir);
	}

	if(trajectoryDir == USE_USER_HOME_STRING)
	{
		trajectoryDir = getHomeDir().string();
	}
	else
	{
		if(!boost::filesystem::is_directory(boost::filesystem::path(trajectoryDir)))
		{
			ROS_ERROR("trajectory_dir is not a valid directory. Will use user home dir instead!");
			trajectoryDir = getHomeDir().string();
		}
	}

	// draw the reproduction diagramm or not
	bool drawGraph = true;
	if(strcmp(argv[DRAW_GRAPH_ARG_IDX], "false") == 0)
	{
		drawGraph = false;
	}

	// threshold an object has to move, so that it is recognized
	objectShiftThreshold = atof(argv[OBJECT_SHIFT_THRESHOLD_ARG_IDX]);
	ROS_INFO("Parameter object_shift_threshold = %f", objectShiftThreshold);

	// use this namespace for the trajectory action and so on
	std::string armController(argv[ARM_CONTROLLER_ARG_IDX]);

	// finished parsing arguments
	////////////////////////////////////////////////////////////////////////

	ROS_INFO("Selected trajectory: %s (home dir: %s)", trajectoryName.c_str(), trajectoryDir.c_str());

	// check if this trajectory exists
	if (lfd.leatra_knows_task("/" + trajectoryName, trajectoryDir.c_str()))
	{
		ROS_INFO("Trajectory name known.");

		tfListenerPtr = new tf::TransformListener();

		// subscribe to the topics
		ros::Subscriber jointStateListener = nodeHandle.subscribe("joint_states",
				1,
				jointStateCallback);
		ros::Subscriber objectTrackingSubscriber = nodeHandle.subscribe(objectTrackingTopic, 1, objectTrackerCallback);

		moveRobotToHomePos();

		ROS_INFO("Waiting till all mandatory objects are found");
		while(!allObjectsFound && ros::ok())
		{
			ros::Duration(0.05).sleep();
			ros::spinOnce();
		}

		// check if all mandatory objects are there
		bool allItemsThere = false;
		allItemsThere =
			lfd.mandatory_objects(&objects, "/" + trajectoryName, trajectoryDir.c_str());

		if (allItemsThere)
		{
			ROS_INFO("Found all mandatory objects.");

			//////////////////////////////////////////////////////////////////////
			// compute the trajecotry
			std::deque<std::deque<double> > reproducedTrajectory;

			reproducedTrajectory =
					lfd.reproduce(objects, "/" + trajectoryName, constraints, trajectoryDir.c_str(), true, drawGraph);

			ROS_INFO("Trajectory length: %i", (int)(reproducedTrajectory.size()));
			if(reproducedTrajectory.size() == 0)
			{
				return EXIT_FAILURE;
			}
			currentTrajectory = &reproducedTrajectory;

			/*for (unsigned int i = 0; i < reproducedTrajectory[0].size(); ++i) {
				for (unsigned int j = 0; j < reproducedTrajectory.size(); ++j) {
					std::cout << reproducedTrajectory.at(j).at(i) << "\t";
				}
				std::cout << std::endl;
			}*/

			// now move the arm
			// after the example from
			// www.ros.org/wiki/pr2_controllers/Tutorials/Moving%20the%20arm%20using%20the%20Joint%20Trajectory%20Action
			// armController was read from the launch file
			TrajClient gripperClient(armController + "/gripper_joint_trajectory_action");
			TrajClient trajClient(armController + "/joint_trajectory_action", true);

			trajClient.waitForServer();
			inSimulation = gripperClient.waitForServer(ros::Duration(0.01));
			inSimulationGlob = inSimulation;
			ROS_INFO("Action client ready.");

			pr2_controllers_msgs::JointTrajectoryGoal goal = createGoal(reproducedTrajectory, inSimulation);
			pr2_controllers_msgs::JointTrajectoryGoal gripperGoal = createGripperGoal(reproducedTrajectory);

			if(!trajClient.isServerConnected())
			{
				ROS_ERROR("Action Server is not connected");
			}

			// before starting move the robot to the first position of the trajectory
			moveRobotToStartPos(reproducedTrajectory, inSimulation);

			///////////////////////////////////////////////////
			// Finally start the trajectory!!!!!
			ROS_INFO("Starting now with the trajectory.");
			trajectoryStartTime = goal.trajectory.header.stamp;
			trajClient.sendGoal(goal);

			if(inSimulation)
			{
				gripperClient.sendGoal(gripperGoal);
			}

			std::deque<std::deque<double> > newTrajectory;
			ros::Rate rate(25.0);

			while(!trajClient.getState().isDone() && ros::ok())
			{
				if(recalculateTrajectory)
				{
					ROS_INFO("Recalculation of trajectory triggered.");
					recalculateTrajectory = false;
					recalculationCount++;

					newTrajectory =	lfd.reproduce(
										objects, "/" + trajectoryName, constraints, trajectoryDir.c_str(),
										true, drawGraph);	// useInterim set to true

					currentTrajectory = &newTrajectory;

					ROS_INFO("Recalculation finished");




					if(inSimulation)
					{
						pr2_controllers_msgs::JointTrajectoryGoal updatedGoal =
								createUpdatedGoal(newTrajectory, reproducedTrajectory, inSimulation);
						trajClient.sendGoal(updatedGoal);
						pr2_controllers_msgs::JointTrajectoryGoal updatedGripperGoal =
												createUpdatedGripperGoal(newTrajectory, reproducedTrajectory);

						ROS_INFO("Sending now updated goal");
						gripperClient.sendGoal(updatedGripperGoal);
					}
					else
					{
						ros::Time::sleepUntil(ros::Time::now() + ros::Duration(POINTS_IN_FUTURE / REPRODUCE_HZ));
						trajClient.cancelGoal();

						while(trajClient.getState() == trajClient.getState().ACTIVE)
						{
							ros::spinOnce();
							ros::Duration(0.001).sleep();
						}
						//ros::Duration(0.2).sleep();
						waitTillRobotStoped();
						ros::spinOnce();
						pr2_controllers_msgs::JointTrajectoryGoal updatedGoal =
								createUpdatedGoal(newTrajectory, reproducedTrajectory, inSimulation);
						for(unsigned int i = 0; i < 6; ++i)
						{
							ROS_INFO("Joint: %u: %f", i, updatedGoal.trajectory.points[0].positions[i]);
						}
						//ros::Time::sleepUntil(updatedGoal.trajectory.header.stamp - ros::Duration(0.2));

						ROS_INFO("Sending now updated goal");
						trajClient.sendGoal(updatedGoal);
					}
				}
				//ROS_INFO_THROTTLE(0.05, "step: %u", getCurrentStepNo());
				//ros::Duration(0.001).sleep();
				rate.sleep();
				ros::spinOnce();
			}

			if (inSimulation)
			{
				while(!gripperClient.getState().isDone() && ros::ok())
				{
					ros::Duration(0.001).sleep();
					ros::spinOnce();
				}

				if(gripperClient.getState().state_ == gripperClient.getState().SUCCEEDED)
				{
					ROS_INFO("Gripper trajectory finished successfully.");
				}
				else
				{
					ROS_WARN("Gripper trajectory finished not successfully! (%s)",
							gripperClient.getState().toString().c_str());
				}
			}

			if(trajClient.getState().state_ == trajClient.getState().SUCCEEDED)
			{
				ROS_INFO("Arm trajectory finished successfully.");
			}
			else
			{
				ROS_WARN("Arm trajectory finished not successfully! (%s)",
						trajClient.getState().toString().c_str());
			}

			moveRobotToHomePos();
		}
		else
		{
			ROS_WARN("Mandatory objects missing!");
		}
	}
	else
	{
		ROS_WARN("Trajectory name unknown!");
	}
	delete tfListenerPtr;
	return EXIT_SUCCESS;
}
