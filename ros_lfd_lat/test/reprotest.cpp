// Bring in my package's API, which is what I'm testing
//#include "ros_lfd_lat/lat_reproducer.h"
// Bring in gtest
#include <gtest/gtest.h>

// hack from https://code.google.com/p/googletest/wiki/FAQ#How_do_I_test_a_file_that_defines_main()?
// Renames main() in foo.cc to avoid redefinition by the unit test main()
#define main FooMain
#include "../src/lat_reproducer.cpp"
#undef main

TEST(TestSuite, testObjectUnderConstraint)
{
	int constraintArray[] =
		{-1, -1, -1, 1, 1, 1, -1, -1, 0, 0, 0, 0, -1, -1};

	std::deque<int> constraints = std::deque<int>();

	for (int i = 0; i < 14; ++i)
	{
		constraints.push_back(constraintArray[i]);
	}

	int objectId = 1;
	unsigned int step = 3;

	EXPECT_EQ(true, objectUnderConstraint(objectId, step, constraints)) << "erster Test ungleich true";

	EXPECT_NE(true, objectUnderConstraint(1, 2, constraints));

	objectId = 1;
	step = 5;
	EXPECT_EQ(true, objectUnderConstraint(objectId, step, constraints));

	EXPECT_NE(true, objectUnderConstraint(1, 0, constraints));

	EXPECT_NE(true, objectUnderConstraint(1, 6, constraints));

	EXPECT_NE(true, objectUnderConstraint(1, 8, constraints));

	EXPECT_NE(true, objectUnderConstraint(1, 13, constraints));

	EXPECT_NE(true, objectUnderConstraint(0, 0, constraints));

	EXPECT_NE(true, objectUnderConstraint(0, 6, constraints));

	objectId = 0;
	step = 5;
	EXPECT_NE(true, objectUnderConstraint(objectId, step, constraints));

	objectId = 0;
	step = 11;
	EXPECT_EQ(true, objectUnderConstraint(objectId, step, constraints));
}

TEST(TestSuite, testObjectAfterConstraint)
{
	int constraintArray[] =
		{-1, -1, -1, 1, 1, 1, -1, -1, 0, 0, 0, 0, -1, -1};

	std::deque<int> constraints = std::deque<int>();

	for (int i = 0; i < 14; ++i)
	{
		constraints.push_back(constraintArray[i]);
	}

	int objectId = 1;
	unsigned int step = 3;

	EXPECT_NE(true, objectAfterConstraint(objectId, step, constraints)) << "erster Test ungleich false";

	EXPECT_NE(true, objectAfterConstraint(1, 2, constraints));

	objectId = 1;
	step = 5;
	EXPECT_NE(true, objectAfterConstraint(objectId, step, constraints));

	EXPECT_NE(true, objectAfterConstraint(1, 0, constraints));

	EXPECT_EQ(true, objectAfterConstraint(1, 6, constraints));

	EXPECT_EQ(true, objectAfterConstraint(1, 8, constraints));

	EXPECT_EQ(true, objectAfterConstraint(1, 13, constraints));

	EXPECT_NE(true, objectAfterConstraint(0, 0, constraints));

	EXPECT_NE(true, objectAfterConstraint(0, 6, constraints));

	objectId = 0;
	step = 5;
	EXPECT_NE(true, objectAfterConstraint(objectId, step, constraints));

	objectId = 0;
	step = 11;
	EXPECT_NE(true, objectAfterConstraint(objectId, step, constraints));

	EXPECT_EQ(true, objectAfterConstraint(0, 12, constraints));

	EXPECT_EQ(true, objectAfterConstraint(0, 13, constraints));
}

