#include <tough_controller_interface/chest_control_interface.h>
#include <tf/transform_listener.h>

#define TO_RADIANS M_PI / 180.0 //goes probably in utils which stores similar math operation parameters

ChestControlInterface::ChestControlInterface(ros::NodeHandle nh):ToughControllerInterface(nh)
{

    chestTrajPublisher_ =
            nh_.advertise<ihmc_msgs::ChestTrajectoryRosMessage>(control_topic_prefix_ +"/chest_trajectory",1,true);

    homePositionPublisher_ = nh_.advertise<ihmc_msgs::GoHomeRosMessage>(control_topic_prefix_+"/go_home",1,true);

}

ChestControlInterface::~ChestControlInterface()
{
}

void ChestControlInterface::controlChest(float roll , float pitch , float yaw, float time, int execution_mode)
{

    roll  =  roll*TO_RADIANS;
    pitch = pitch*TO_RADIANS;
    yaw   =   yaw*TO_RADIANS;

    tf::Quaternion quatInPelvisFrame;
    quatInPelvisFrame.setRPY(roll,pitch,yaw);
    geometry_msgs::Quaternion quat;
    tf::quaternionTFToMsg(quatInPelvisFrame, quat);

    controlChest(quat, time, execution_mode);
}

void ChestControlInterface::controlChest(geometry_msgs::Quaternion quat, float time, int execution_mode)
{
    ihmc_msgs::ChestTrajectoryRosMessage msg;
    generateMessage(quat, time, execution_mode, msg);

    // publish the message
    chestTrajPublisher_.publish(msg);

}

void ChestControlInterface::executeMessage(const ihmc_msgs::ChestTrajectoryRosMessage &msg)
{
    chestTrajPublisher_.publish(msg);
}

void ChestControlInterface::generateMessage(const geometry_msgs::Quaternion &quat,const float time, const int execution_mode, ihmc_msgs::ChestTrajectoryRosMessage &msg){

    ihmc_msgs::SO3TrajectoryPointRosMessage data;

    data.time = time;
    data.orientation = quat;

    msg.unique_id = ChestControlInterface::id_++;
    msg.execution_mode = execution_mode;

    ihmc_msgs::FrameInformationRosMessage reference_frame;
    reference_frame.trajectory_reference_frame_id = rd_->getPelvisZUPFrameHash();   //Pelvis frame
    reference_frame.data_reference_frame_id = rd_->getPelvisZUPFrameHash();//Pelvis frame

    msg.frame_information = reference_frame;
    msg.taskspace_trajectory_points.push_back(data);

}

void ChestControlInterface::generateMessage(const std::vector<geometry_msgs::Quaternion> &quats, const float time, const int execution_mode, ihmc_msgs::ChestTrajectoryRosMessage &msg)
{
    msg.unique_id = ChestControlInterface::id_++;
    msg.execution_mode = execution_mode;
    double dt = 0;

    ihmc_msgs::FrameInformationRosMessage reference_frame;
    reference_frame.trajectory_reference_frame_id = rd_->getPelvisZUPFrameHash();   //Pelvis frame
    reference_frame.data_reference_frame_id = rd_->getPelvisZUPFrameHash();//Pelvis frame
    msg.frame_information = reference_frame;

    for(size_t i = 0; i < quats.size(); i++){
        ihmc_msgs::SO3TrajectoryPointRosMessage data;
        dt += time/quats.size();
        data.time = dt;
        data.orientation = quats.at(i);
        data.unique_id = id_;
        msg.taskspace_trajectory_points.push_back(data);
    }

}

void ChestControlInterface::generateMessage(const std::vector<geometry_msgs::Quaternion> &quats, const std::vector<float> &timeVec, const int execution_mode, ihmc_msgs::ChestTrajectoryRosMessage &msg)
{
    msg.unique_id = ChestControlInterface::id_++;
    msg.execution_mode = execution_mode;

    ihmc_msgs::FrameInformationRosMessage reference_frame;
    reference_frame.trajectory_reference_frame_id = rd_->getPelvisZUPFrameHash();   //Pelvis frame
    reference_frame.data_reference_frame_id = rd_->getPelvisZUPFrameHash();//Pelvis frame
    msg.frame_information = reference_frame;

    for(size_t i = 0; i < quats.size(); i++){
        ihmc_msgs::SO3TrajectoryPointRosMessage data;
        data.time = timeVec.at(i);
        data.orientation = quats.at(i);
        data.unique_id = id_;
        msg.taskspace_trajectory_points.push_back(data);
    }

}

void ChestControlInterface::getChestOrientation(geometry_msgs::Quaternion &orientation)
{
        geometry_msgs::Pose chest_pose;
        state_informer_->getCurrentPose(rd_->getTorsoFrame(), chest_pose, rd_->getPelvisFrame());
        orientation = chest_pose.orientation;
}

void ChestControlInterface::resetPose(float time)
{
    ihmc_msgs::GoHomeRosMessage go_home;
    go_home.body_part = ihmc_msgs::GoHomeRosMessage::CHEST;
    go_home.trajectory_time = time;
    go_home.unique_id = ChestControlInterface::id_++;
    homePositionPublisher_.publish(go_home);

}

bool ChestControlInterface::getJointSpaceState(std::vector<double> &joints, RobotSide side)
{
    return false;
}

bool ChestControlInterface::getTaskSpaceState(geometry_msgs::Pose &pose, RobotSide side, std::string fixedFrame)
{
    return state_informer_->getCurrentPose(rd_->getTorsoFrame(), pose, fixedFrame);
}
