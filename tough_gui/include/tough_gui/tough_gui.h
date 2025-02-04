#ifndef VAL_GUI_H
#define VAL_GUI_H

// QT
#include <QMainWindow>
#include <QWidget>
#include <QEvent>
#include <QKeyEvent>
#include <QStatusBar>
#include <QImage>
#include <QPainter>
#include <QLabel>
#include <QTabWidget>
#include <QCheckBox>
#include <QSlider>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QPushButton>

// standard libraries
#include <mutex>

// rviz
#include "rviz/visualization_manager.h"
#include "rviz/render_panel.h"
#include "rviz/display.h"
#include "rviz/panel.h"
#include "rviz/default_plugin/tools/measure_tool.h"
#include "rviz/tool_manager.h"
#include "rviz/default_plugin/tools/point_tool.h"

// ros
#include <ros/ros.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Bool.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/Pose.h>
#include <sensor_msgs/JointState.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/image_encodings.h>

// opencv/PCL
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

// custom Valkyrie APIs
#include <tough_controller_interface/arm_control_interface.h>
#include <tough_controller_interface/chest_control_interface.h>
#include <tough_controller_interface/pelvis_control_interface.h>
#include <tough_controller_interface/head_control_interface.h>
#include <tough_footstep/RobotWalker.h>
#include <tough_controller_interface/gripper_control_interface.h>
#include "tough_moveit_planners/tough_cartesian_planner.h"
#include <tough_controller_interface/wholebody_control_interface.h>
#include "tough_common/robot_state.h"
// Constants
#define TO_RADIANS 3.1415926535f / 180.0f
#define TO_DEGREES 180.0f / 3.1415926535f


#define IMAGE_HEIGHT 544
#define IMAGE_WIDTH 1024

namespace Ui {
class ToughGUI;
}

namespace rviz
{
class Display;
class RenderPanel;
class VisualizationManager;
class ImageDisplay;
class Panel;
}

class ToughGUI : public QMainWindow
{
    Q_OBJECT

public:
    explicit ToughGUI(QWidget *parent = 0);
    virtual ~ToughGUI();


private:
    Ui::ToughGUI *ui;

private:
    void initActionsConnections();
    void initDisplayWidgets();
    void initVariables();
    void initTools();
    void initDefaultValues();
    void initJointLimits();
    void initToughControllers();

    void getArmState();
    void getChestState();
    void getPelvisState();
    void getNeckState();
    void getGripperState();
    void getClickedPoint(const geometry_msgs::PointStamped::Ptr msg);

private Q_SLOTS:
    void keyPressEvent(QKeyEvent *event);

    void setCurrentTool(int btnID);
    void displayPointcloud(int btnID);
    void updateDisplay(int tabID);

    void changePelvisHeight();
    void walkSteps();
    void abortSteps();
    void approveSteps();
    void moveChestJoints();
    void moveHeadJoints();
    void moveArmJoints();
    void moveToPoint();
    void nudgeArm(int btnID);

    void setMode();
    void closeGrippers();
    void openGrippers();
    void closeFingers();
    void openFingers();
    void closeThumb();
    void openThumb();
    void closeBothGrippers();
    void resetGrippers();

    void updateJointStateSub(int tabID);
    void updateGripperSide(int btnID);
    void updateArmSide(int btnID);

    void resetChestOrientation();
    void resetArm();
    void resetRobot();
    void createMoveitDisplay();
    void deleteMoveitDisplay();

    void resetPointcloud();
    void pausePointcloud();



private:
  rviz::VisualizationManager* manager_;
  rviz::VisualizationManager* mapManager_;

  rviz::RenderPanel* renderPanel_;
  rviz::RenderPanel* mapRenderPanel_ ;
  rviz::RenderPanel* imagePanel_;

  rviz::ViewManager* mapViewManager_;
  rviz::ViewController* mapViewController_ ;

  rviz::Display* cloudDisplay_;
//  rviz::Display* imageDisplay_;
  rviz::Display* octomapDisplay_;
  rviz::Display* mapDisplay_ ;
  rviz::Display* footstepMarkersDisplay_;
  rviz::Display* footstepMarkersMainDisplay_;
  rviz::Display* goalDisplay_;
  rviz::Display* moveitDisplay_;

  rviz::ToolManager* toolManager_ ;
  rviz::ToolManager* mapToolManager_ ;

  rviz::Tool* measureTool_ ;
  rviz::Tool* pointTool_ ;
  rviz::Tool* interactTool_;
  rviz::Tool* mapInteractTool_;
  rviz::Tool* setGoalTool_;
  rviz::Tool* setMapGoalTool_;
  rviz::Tool* setInitialPoseTool_;
  rviz::Tool* setMapInitialPoseTool_;

private:
  ros::NodeHandle nh_;
  ros::Publisher approveStepsPub_;

  ros::Publisher reset_pointcloud;
  ros::Publisher pause_pointcloud;
//  ros::Subscriber centerDistSub;
//  ros::Subscriber baseSensorStatus;
  ros::Subscriber rviz2DNavGoalSub;
//  ros::Subscriber jointStateSub_;
  ros::Subscriber clickedPointSub_;
  ros::Timer      jointStatesUpdater_;
  tf::TransformListener listener_;

  image_transport::ImageTransport it_;
  image_transport::Subscriber liveVideoSub;

