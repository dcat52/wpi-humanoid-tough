#ifndef CHEST_CONTROL_INTERFACE_H
#define CHEST_CONTROL_INTERFACE_H

#include <ros/ros.h>
#include <ihmc_msgs/ChestTrajectoryRosMessage.h>
#include <ihmc_msgs/GoHomeRosMessage.h>
#include <tf/tf.h>
#include <math.h>
#include <time.h>
#include <geometry_msgs/Quaternion.h>
#include <geometry_msgs/Vector3.h>
#include "tough_common/robot_state.h"
#include "tough_common/robot_description.h"
#include "tough_controller_interface/tough_controller_interface.h"

class ChestControlInterface: public ToughControllerInterface {

private:

    ros::Publisher chestTrajPublisher_;
    ros::Publisher homePositionPublisher_;

public:

    ChestControlInterface(ros::NodeHandle nh);
    ~ChestControlInterface();
    void controlChest(float roll , float pitch , float yaw, float time = 1.0f, int execution_mode=ihmc_msgs::ChestTrajectoryRosMessage::OVERRIDE);
    void controlChest(geometry_msgs::Quaternion quat, float time = 1.0f, int execution_mode=ihmc_msgs::ChestTrajectoryRosMessage::OVERRIDE);
    void executeMessage(const ihmc_msgs::ChestTrajectoryRosMessage &msg);
    void generateMessage(const geometry_msgs::Quaternion &quat,const float time,const int execution_mode, ihmc_msgs::ChestTrajectoryRosMessage &msg);
    void generateMessage(const std::vector<geometry_msgs::Quaternion> &quats,const float time,const int execution_mode, ihmc_msgs::ChestTrajectoryRosMessage &msg);
    void generateMessage(const std::vector<geometry_msgs::Quaternion> &quats,const std::vector<float> &timeVec,const int execution_mode, ihmc_msgs::ChestTrajectoryRosMessage &msg);
    void getChestOrientation(geometry_msgs::Quaternion &orientation);
    void resetPose(float time=0.0f);

    virtual bool getJointSpaceState(std::vector<double> &joints, RobotSide side) override;

    virtual bool getTaskSpaceState(geometry_msgs::Pose &pose, RobotSide side, std::string fixedFrame=TOUGH_COMMON_NAMES::WORLD_TF) override;
    
};

#endif // CHEST_CONTROL_INTERFACE_H