TEST(TestSuite, testIsObjectReachable)
{
	geometry_msgs::PointStamped objectLocation = geometry_msgs::PointStamped();

	objectLocation.header.frame_id = "/katana_base_link";

	objectLocation.point.x = 1;		// to the front
	objectLocation.point.y = 1;		// to the side
	objectLocation.point.z = 1;		// up

	EXPECT_NE(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0.4;
	objectLocation.point.y = 0;
	objectLocation.point.z = 0.2;

	EXPECT_EQ(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0.4;
	objectLocation.point.y = 0;
	objectLocation.point.z = 0.3;

	EXPECT_EQ(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0;
	objectLocation.point.y = 0;
	objectLocation.point.z = 0.83;

	EXPECT_NE(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0;
	objectLocation.point.y = 0;
	objectLocation.point.z = 0.82;

	EXPECT_EQ(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0.61;
	objectLocation.point.y = 0;
	objectLocation.point.z = 0.22;

	EXPECT_NE(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0.6;
	objectLocation.point.y = 0;
	objectLocation.point.z = 0.22;

	EXPECT_EQ(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0;
	objectLocation.point.y = 0.61;
	objectLocation.point.z = 0.22;

	EXPECT_NE(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0;
	objectLocation.point.y = 0.6;
	objectLocation.point.z = 0.22;

	EXPECT_EQ(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0;
	objectLocation.point.y = -0.61;
	objectLocation.point.z = 0.22;

	EXPECT_NE(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0;
	objectLocation.point.y = -0.6;
	objectLocation.point.z = 0.22;

	EXPECT_EQ(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0.15;
	objectLocation.point.y = 0.0;
	objectLocation.point.z = -0.31;

	EXPECT_NE(true, isObjectReachable(objectLocation));

	objectLocation.point.x = 0.15;
	objectLocation.point.y = 0.0;
	objectLocation.point.z = -0.3;

	EXPECT_EQ(true, isObjectReachable(objectLocation));
}

TEST(TestSuite, testgetStepsTillConstraint)
{
	int constraintArray[] =	{-1, -1, -1, 1, 1, 1, -1, -1, 0, 0, 0, 0, -1, -1};

	std::deque<int> constraints = std::deque<int>();

	for (int i = 0; i < 14; ++i)
	{
		constraints.push_back(constraintArray[i]);
	}

	// object id 1
	EXPECT_EQ(3, getStepsTillConstraint(1, 0, constraints));
	EXPECT_EQ(2, getStepsTillConstraint(1, 1, constraints));
	EXPECT_EQ(1, getStepsTillConstraint(1, 2, constraints));

	EXPECT_EQ(0, getStepsTillConstraint(1, 3, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 4, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 5, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 6, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 7, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 8, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 9, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 10, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 11, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 12, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 13, constraints));

	EXPECT_EQ(0, getStepsTillConstraint(1, 14, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 99, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 4000, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 763356, constraints));

	// object id 0
	EXPECT_EQ(8, getStepsTillConstraint(0, 0, constraints));
	EXPECT_EQ(7, getStepsTillConstraint(0, 1, constraints));
	EXPECT_EQ(6, getStepsTillConstraint(0, 2, constraints));
	EXPECT_EQ(5, getStepsTillConstraint(0, 3, constraints));
	EXPECT_EQ(4, getStepsTillConstraint(0, 4, constraints));
	EXPECT_EQ(3, getStepsTillConstraint(0, 5, constraints));
	EXPECT_EQ(2, getStepsTillConstraint(0, 6, constraints));
	EXPECT_EQ(1, getStepsTillConstraint(0, 7, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(0, 8, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(0, 9, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(0, 10, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(0, 11, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(0, 12, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(0, 13, constraints));

	EXPECT_EQ(0, getStepsTillConstraint(1, 14, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 98, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 4001, constraints));
	EXPECT_EQ(0, getStepsTillConstraint(1, 763756, constraints));
}

TEST(TestSuite, testGetMaximumDistance)
{
	// prepare test data
	std::deque<std::deque<double> > newTrajectory;
	std::deque<std::deque<double> > oldTrajectory;

	double positionArray1[][15] = {
			{ 1.5,  1.6,  1.7,  1.7,  1.8,  1.8,   1.9,  1.8,  1.8,  1.7,  1.65,  1.6,  1.5,  1.5,  1.4},
			{ 1.2,  1.2,  1.2,  1.1,  1.1,  1.0,   1.0,  1.0,  1.1,  1.1,  1.25,  1.2,  1.1,  1.0,  1.0},
			{ 1.5,  1.3,  1.1,  0.9,  0.7,  0.5,   0.3,  0.1, -0.1, -0.2, -0.3,  -0.4, -0.5, -0.6, -0.7},
			{ 1.2,  1.2,  1.2,  1.0,  0.9,  0.75,  0.9,  1.0,  1.1,  1.1,  1.25,  1.2,  1.1,  1.0,  0.9},
			{-1.2, -1.2, -1.2, -1.0, -0.9, -0.75, -0.9, -1.0, -1.1, -1.1, -1.25, -1.2, -1.1, -1.0, -0.9}};

	double positionArray2[][15] = {
			{ 1.9,  2.0,   2.1,  2.1,  2.2,  2.2,   2.3,  2.3,  2.4,  2.4,  2.3,  2.15, 2.0,  1.8,  1.8},
			{ 1.2,  1.2,   1.2,  1.0,  1.0,  0.8,   0.8,  0.9,  1.0,  1.1,  1.2,  1.1,  1.0,  1.0,  0.9},
			{-1.5, -1.25, -1.0, -0.7, -0.5, -0.2,   0.0,  0.1, -0.1, -0.2, -0.3, -0.4, -0.5, -0.5, -0.6},
			{ 1.4,  1.4,   1.3,  1.1,  0.9,  0.75,  0.9,  1.0,  1.1,  1.1,  1.25, 1.2,  1.1,  1.0,  0.9},
			{-1.2, -1.2,  -1.2, -1.0, -0.9, -0.75, -0.9, -0.6, -0.3, -0.0,  0.25, 0.6,  0.8,  1.0,  0.9}};

	for (int i = 0; i < 5; ++i)
	{
		newTrajectory.push_back(std::deque<double>());
		oldTrajectory.push_back(std::deque<double>());

		for (int j = 0; j < 15; ++j)
		{
			newTrajectory[i].push_back(positionArray1[i][j]);
			oldTrajectory[i].push_back(positionArray2[i][j]);
		}
	}

	// perform tests
	ASSERT_DOUBLE_EQ(3, getMaximumDistance(newTrajectory, oldTrajectory, 0, 1));
	ASSERT_DOUBLE_EQ(3, getMaximumDistance(newTrajectory, oldTrajectory, 0, 2));
	ASSERT_DOUBLE_EQ(3, getMaximumDistance(newTrajectory, oldTrajectory, 0, 3));

	ASSERT_DOUBLE_EQ(2.55, getMaximumDistance(newTrajectory, oldTrajectory, 1, 3));
	ASSERT_DOUBLE_EQ(2.55, getMaximumDistance(newTrajectory, oldTrajectory, 1, 4));

	ASSERT_DOUBLE_EQ(2.0, getMaximumDistance(newTrajectory, oldTrajectory, 11, 14));
	ASSERT_DOUBLE_EQ(2.0, getMaximumDistance(newTrajectory, oldTrajectory, 12, 14));
	ASSERT_DOUBLE_EQ(2.0, getMaximumDistance(newTrajectory, oldTrajectory, 13, 14));

	ASSERT_DOUBLE_EQ(1.6, getMaximumDistance(newTrajectory, oldTrajectory, 3, 7));
	ASSERT_DOUBLE_EQ(2.1, getMaximumDistance(newTrajectory, oldTrajectory, 2, 7));
	ASSERT_DOUBLE_EQ(1.8, getMaximumDistance(newTrajectory, oldTrajectory, 7, 11));
}

TEST(TestSuite, testBlaBla)
{

}

// Run all the tests that were declared with TEST()
int main(int argc, char **argv){
testing::InitGoogleTest(&argc, argv);
return RUN_ALL_TESTS();
}