  ChestControlInterface       *chestController_;
  PelvisControlInterface      *pelvisHeightController_;
  ArmControlInterface   *armJointController_;
  RobotWalker           *walkingController_;
  HeadControlInterface        *headController_;
  GripperControlInterface        *gripperController_;
  WholebodyControlInterface *wholeBodyController_;
  CartesianPlanner      *rightArmPlanner_;
  CartesianPlanner      *leftArmPlanner_;


  RobotDescription      *rd_;
  RobotStateInformer    *currentState_;
  geometry_msgs::Pose   *clickedPoint_;
  bool                  moveArmCommand_;

  std::mutex                      mtx_;
  std::map<std::string, QLabel*>  jointLabelMap_;
  std::map<std::string, double>    jointStateMap_;
  std::vector<std::string> leftArmJointNames_;
  std::vector<std::string> rightArmJointNames_;

  void distanceSubCallback(const std_msgs::Float32::ConstPtr& msg);
  void liveVideoCallback(const sensor_msgs::ImageConstPtr &msg);
  void setVideo(QLabel* label, cv_bridge::CvImagePtr cv_ptr,bool is_RGB);
  void jointStateCallBack(const ros::TimerEvent &e);
  void changeToolButtonStatus(int btnID);

  QString fixedFrame_;
  QString targetFrame_ ;
  QString mapTopic_;
  QString imageTopic_;
  QString pointCloudTopic_;
  QString octomapTopic_;
  QString baseSensorTopic_;
  QString velocityTopic_;
  QString pathTopic_;
  QString robotType_;
  QString goalTopic_;
  QString footstepTopic_;
  QString jointStatesTopic_;
  QString approveStepsTopic_;
  QString resetPointcloudTopic_;
  QString pausePointcloudTopic_;

  bool flipImage_;
  QLabel* status_label_;

  //Step params
  float swingTime_;
  float transferTime_;
  float swingHeight_;

  // joint limits
  float CHEST_ROLL_MAX = 14.61;
  float CHEST_ROLL_MIN = -13;
  float CHEST_PITCH_MAX = 38;
  float CHEST_PITCH_MIN = -7;
  float CHEST_YAW_MAX = 67;
  float CHEST_YAW_MIN = -76;

  float PELVIS_HEIGHT_MAX = 0.95;
  float PELVIS_HEIGHT_MIN = 0.55;

  float RIGHT_SHOULDER_ROLL_MAX = 1.519*TO_DEGREES;
  float RIGHT_SHOULDER_ROLL_MIN = -1.26*TO_DEGREES;
  float RIGHT_SHOULDER_PITCH_MAX = 2.0*TO_DEGREES;
  float RIGHT_SHOULDER_PITCH_MIN = -2.85*TO_DEGREES;
  float RIGHT_SHOULDER_YAW_MAX = 2.18*TO_DEGREES;
  float RIGHT_SHOULDER_YAW_MIN = -3.1*TO_DEGREES;

  float LEFT_SHOULDER_ROLL_MAX = 1.266*TO_DEGREES;
  float LEFT_SHOULDER_ROLL_MIN = -1.519*TO_DEGREES;
  float LEFT_SHOULDER_PITCH_MAX = 2.0*TO_DEGREES;
  float LEFT_SHOULDER_PITCH_MIN = -2.85*TO_DEGREES;
  float LEFT_SHOULDER_YAW_MAX = 2.18*TO_DEGREES;
  float LEFT_SHOULDER_YAW_MIN = -3.1*TO_DEGREES;

  float RIGHT_WRIST_ROLL_MAX = 0.62*TO_DEGREES;
  float RIGHT_WRIST_ROLL_MIN = -0.625*TO_DEGREES;
  float RIGHT_WRIST_PITCH_MAX = 0.36*TO_DEGREES;
  float RIGHT_WRIST_PITCH_MIN = -0.49*TO_DEGREES;
  float RIGHT_WRIST_YAW_MAX = 3.14*TO_DEGREES;
  float RIGHT_WRIST_YAW_MIN = -2.019*TO_DEGREES;

  float LEFT_WRIST_ROLL_MAX = 0.625*TO_DEGREES;
  float LEFT_WRIST_ROLL_MIN = -0.62*TO_DEGREES;
  float LEFT_WRIST_PITCH_MAX = 0.49*TO_DEGREES;
  float LEFT_WRIST_PITCH_MIN = -0.36*TO_DEGREES;
  float LEFT_WRIST_YAW_MAX = 3.14*TO_DEGREES;
  float LEFT_WRIST_YAW_MIN = -2.019*TO_DEGREES;

  float LEFT_ELBOW_MAX = 0.12*TO_DEGREES;
  float LEFT_ELBOW_MIN = -2.174*TO_DEGREES;

  float RIGHT_ELBOW_MAX = 2.174*TO_DEGREES;
  float RIGHT_ELBOW_MIN = -0.12*TO_DEGREES;

  float LOWER_NECK_PITCH_MAX = 66.61;
  float LOWER_NECK_PITCH_MIN = 0;
  float UPPER_NECK_PITCH_MAX = 50.0;
  float UPPER_NECK_PITCH_MIN = -49.9;
  float NECK_YAW_MAX = 60;
  float NECK_YAW_MIN = -60;

  QString PREVIOUS_MODE_LEFT = "BASIC";
  QString PREVIOUS_MODE_RIGHT = "BASIC";

  std::map<QString,GripperControlInterface::GRIPPER_MODES> mode_map;
  std::map<QString,int> prev_mode_map;
  std_msgs::Empty empty_msg;
  std_msgs::Bool bool_msg;
};


#endif // VAL_GUI_H
