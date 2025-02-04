#include "tough_gui/tough_gui.h"
#include "ui_tough_gui.h"
#include <iostream>
#include "rviz/view_manager.h"
#include "rviz/tool_manager.h"
#include "rviz/properties/property_tree_model.h"
#include "tough_gui/configurationreader.h"
#include "ros/package.h"
#include <boost/lexical_cast.hpp>
#include <map>

/**
 * This class creates the GUI using rviz APIs.
 */

ToughGUI::ToughGUI(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ToughGUI),it_(nh_)
{
    /**
     * Set up the QT related UI components.
     */
    ui->setupUi(this);

    //set all the controller pointers to null
    chestController_        = nullptr;
    pelvisHeightController_ = nullptr;
    armJointController_     = nullptr;
    walkingController_      = nullptr;
    headController_         = nullptr;
    gripperController_      = nullptr;

    //clickedpoint is used for moving arm in taskspace
    clickedPoint_           = nullptr;

    rd_ = RobotDescription::getRobotDescription(nh_);
    currentState_ = RobotStateInformer::getRobotStateInformer(nh_);

    //initialize everything
    initJointLimits();
    initToughControllers();
    initVariables();
    initDisplayWidgets();
    initTools();
    initActionsConnections();
    initDefaultValues();
}

ToughGUI::~ToughGUI()
{
    delete ui;
    delete mapManager_;
    delete mapRenderPanel_;
    delete manager_;
    delete renderPanel_;
    delete status_label_;
    delete chestController_;
    delete pelvisHeightController_;
    delete armJointController_;
    delete headController_;
    delete gripperController_;
    delete clickedPoint_;
}

void ToughGUI::initVariables()
{
    /**
     *Initialize default values of all the variables. Push these definitions to xml/config file in future
     */

    //Read the configuration file
    std::string configFile = ros::package::getPath("tough_gui") + "/config/config.ini";
    ROS_INFO("config file : %s", configFile.c_str());
    ConfigurationReader configfile(configFile.c_str());

    std::string robot_name;
    nh_.getParam("/ihmc_ros/robot_name", robot_name);

    //Assign topic names to corresponding variables
    fixedFrame_             = QString::fromStdString(configfile.currentTopics["fixedFrame"]);
    mapTopic_               = QString::fromStdString(configfile.currentTopics["mapTopic"]);
    imageTopic_             = QString::fromStdString(configfile.currentTopics["imageTopic"]);
    pointCloudTopic_        = QString::fromStdString(robot_name + "/" + configfile.currentTopics["pointCloudTopic"]);
    octomapTopic_           = QString::fromStdString(robot_name + "/" + configfile.currentTopics["octomapTopic"]);
    baseSensorTopic_        = QString::fromStdString(configfile.currentTopics["baseSensorTopic"]);
    velocityTopic_          = QString::fromStdString(configfile.currentTopics["velocityTopic"]);
    pathTopic_              = QString::fromStdString(configfile.currentTopics["pathTopic"]);
    targetFrame_            = QString::fromStdString(configfile.currentTopics["targetFrame"]);
    robotType_              = QString::fromStdString(configfile.currentTopics["robotType"]);
    goalTopic_              = QString::fromStdString(configfile.currentTopics["goalTopic"]);
    footstepTopic_          = QString::fromStdString(configfile.currentTopics["footstepTopic"]);
    jointStatesTopic_       = QString::fromStdString(configfile.currentTopics["jointStatesTopic"]);
    approveStepsTopic_      = QString::fromStdString(configfile.currentTopics["approveStepsTopic"]);

    resetPointcloudTopic_   = QString::fromStdString( robot_name + "/" + configfile.currentTopics["resetPoincloudTopic"]);
    pausePointcloudTopic_   = QString::fromStdString( robot_name + "/" + configfile.currentTopics["pausePoincloudTopic"]);


    try
    {
        flipImage_        = boost::lexical_cast<bool>(configfile.currentTopics["flip"]);
    }
    catch (const boost::bad_lexical_cast &e)
    {
        std::cerr<<"flip parameter is incorrectly set in config.ini. setting flip to false"<<std::endl;
    }

    //subscribers
    liveVideoSub        = it_.subscribe(imageTopic_.toStdString(),1,&ToughGUI::liveVideoCallback,this,image_transport::TransportHints("raw"));
    jointStatesUpdater_ = nh_.createTimer(ros::Duration(0.5), &ToughGUI::jointStateCallBack, this);
    //    @todo: add timer based callback here to call jointStateCallBack method
    clickedPointSub_    = nh_.subscribe("clicked_point",1, &ToughGUI::getClickedPoint, this);

    approveStepsPub_    = nh_.advertise<std_msgs::Empty>(approveStepsTopic_.toStdString(), 1, true);
    //initialize a onetime map to lookup for joint values
    std::vector<std::string> joints;
    rd_->getLeftArmJointNames(leftArmJointNames_);
    rd_->getRightArmJointNames(rightArmJointNames_);

    std::vector<std::string> chest_neck_joints = {  "torsoYaw", "torsoPitch", "torsoRoll",
                                                    "lowerNeckPitch", "neckYaw", "upperNeckPitch",
                                                    "back_bkz", "back_bky", "back_bkx",
                                                    "neckry", "neckYaw2", "upperNeckPitch2"};

    joints.clear();

    joints.insert(joints.end(), leftArmJointNames_.begin(), leftArmJointNames_.end());
    joints.insert(joints.end(), rightArmJointNames_.begin(), rightArmJointNames_.end());
    joints.insert(joints.end(), chest_neck_joints.begin(), chest_neck_joints.end());
    std::vector<QLabel *> jointLabels = {   ui->lblLeftShoulderPitch, ui->lblLeftShoulderRoll, ui->lblLeftShoulderYaw, 
                                            ui->lblLeftElbowPitch, ui->lblLeftForearmYaw, ui->lblLeftWristRoll, ui->lblLeftWristPitch,
                                            ui->lblRightShoulderPitch, ui->lblRightShoulderRoll, ui->lblRightShoulderYaw, 
                                            ui->lblRightElbowPitch, ui->lblRightForearmYaw, ui->lblRightWristRoll, 
                                            ui->lblRightWristPitch, ui->lblChestYaw, ui->lblChestPitch, 
                                            ui->lblChestRoll, ui->lblLowerNeckPitch, ui->lblNeckYaw, 
                                            ui->lblNeckUpperPitch, ui->lblChestYaw, ui->lblChestPitch, 
                                            ui->lblChestRoll, ui->lblLowerNeckPitch, ui->lblNeckYaw, 
                                            ui->lblNeckUpperPitch};

    assert(joints.size() == jointLabels.size() && "joints and jointlabels must be of same size");

    for(size_t i = 0; i< joints.size(); ++i)
    {
        jointLabelMap_[joints[i]] = jointLabels[i];
    }

    //moveArmCommand is a flag used to check if user intends to move arm or just publish a point
    moveArmCommand_ = false;

    mode_map = {    {"BASIC",   GripperControlInterface::GRIPPER_MODES::BASIC},
                    {"PINCH",   GripperControlInterface::GRIPPER_MODES::PINCH},
                    {"WIDE",    GripperControlInterface::GRIPPER_MODES::WIDE},
                    {"SCISSOR", GripperControlInterface::GRIPPER_MODES::SCISSOR}
               };
    
    prev_mode_map = {   {"BASIC",   0},
                        {"PINCH",   1},
                        {"WIDE",    2},
                        {"SCISSOR", 3}
                    };

    reset_pointcloud = nh_.advertise<std_msgs::Empty>(resetPointcloudTopic_.toStdString(), 1, true);
    pause_pointcloud = nh_.advertise<std_msgs::Bool>(pausePointcloudTopic_.toStdString(), 1, true);
    bool_msg.data = false;
}

void ToughGUI::initActionsConnections()
{
    /**
     * Set up the status Bar and display messages emitted from each of the tools.
     * All the tools in rviz API has updateStatus function to emit messages to the status bar.
     */
    status_label_ = new QLabel("");
    statusBar()->addPermanentWidget( status_label_,1);
    connect( manager_, SIGNAL( statusUpdate( const QString& )), status_label_, SLOT( setText( const QString& )));

    /**
     * Setup Signals and slots for different buttons/sliders in UI.
     */
    // Tool and display selection
    connect(ui->btnGroupRvizTools,       SIGNAL(buttonClicked(int)),        this, SLOT(setCurrentTool(int)));
    connect(ui->btnGroupDisplays,        SIGNAL(buttonClicked(int)),        this, SLOT(displayPointcloud(int)));
    connect(ui->controlTabs,             SIGNAL(currentChanged(int)),       this, SLOT(updateJointStateSub(int)));
    connect(ui->tab_display,             SIGNAL(currentChanged(int)),       this, SLOT(updateDisplay(int)));

    //pointCloud
    connect(ui->btnResetPointcloud,      SIGNAL(clicked()),                 this, SLOT(resetPointcloud()));
    connect(ui->btnPausePointcloud,      SIGNAL(clicked()),                 this, SLOT(pausePointcloud()));

    //nudge
    connect(ui->btnMoveToPoint,          SIGNAL(clicked()),                 this, SLOT(moveToPoint()));
    connect(ui->btnGroupNudge,           SIGNAL(buttonClicked(int)),        this, SLOT(nudgeArm(int)));

    //arm control
    connect(ui->btnGroupArm,             SIGNAL(buttonClicked(int)),        this, SLOT(updateArmSide(int)));
    connect(ui->sliderShoulderRoll,      SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->sliderShoulderPitch,     SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->sliderShoulderYaw,       SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->sliderWristRoll,         SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->sliderWristPitch,        SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->sliderWristYaw,          SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->sliderElbow,             SIGNAL(sliderReleased()),          this, SLOT(moveArmJoints()));
    connect(ui->btnResetArm,             SIGNAL(clicked()),                 this, SLOT(resetArm()));

    //gripper control
    connect(ui->btnGroupGripper,         SIGNAL(buttonClicked(int)),        this, SLOT(updateGripperSide(int)));
    connect(ui->cmbBoxGripMode,          SIGNAL(currentIndexChanged(int)),  this, SLOT(setMode()));
    connect(ui->btnResetGrippers,        SIGNAL(clicked()),                 this, SLOT(resetGrippers()));
    connect(ui->btnCloseBothHands,       SIGNAL(clicked()),                 this, SLOT(closeBothGrippers()));
    connect(ui->btnCloseHand,            SIGNAL(clicked()),                 this, SLOT(closeGrippers()));
    connect(ui->btnOpenHand,             SIGNAL(clicked()),                 this, SLOT(openGrippers()));
    connect(ui->btnCloseFingers,         SIGNAL(clicked()),                 this, SLOT(closeFingers()));
    connect(ui->btnOpenFingers,          SIGNAL(clicked()),                 this, SLOT(openFingers()));
    connect(ui->btnCloseThumb,           SIGNAL(clicked()),                 this, SLOT(closeThumb()));
    connect(ui->btnOpenThumb,            SIGNAL(clicked()),                 this, SLOT(openThumb()));

    // chest control
    connect(ui->sliderChestRoll,         SIGNAL(sliderReleased()),          this, SLOT(moveChestJoints()));
    connect(ui->sliderChestPitch,        SIGNAL(sliderReleased()),          this, SLOT(moveChestJoints()));
    connect(ui->sliderChestYaw,          SIGNAL(sliderReleased()),          this, SLOT(moveChestJoints()));
    connect(ui->btnChestReset,           SIGNAL(clicked()),                 this, SLOT(resetChestOrientation()));

    // neck control
    connect(ui->sliderUpperNeckPitch,    SIGNAL(sliderReleased()),          this, SLOT(moveHeadJoints()));
    connect(ui->sliderLowerNeckPitch,    SIGNAL(sliderReleased()),          this, SLOT(moveHeadJoints()));
    connect(ui->sliderNeckYaw,           SIGNAL(sliderReleased()),          this, SLOT(moveHeadJoints()));

    //walk
    connect(ui->btnWalk,                 SIGNAL(clicked()),                 this, SLOT(walkSteps()));
    connect(ui->sliderPelvisHeight,      SIGNAL(sliderReleased()),          this, SLOT(changePelvisHeight()));
    connect(ui->btnApproveSteps,         SIGNAL(clicked()),                 this, SLOT(approveSteps()));
    connect(ui->btnAbortWalk,            SIGNAL(clicked()),                 this, SLOT(abortSteps()));

    //reset robot
    connect(ui->btnResetRobot,           SIGNAL(clicked()),                 this, SLOT(resetRobot()));

}

void ToughGUI::initDisplayWidgets()
{

    //Setup the UI elements for displaying 2D map
    /**
     * VisualizationManager is used to control different displays that are shown in a widget.
     * Renderpanel is a widget that provides a 3D space in the visualizationmanager.
     * startUpdate() function starts the timers and subscribes to defined topics at 30Hz.
     */
    mapRenderPanel_ = new rviz::RenderPanel();
    ui->map_layout->addWidget(mapRenderPanel_);
    mapManager_     = new rviz::VisualizationManager( mapRenderPanel_ );
    mapRenderPanel_->initialize( mapManager_->getSceneManager(), mapManager_);
    mapManager_->setFixedFrame(fixedFrame_);
    mapManager_->initialize();
    mapManager_->startUpdate();

    //Create and assign FixedOrientationOrthoViewController to the existing viewmanager of the visualization manager
    /**
     * VisualisationManager has a manager for most of its children. ViewManager is responsible for setting the viewController.
     * Default View Controller is rviz/Orbit, for map we are changing it to rviz/TopDownOrtho
     * To set properties of most of the rviz objects, use subProp and setValue functions as shown below
     * New displays can be created and added to the visualization manager using createDisplay function as used below
     *
     * @todo Create an xml/config file to define objects to be displayed in GUI alongwith their parameters
     */

    mapViewManager_     = mapManager_->getViewManager();
    mapViewManager_->setCurrentViewControllerType("rviz/TopDownOrtho");

    mapViewController_  = mapViewManager_->getCurrent();

    //Set parameters of the view controller to show map correctly
    mapViewController_->subProp("X")->setValue(4.52);
    mapViewController_->subProp("Y")->setValue(0);
    mapViewController_->subProp("Angle")->setValue(0);
    mapViewController_->subProp("Scale")->setValue(100);

    // Create a map display
    mapDisplay_ = mapManager_->createDisplay( "rviz/Map", "2D Map view", true );
    ROS_ASSERT( mapDisplay_ != NULL );

    mapDisplay_->subProp( "Topic" )->setValue( mapTopic_ );

    QString robotModelTopic = QString::fromStdString(rd_->getRobotName() + "/robot_description");
    mapManager_->createDisplay( "rviz/RobotModel", robotType_, true )->subProp( "Robot Description" )->setValue(robotModelTopic);


    mapManager_->createDisplay("rviz/Path","Global path",true)->subProp( "Topic" )->setValue(pathTopic_);
    mapManager_->createDisplay( "rviz/Grid", "Grid", true );
    mapManager_->createDisplay("rviz/MarkerArray", "Footstep markers", true)->subProp("Marker Topic")->setValue(footstepTopic_);

    // Initialize GUI elements for main panel
    renderPanel_ = new rviz::RenderPanel();
    ui->display3d_layout->addWidget(renderPanel_);

    manager_ = new rviz::VisualizationManager( renderPanel_ );
    renderPanel_->initialize( manager_->getSceneManager(), manager_ );

    //set the fixed frame before initializing Visualization Manager. pointcloud2 will not work with this
    manager_->setFixedFrame(fixedFrame_);
    manager_->initialize();
    manager_->startUpdate();

    // Create a main display to show pointcloud and octomap
    manager_->createDisplay( "rviz/Grid", "Grid", true );
    manager_->createDisplay( "rviz/RobotModel", robotType_, true )->subProp( "Robot Description" )->setValue(robotModelTopic);

    cloudDisplay_ = manager_->createDisplay( "rviz/PointCloud2", "3D Pointcloud view", false );
    assert( cloudDisplay_ != NULL && "Could not create a display");

    cloudDisplay_->subProp("Topic")->setValue(pointCloudTopic_);
    cloudDisplay_->subProp("Selectable")->setValue("true");
    cloudDisplay_->subProp("Style")->setValue("Boxes");
    cloudDisplay_->subProp("Alpha")->setValue(0.5);
    cloudDisplay_->subProp("Color Transformer")->setValue("AxisColor");

    octomapDisplay_ = manager_->createDisplay( "rviz/MarkerArray", "Octomap view", false );
    ROS_ASSERT( octomapDisplay_ != NULL );

    octomapDisplay_->subProp("Marker Topic")->setValue(octomapTopic_);

    footstepMarkersDisplay_ = manager_->createDisplay("rviz/MarkerArray", "Footsteps", true);
    ROS_ASSERT(footstepMarkersDisplay_ != NULL);

    footstepMarkersDisplay_->subProp("Marker Topic")->setValue(footstepTopic_);
    footstepMarkersDisplay_->setEnabled(true);

    //Assign Target Frame to the existing viewmanager of the visualization manager
    rviz::ViewManager*    viewManager_    = manager_->getViewManager();
    rviz::ViewController* viewController_ = viewManager_->getCurrent();
    viewController_->subProp("Target Frame")->setValue(targetFrame_);
    manager_->createDisplay("rviz/Path","Global path",true)->subProp( "Topic" )->setValue(pathTopic_);

    ROS_INFO("Footstep Topic : %s", footstepTopic_.toStdString().c_str());
    footstepMarkersDisplay_ = mapManager_->createDisplay("rviz/MarkerArray", "Footsteps", true);
    footstepMarkersDisplay_->subProp("Marker Topic")->setValue(footstepTopic_);
    footstepMarkersDisplay_->subProp("Queue Size")->setValue("100");

    footstepMarkersMainDisplay_ = manager_->createDisplay("rviz/MarkerArray", "Footsteps", true);
    footstepMarkersMainDisplay_->subProp("Marker Topic")->setValue(footstepTopic_);
    footstepMarkersMainDisplay_->subProp("Queue Size")->setValue("100");


    //    footstepMarkersDisplay_->subProp("Marker Topic")->setValue("/footstep_planner/footsteps_array");

    //    footstepMarkersDisplay_->subProp("Namespaces")->setValue("valkyrie");
    //    footstepMarkersDisplay_ = manager_->createDisplay("rviz/MarkerArray", "Footsteps", true);
    //    ROS_ASSERT(footstepMarkersDisplay_ != NULL);

    //    footstepMarkersDisplay_->subProp("Marker Topic")->setValue(footstepTopic_);
    //    footstepMarkersDisplay_->subProp("Queue Size")->setValue("100");
    //    footstepMarkersDisplay_->subProp("Namespaces")->setValue("valkyrie");

    //    ui->sliderLowerNeckPitch->setEnabled(false);

    QString imagePath = QString::fromStdString(ros::package::getPath("tough_gui") + "/resources/coordinates.png");
    QImage qImage(imagePath);
    ui->lblAxes->setPixmap(QPixmap::fromImage(qImage));
    moveitDisplay_ = nullptr;
}

// This doesn';t work as Moveit loads a panel which ends up with seg fault :(
// I'll fix it some day. Until then we will try a different solution
void ToughGUI::createMoveitDisplay(){
    moveitDisplay_ = manager_->createDisplay( "moveit_rviz_plugin/MotionPlanning", "MoveIt",false );
    moveitDisplay_->subProp("Planning Request")->subProp("Planning Group")->setValue("leftMiddleFingerGroup");
    moveitDisplay_->subProp("Planning Request")->subProp("Interactive Marker Size")->setValue("0.2");
    moveitDisplay_->setEnabled(true);
}

void ToughGUI::deleteMoveitDisplay(){
    rviz::Display* tempDisplay =  moveitDisplay_;
    moveitDisplay_ = nullptr;
    delete tempDisplay;
}


void ToughGUI::initTools(){
    /**
     * ToolManager is similar to ViewManager. It can be used to add new tools and change the current or default tool.
     * Properties of tools are stored in a PropertyTreeModel. To set/modify any property of a tool use getPropertyContainer function.
     */
    toolManager_           = manager_->getToolManager();
    pointTool_             = toolManager_->addTool("rviz/PublishPoint");
    measureTool_           = toolManager_->addTool("rviz/Measure");
    setGoalTool_           = toolManager_->addTool("rviz/SetGoal");
    setInitialPoseTool_    = toolManager_->addTool("rviz/SetInitialPose");
    interactTool_          = toolManager_->addTool("rviz/Interact");
    mapToolManager_        = mapManager_->getToolManager();
    mapInteractTool_       = mapToolManager_->addTool("rviz/Interact");
    setMapGoalTool_        = mapToolManager_->addTool("rviz/SetGoal");
    setMapInitialPoseTool_ = mapToolManager_->addTool("rviz/SetInitialPose");

    // Find the entry in propertytreemodel and set the value for Topic
    setGoalTool_->getPropertyContainer()->subProp("Topic")->setValue(goalTopic_);
    setMapGoalTool_->getPropertyContainer()->subProp("Topic")->setValue(goalTopic_);

    //set the initial rviz tool to be "interact"
    changeToolButtonStatus(-2);

}

void ToughGUI::initJointLimits() {
    std::vector<std::pair<double, double> > left_arm_joint_limits;
    std::vector<std::pair<double, double> > right_arm_joint_limits;

    rd_->getLeftArmJointLimits(left_arm_joint_limits);
    rd_->getRightArmJointLimits(right_arm_joint_limits);
    // reduce the joint limits by 1cm to avoid excceeding limits at higher precision of float
    for (size_t i = 0; i < left_arm_joint_limits.size(); i++ ){
        left_arm_joint_limits[i] = {left_arm_joint_limits[i].first + 0.01, left_arm_joint_limits[i].second - 0.01};
        right_arm_joint_limits[i] = {right_arm_joint_limits[i].first + 0.01, right_arm_joint_limits[i].second - 0.01};
    }

    RIGHT_SHOULDER_PITCH_MAX= right_arm_joint_limits[0].second*TO_DEGREES;
    RIGHT_SHOULDER_PITCH_MIN= right_arm_joint_limits[0].first*TO_DEGREES;

    RIGHT_SHOULDER_ROLL_MAX = right_arm_joint_limits[1].second*TO_DEGREES;
    RIGHT_SHOULDER_ROLL_MIN = right_arm_joint_limits[1].first*TO_DEGREES;

    RIGHT_SHOULDER_YAW_MAX  = right_arm_joint_limits[2].second*TO_DEGREES;
    RIGHT_SHOULDER_YAW_MIN  = right_arm_joint_limits[2].first*TO_DEGREES;

    RIGHT_ELBOW_MAX         = right_arm_joint_limits[3].second*TO_DEGREES;
    RIGHT_ELBOW_MIN         = right_arm_joint_limits[3].first*TO_DEGREES;

    RIGHT_WRIST_YAW_MAX     = right_arm_joint_limits[4].second*TO_DEGREES;
    RIGHT_WRIST_YAW_MIN     = right_arm_joint_limits[4].first*TO_DEGREES;

    RIGHT_WRIST_ROLL_MAX    = right_arm_joint_limits[5].second*TO_DEGREES;
    RIGHT_WRIST_ROLL_MIN    = right_arm_joint_limits[5].first*TO_DEGREES;

    RIGHT_WRIST_PITCH_MAX   = right_arm_joint_limits[6].second*TO_DEGREES;
    RIGHT_WRIST_PITCH_MIN   = right_arm_joint_limits[6].first*TO_DEGREES;


    LEFT_SHOULDER_PITCH_MAX = left_arm_joint_limits[0].second*TO_DEGREES;
    LEFT_SHOULDER_PITCH_MIN = left_arm_joint_limits[0].first*TO_DEGREES;

    LEFT_SHOULDER_ROLL_MAX  = left_arm_joint_limits[1].second*TO_DEGREES;
    LEFT_SHOULDER_ROLL_MIN  = left_arm_joint_limits[1].first*TO_DEGREES;

    LEFT_SHOULDER_YAW_MAX   = left_arm_joint_limits[2].second*TO_DEGREES;
    LEFT_SHOULDER_YAW_MIN   = left_arm_joint_limits[2].first*TO_DEGREES;

    LEFT_ELBOW_MAX          = left_arm_joint_limits[3].second*TO_DEGREES;
    LEFT_ELBOW_MIN          = left_arm_joint_limits[3].first*TO_DEGREES;

    LEFT_WRIST_YAW_MAX      = left_arm_joint_limits[4].second*TO_DEGREES;
    LEFT_WRIST_YAW_MIN      = left_arm_joint_limits[4].first*TO_DEGREES;

    LEFT_WRIST_ROLL_MAX     = left_arm_joint_limits[5].second*TO_DEGREES;
    LEFT_WRIST_ROLL_MIN     = left_arm_joint_limits[5].first*TO_DEGREES;

    LEFT_WRIST_PITCH_MAX    = left_arm_joint_limits[6].second*TO_DEGREES;
    LEFT_WRIST_PITCH_MIN    = left_arm_joint_limits[6].first*TO_DEGREES;

}

void ToughGUI::initDefaultValues() {
    // 3D view. select Pointcloud by default
    ui->radioBtnPointcloud->setEnabled(true);
    ui->radioBtnPointcloud->setChecked(true);
    octomapDisplay_->setEnabled(false);
    cloudDisplay_->setEnabled(true);

    //Arms select left arm by default
    ui->radioArmSideLeft->setChecked(true);
    ui->radioNudgeSideLeft->setChecked(true);

    //Gripper select left Gripper by default
    ui->radioGripSideLeft->setChecked(true);

    //Set the default values for the offset
    ui->lineEditNumSteps->setText("2");
    ui->lineEditXOffset->setText("0.3");
    ui->lineEditYOffset->setText("0.0");
    ui->lineEditSwingTime->setText(QString::number(swingTime_));
    ui->lineEditTransferTime->setText(QString::number(transferTime_));
    ui->lineEditSwingHeight->setText(QString::number(swingHeight_));

    //check the right foot start button
    ui->radioRightFoot->setChecked(true);

    //query current state of robot and update sliders accordingly
    //Chest control . Replace these defaults with actual values from robot
    float zeroRoll  = fabs(CHEST_ROLL_MIN/((CHEST_ROLL_MAX - CHEST_ROLL_MIN)/100.0));
    float zeroPitch = fabs(CHEST_PITCH_MIN/((CHEST_PITCH_MAX - CHEST_PITCH_MIN)/100.0));
    float zeroYaw   = fabs(CHEST_YAW_MIN/((CHEST_YAW_MAX - CHEST_YAW_MIN)/100.0));
    ui->sliderChestRoll->setValue(zeroRoll);
    ui->sliderChestPitch->setValue(zeroPitch);
    ui->sliderChestYaw->setValue(zeroYaw);

    //Neck control . Replace these defaults with actual values from robot
    float zeroUpperPitch = fabs(UPPER_NECK_PITCH_MIN/((UPPER_NECK_PITCH_MAX - UPPER_NECK_PITCH_MIN)/100.0));
    float zeroLowerPitch = fabs(LOWER_NECK_PITCH_MIN/((LOWER_NECK_PITCH_MAX - LOWER_NECK_PITCH_MIN)/100.0));
    zeroYaw   = fabs(NECK_YAW_MIN/((NECK_YAW_MAX - NECK_YAW_MIN)/100.0));
    ui->sliderUpperNeckPitch->setValue(zeroUpperPitch);
    ui->sliderLowerNeckPitch->setValue(zeroLowerPitch);
    ui->sliderNeckYaw->setValue(zeroYaw);

    //PelvisHeight . Replace these defaults with actual values from robot
    float defaultPelvisHeight = (0.9 - PELVIS_HEIGHT_MIN) *100/ (PELVIS_HEIGHT_MAX - PELVIS_HEIGHT_MIN);
    ui->sliderPelvisHeight->setValue(defaultPelvisHeight);
    //configure arm sliders
    getArmState();

    //    jointStateSub_.shutdown();
}

void ToughGUI::initToughControllers() {

    //create a chest trajectory controller object
    chestController_ = new ChestControlInterface(nh_);

    //create pelvis height controller object
    pelvisHeightController_ = new PelvisControlInterface(nh_);

    //create walking controller object
    swingTime_ = 1.0f;
    transferTime_ = 1.0f;
    swingHeight_ = 0.18f;
    walkingController_ = new RobotWalker(nh_, transferTime_, swingTime_, 0, swingHeight_);

    //create arm joint controller object
    armJointController_ = new ArmControlInterface(nh_);

    //create a chest trajectory controller object
    headController_ = new HeadControlInterface(nh_);

    //create a gripper controller object
    gripperController_ = new GripperControlInterface(nh_);

    // create a wholebody controller object
    wholeBodyController_ = new WholebodyControlInterface(nh_);

    // right arm cartesian planner
    //    rightArmPlanner_ = new CartesianPlanner(TOUGH_COMMON_NAMES::RIGHT_ENDEFFECTOR_GROUP, TOUGH_COMMON_NAMES::WORLD_TF);

    // left arm cartesian planner
    //    leftArmPlanner_ = new CartesianPlanner(TOUGH_COMMON_NAMES::LEFT_ENDEFFECTOR_GROUP, TOUGH_COMMON_NAMES::WORLD_TF);
}

void ToughGUI::resetPointcloud()
{
    reset_pointcloud.publish(empty_msg);
}

void ToughGUI::pausePointcloud()
{
    pause_pointcloud.publish(bool_msg);
    bool_msg.data = !bool_msg.data;
    ui->btnPausePointcloud->setFlat(bool_msg.data);
}
void ToughGUI::getArmState()
{
    if(jointStateMap_.empty()){
        ros::TimerEvent e;
        this->jointStateCallBack(e);
    }
    RobotSide side = ui->radioArmSideLeft->isChecked() ? LEFT : RIGHT;
    mtx_.lock();

    if(side == LEFT) {
        double leftShoulderPitchJointVal = jointStateMap_[leftArmJointNames_.at(0)] * TO_DEGREES;
        leftShoulderPitchJointVal = ((leftShoulderPitchJointVal - LEFT_SHOULDER_PITCH_MIN )* 100)/(LEFT_SHOULDER_PITCH_MAX-LEFT_SHOULDER_PITCH_MIN);
        ui->sliderShoulderPitch->setValue(leftShoulderPitchJointVal);

        double leftShoulderRollJointVal = jointStateMap_[leftArmJointNames_.at(1)] * TO_DEGREES;
        leftShoulderRollJointVal = ((leftShoulderRollJointVal - LEFT_SHOULDER_ROLL_MIN )* 100)/(LEFT_SHOULDER_ROLL_MAX-LEFT_SHOULDER_ROLL_MIN);
        ui->sliderShoulderRoll->setValue(leftShoulderRollJointVal);

        double leftShoulderYawJointVal = jointStateMap_[leftArmJointNames_.at(2)] * TO_DEGREES;
        leftShoulderYawJointVal = ((leftShoulderYawJointVal - LEFT_SHOULDER_YAW_MIN )* 100)/(LEFT_SHOULDER_YAW_MAX-LEFT_SHOULDER_YAW_MIN);
        ui->sliderShoulderYaw->setValue(leftShoulderYawJointVal);

        double leftElbowPitchJointVal = jointStateMap_[leftArmJointNames_.at(3)] * TO_DEGREES;
        leftElbowPitchJointVal = ((leftElbowPitchJointVal - LEFT_ELBOW_MIN )* 100)/(LEFT_ELBOW_MAX-LEFT_ELBOW_MIN);
        ui->sliderElbow->setValue(leftElbowPitchJointVal);

        double leftForearmYawJointVal = jointStateMap_[leftArmJointNames_.at(4)] * TO_DEGREES;
        leftForearmYawJointVal = ((leftForearmYawJointVal - LEFT_WRIST_YAW_MIN )* 100)/(LEFT_WRIST_YAW_MAX-LEFT_WRIST_YAW_MIN);
        ui->sliderWristYaw->setValue(leftForearmYawJointVal);

        double leftWristRollJointVal = jointStateMap_[leftArmJointNames_.at(5)] * TO_DEGREES;
        leftWristRollJointVal = ((leftWristRollJointVal - LEFT_WRIST_ROLL_MIN )* 100)/(LEFT_WRIST_ROLL_MAX-LEFT_WRIST_ROLL_MIN);
        ui->sliderWristRoll->setValue(leftWristRollJointVal);

        double leftWristPitchJointVal = jointStateMap_[leftArmJointNames_.at(6)] * TO_DEGREES;
        leftWristPitchJointVal = ((leftWristPitchJointVal - LEFT_WRIST_PITCH_MIN )* 100)/(LEFT_WRIST_PITCH_MAX-LEFT_WRIST_PITCH_MIN);
        ui->sliderWristPitch->setValue(leftWristPitchJointVal);
    }
    else{

        double rightShoulderPitchJointVal = jointStateMap_[rightArmJointNames_.at(0)] * TO_DEGREES;
        rightShoulderPitchJointVal = ((rightShoulderPitchJointVal - RIGHT_SHOULDER_PITCH_MIN )* 100)/(RIGHT_SHOULDER_PITCH_MAX-RIGHT_SHOULDER_PITCH_MIN);
        ui->sliderShoulderPitch->setValue(rightShoulderPitchJointVal);

        double rightShoulderRollJointVal = jointStateMap_[rightArmJointNames_.at(1)] * TO_DEGREES;
        rightShoulderRollJointVal = ((rightShoulderRollJointVal - RIGHT_SHOULDER_ROLL_MIN )* 100)/(RIGHT_SHOULDER_ROLL_MAX-RIGHT_SHOULDER_ROLL_MIN);
        ui->sliderShoulderRoll->setValue(rightShoulderRollJointVal);

        double rightShoulderYawJointVal = jointStateMap_[rightArmJointNames_.at(2)] * TO_DEGREES;
        rightShoulderYawJointVal = ((rightShoulderYawJointVal - RIGHT_SHOULDER_YAW_MIN )* 100)/(RIGHT_SHOULDER_YAW_MAX-RIGHT_SHOULDER_YAW_MIN);
        ui->sliderShoulderYaw->setValue(rightShoulderYawJointVal);

        double rightElbowPitchJointVal = jointStateMap_[rightArmJointNames_.at(3)] * TO_DEGREES;
        rightElbowPitchJointVal = ((rightElbowPitchJointVal - RIGHT_ELBOW_MIN )* 100)/(RIGHT_ELBOW_MAX-RIGHT_ELBOW_MIN);
        ui->sliderElbow->setValue(rightElbowPitchJointVal);

        double rightForearmYawJointVal = jointStateMap_[rightArmJointNames_.at(4)] * TO_DEGREES;
        rightForearmYawJointVal = ((rightForearmYawJointVal - RIGHT_WRIST_YAW_MIN )* 100)/(RIGHT_WRIST_YAW_MAX-RIGHT_WRIST_YAW_MIN);
        ui->sliderWristYaw->setValue(rightForearmYawJointVal);

        double rightWristRollJointVal = jointStateMap_[rightArmJointNames_.at(5)] * TO_DEGREES;
        rightWristRollJointVal = ((rightWristRollJointVal - RIGHT_WRIST_ROLL_MIN )* 100)/(RIGHT_WRIST_ROLL_MAX-RIGHT_WRIST_ROLL_MIN);
        ui->sliderWristRoll->setValue(rightWristRollJointVal);

        double rightWristPitchJointVal = jointStateMap_[rightArmJointNames_.at(6)] * TO_DEGREES;
        rightWristPitchJointVal = ((rightWristPitchJointVal - RIGHT_WRIST_PITCH_MIN )* 100)/(RIGHT_WRIST_PITCH_MAX-RIGHT_WRIST_PITCH_MIN);
        ui->sliderWristPitch->setValue(rightWristPitchJointVal);

    }

    mtx_.unlock();

}

void ToughGUI::getChestState()
{
    geometry_msgs::Quaternion orientation;
    chestController_->getChestOrientation(orientation);
    tf::Quaternion q( orientation.x, orientation.y, orientation.z, orientation.w);
    tf::Matrix3x3 m(q);
    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);

    yaw   = ((yaw - CHEST_YAW_MIN )* 100)/(CHEST_YAW_MAX-CHEST_YAW_MIN);
    roll  = ((roll - CHEST_ROLL_MIN )* 100)/(CHEST_ROLL_MAX-CHEST_ROLL_MIN);
    pitch = ((pitch - CHEST_PITCH_MIN )* 100)/(CHEST_PITCH_MAX-CHEST_PITCH_MIN);

    ui->sliderChestYaw->setValue(yaw);
    ui->sliderChestRoll->setValue(roll);
    ui->sliderChestPitch->setValue(pitch);
    return;
}

void ToughGUI::getPelvisState()
{
    geometry_msgs::Pose pose;
    pelvisHeightController_->getTaskSpaceState(pose, LEFT) ;
    ui->txtPelvisHeight->setValue(pose.position.z);

    ui->lblRobotPositionX->setText(QString::number(pose.position.x, 'f',2));
    ui->lblRobotPositionY->setText(QString::number(pose.position.y, 'f',2));
    ui->lblRobotPositionTheta->setText(QString::number(tf::getYaw(pose.orientation), 'f',2));
}

void ToughGUI::getNeckState()
{

}

void ToughGUI::getGripperState()
{

}

void ToughGUI::getClickedPoint(const geometry_msgs::PointStamped::Ptr msg)
{
    if (!moveArmCommand_)
        return;

    if (clickedPoint_ != nullptr)
        delete clickedPoint_;
    ROS_INFO("Creating new point");
    clickedPoint_ = new geometry_msgs::Pose();
    clickedPoint_->orientation.w = 1.0;
    clickedPoint_->position = msg->point;

    RobotSide side = ui->radioNudgeSideLeft->isChecked() ? LEFT : RIGHT;
    ROS_INFO("Moving arm");
    armJointController_->moveArmInTaskSpace(side, *clickedPoint_, 3.0f);
    moveArmCommand_ = false;
}

void ToughGUI::jointStateCallBack(const ros::TimerEvent& e)
{

    static std::vector<std::string>        jointNames;
    static std::vector<double>              jointValues;

    if(jointNames.empty()){
        currentState_->getJointNames(jointNames);
    }
    currentState_->getJointPositions(jointValues);

    for(size_t i=0; i < jointNames.size(); i++){
        jointStateMap_[jointNames.at(i)] = jointValues.at(i);
        if(jointLabelMap_.find(jointNames.at(i)) != jointLabelMap_.end()){
            QLabel *label = jointLabelMap_[jointNames.at(i)];
            QString text;
            label->setText(text.sprintf("%.2f",jointValues.at(i)));
        }
    }

    // update pelvis height
    if(!ui->txtPelvisHeight->hasFocus())
        getPelvisState();
}

void ToughGUI::updateJointStateSub(int tabID){
    //0 = nudge
    //1 = arm
    //2 = chest
    //3 = neck
    //4 = walk
    //    jointStateSub_ = nh_.subscribe("/joint_states",1, &ValkyrieGUI::jointStateCallBack, this);
    switch (tabID) {
    case 0:
        getArmState();
        getGripperState();
        break;
    case 1:
        getArmState();
        getGripperState();
        break;
    case 2:
        getChestState();
        break;
    case 3:
        getNeckState();
        break;
    case 4:
        getPelvisState();
        break;
    default:
        break;
    }

    //    jointStateSub_.shutdown();

}

void ToughGUI::updateArmSide(int btnID)
{
    //-2 = Left arm
    //-3 = right arm
    getArmState();
}

void ToughGUI::resetChestOrientation()
{
    chestController_->resetPose();
    getChestState();
}

void ToughGUI::resetArm()
{
    RobotSide side = ui->radioArmSideLeft->isChecked() ? LEFT : RIGHT;
    armJointController_->moveToDefaultPose(side);
    getArmState();
}

void ToughGUI::resetRobot()
{
    resetChestOrientation();
    ros::Duration(0.2).sleep();
    armJointController_->moveToDefaultPose(LEFT);
    ros::Duration(0.2).sleep();
    armJointController_->moveToDefaultPose(RIGHT);
    ros::Duration(0.2).sleep();
    pelvisHeightController_->controlPelvisHeight(0.717);
    getArmState();
}

void ToughGUI::moveToPoint()
{
    toolManager_->setCurrentTool(pointTool_);
    moveArmCommand_ = true;
    //    pointTool_->getPropertyContainer()->subProp("Topic")->setValue("clicked_point");
    //    armSide side = ui->radioArmSideLeft->isChecked() ? LEFT : RIGHT;
    //    armJointController_->moveArmInTaskSpace(side, *clickedPoint_);

}

void ToughGUI::nudgeArm(int btnID)
{
    //    down -2
    //    up  -3
    //    back  -4
    //    front -7
    //    left  -5
    //    right -6
    RobotSide side = ui->radioNudgeSideLeft->isChecked() ? LEFT : RIGHT;
    //    std::vector<geometry_msgs::Pose> waypoint;
    //    geometry_msgs::Pose pose;
    //    moveit_msgs::RobotTrajectory traj;

    //    float x=0.0f, y=0.0f, z=0.0f;

    switch (btnID) {
    case -2: //down
        armJointController_->nudgeArm(side, direction::DOWN);
        //        z -= 0.05;
        break;
    case -3: //up
        armJointController_->nudgeArm(side, direction::UP);
        //        z += 0.05;
        break;
    case -4: //back
        armJointController_->nudgeArm(side, direction::BACK);
        //        x -= 0.05;
        break;
    case -7: //front
        armJointController_->nudgeArm(side, direction::FRONT);
        //        x += 0.05;
        break;
    case -5: //left
        armJointController_->nudgeArm(side, direction::LEFT);
        //        y += 0.05;
        break;
    case -6: //right
        armJointController_->nudgeArm(side, direction::RIGHT);
        //        y -= 0.05;
        break;
    default:
        break;
    }

    //    armJointController_->nudgeArmLocal(side, x, y, z,pose);
    //    waypoint.clear();
    //    waypoint.push_back(pose);
    //    if(side ==armSide::RIGHT)
    //    {
    //        if(rightArmPlanner_->getTrajFromCartPoints(waypoint, traj, false, 0.2) > 0.1) {
    //            armJointController_->moveArmTrajectory(side, traj.joint_trajectory);
    //        }

    //    }
    //    else
    //    {
    //        if(leftArmPlanner_->getTrajFromCartPoints(waypoint, traj, false, 0.2) > 0.1){
    //            armJointController_->moveArmTrajectory(side, traj.joint_trajectory);
    //        }

    //    }

}

void ToughGUI::updateDisplay(int tabID)
{
    switch (tabID) {
    case 0:
        ui->radioBtnNone->setEnabled(true);
        ui->radioBtnOctomap->setEnabled(true);
        ui->radioBtnPointcloud->setEnabled(true);
        // change current tool to interact when changing tabs
        setCurrentTool(-2);
        break;
    case 1:
        ui->radioBtnNone->setEnabled(false);
        ui->radioBtnOctomap->setEnabled(false);
        ui->radioBtnPointcloud->setEnabled(false);
        // change current tool to interact when changing tabs
        setCurrentTool(-2);
        break;
    default:
        break;
    }
    //0 = 3d scene
    //1 = map

}

void ToughGUI::keyPressEvent(QKeyEvent *event)
{
    switch(event->key())
    {
    case Qt::Key_W:
        closeGrippers();
        ROS_INFO("key W pressed");
        break;
    case Qt::Key_A:
        ROS_INFO("key A pressed");
        break;
    case Qt::Key_D:
        ROS_INFO("key D pressed");
        break;
    case Qt::Key_S:
        openGrippers();
        ROS_INFO("key S pressed");
        break;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

void ToughGUI::liveVideoCallback(const sensor_msgs::ImageConstPtr& msg)
{

    /**
     * Adding Image display opens up the image in a new window.
     * As a workaround to show image in the same GUI window, OpenCV is being used to display image on a Qlabel
     */
    cv_bridge::CvImagePtr cv_ptr, cv_ptr_big;
    bool is_rgb = false;
    try{
        if(msg->encoding == sensor_msgs::image_encodings::RGB8 || msg->encoding == sensor_msgs::image_encodings::RGB16) {
            cv_ptr     = cv_bridge::toCvCopy(msg, msg->encoding);
            is_rgb = true;
        }

        else{
            cv_ptr     = cv_bridge::toCvCopy(msg, msg->encoding);
        }
    }
    catch (cv_bridge::Exception& e){
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    //  convert cv image into RGB image and resize it to the size of available layout
    setVideo(ui->liveVideoLabel,cv_ptr, is_rgb);
}

void ToughGUI::setVideo(QLabel* label, cv_bridge::CvImagePtr cv_ptr, bool is_RGB){
    cv::Mat RGBImg;
    QLabel* liveVideoLabel = label;

    // To avoid auto expansion of QLabel,keep the video dimensions slightly less than the label dimension
    int height = liveVideoLabel->height()-1;
    int width  = liveVideoLabel->width()-1;

    if(liveVideoLabel->height()-1 >= (liveVideoLabel->width()-1)*IMAGE_HEIGHT/IMAGE_WIDTH)
        height = (liveVideoLabel->width()-1)*IMAGE_HEIGHT/IMAGE_WIDTH;
    else
        width  = (liveVideoLabel->height()-1)*IMAGE_WIDTH/IMAGE_HEIGHT;
    if (is_RGB){
        RGBImg = cv_ptr->image;
    }
    else{
        cv::cvtColor(cv_ptr->image, RGBImg, CV_BGR2RGB);
    }
    cv::resize(RGBImg, RGBImg, cvSize(width, height));
    //flip the image
    if(flipImage_)
        cv::flip(RGBImg, RGBImg, -1);
    //  convert RGB image into QImage and publish that on the label for livevideo
    QImage qImage_= QImage((uchar*) RGBImg.data, RGBImg.cols, RGBImg.rows, RGBImg.cols*3, QImage::Format_RGB888);
    liveVideoLabel->setPixmap(QPixmap::fromImage(qImage_));
    liveVideoLabel->show();

}

void ToughGUI::updateGripperSide(int btnID)
{
    ui->cmbBoxGripMode->setCurrentIndex((ui->radioGripSideLeft->isChecked()) ? prev_mode_map[PREVIOUS_MODE_LEFT] : prev_mode_map[PREVIOUS_MODE_RIGHT]);
}

void ToughGUI::setMode()
{
    if(ui->radioGripSideLeft->isChecked())
    {
        if(!(PREVIOUS_MODE_LEFT == ui->cmbBoxGripMode->currentText()))
        {
        gripperController_->setMode(RobotSide::LEFT,mode_map[ui->cmbBoxGripMode->currentText()]);
        PREVIOUS_MODE_LEFT = ui->cmbBoxGripMode->currentText();
        }
    }
    else
    {
        if(!(PREVIOUS_MODE_RIGHT == ui->cmbBoxGripMode->currentText()))
        {
        gripperController_->setMode(RobotSide::RIGHT,mode_map[ui->cmbBoxGripMode->currentText()]);
        PREVIOUS_MODE_RIGHT = ui->cmbBoxGripMode->currentText();
        }
    }
}

void ToughGUI::closeGrippers()
{
    //call close grppiers function here
    setMode();
    RobotSide side = ui->radioGripSideLeft->isChecked() ? LEFT : RIGHT;
    gripperController_->closeGripper(side);
}

void ToughGUI::openGrippers()
{
    //call open grippers function here
    setMode();
    RobotSide side = ui->radioGripSideLeft->isChecked() ? LEFT : RIGHT;
    gripperController_->openGripper(side);
}

void ToughGUI::closeFingers()
{
    setMode();
    RobotSide side = ui->radioGripSideLeft->isChecked() ? LEFT : RIGHT;
    gripperController_->closeFingers(side);

}

void ToughGUI::openFingers()
{
    setMode();
    RobotSide side = ui->radioGripSideLeft->isChecked() ? LEFT : RIGHT;
    gripperController_->openFingers(side);
}

void ToughGUI::closeThumb()
{
    setMode();
    RobotSide side = ui->radioGripSideLeft->isChecked() ? LEFT : RIGHT;
    gripperController_->closeThumb(side);

}

void ToughGUI::openThumb()
{
    setMode();
    RobotSide side = ui->radioGripSideLeft->isChecked() ? LEFT : RIGHT;
    gripperController_->openThumb(side);
}

void ToughGUI::resetGrippers()
{
    gripperController_->resetGripper(RobotSide::LEFT);
    gripperController_->resetGripper(RobotSide::RIGHT);

    PREVIOUS_MODE_LEFT = "BASIC";
    PREVIOUS_MODE_RIGHT = "BASIC";

    updateGripperSide(-1);
}

void ToughGUI::closeBothGrippers()
{
    // Putting both grippers in BASIC mode.
    gripperController_->setMode(RobotSide::LEFT,GripperControlInterface::BASIC);
    PREVIOUS_MODE_LEFT = "BASIC";
    ros::Duration(0.1).sleep();

    gripperController_->setMode(RobotSide::RIGHT,GripperControlInterface::BASIC);
    PREVIOUS_MODE_RIGHT = "BASIC";
    ros::Duration(0.1).sleep();

    // Closing both hands
    gripperController_->closeGripper(RobotSide::LEFT);
    ros::Duration(0.1).sleep();
    gripperController_->closeGripper(RobotSide::RIGHT);

    updateGripperSide(-1);
}
void ToughGUI::setCurrentTool(int btnID)
{
    if(btnID == -2)
    {
        ROS_INFO("Interact Tool Selected");
        toolManager_->setCurrentTool(interactTool_);
        mapToolManager_->setCurrentTool(mapInteractTool_);

    }
    else if(btnID == -3)
    {
        ROS_INFO("Measure Tool Selected");
        toolManager_->setCurrentTool(measureTool_);

    }
    else if(btnID == -4)
    {
        ROS_INFO("2DPoseEstimate Tool Selected");
        toolManager_->setCurrentTool(setInitialPoseTool_);
        mapToolManager_->setCurrentTool(setMapInitialPoseTool_);
    }
    else if(btnID == -5)
    {
        ROS_INFO("2DNavGoal Tool Selected");
        toolManager_->setCurrentTool(setGoalTool_);
        mapManager_->getToolManager()->setCurrentTool(setMapGoalTool_);
        ui->controlTabs->setCurrentIndex(4);
    }
    else if(btnID == -6)
    {
        ROS_INFO("PublishPoint Tool Selected");
        toolManager_->setCurrentTool(pointTool_);
    }

    changeToolButtonStatus(btnID);
}

void ToughGUI::changeToolButtonStatus(int btnID)
{
    ui->btnRvizInteract->setFlat(true);
    ui->btnRvizMeasure->setFlat(true);
    ui->btnRvizNavGoal->setFlat(true);
    ui->btnRvizPoseEstimate->setFlat(true);
    ui->btnRvizPublishPoint->setFlat(true);

    switch(btnID)
    {
    case -2: ui->btnRvizInteract->setFlat(false);
        break;
    case -3: ui->btnRvizMeasure->setFlat(false);
        break;
    case -4: ui->btnRvizPoseEstimate->setFlat(false);
        break;
    case -5: ui->btnRvizNavGoal->setFlat(false);
        break;
    case -6: ui->btnRvizPublishPoint->setFlat(false);
    }
}

void ToughGUI::displayPointcloud(int btnID)
{

    switch (btnID) {
    case -4:    // button ID of octomap
        octomapDisplay_->setEnabled(true);
        cloudDisplay_->setEnabled(false);
        break;
    case -2:    // button ID of Pointcloud
        octomapDisplay_->setEnabled(false);
        cloudDisplay_->setEnabled(true);
        break;
    case -3:    //button ID of None
        octomapDisplay_->setEnabled(false);
        cloudDisplay_->setEnabled(false);
    default:
        break;
    }
}

void ToughGUI::walkSteps()
{

    RobotSide side = ui->radioLeftFoot->isChecked() ? LEFT : RIGHT;
    int numOfSteps = ui->lineEditNumSteps->text().toInt();
    float xOffset = ui->lineEditXOffset->text().toFloat();
    float yOffset = ui->lineEditYOffset->text().toFloat();
    float swingTime = ui->lineEditSwingTime->text().toFloat();
    float transferTime = ui->lineEditTransferTime->text().toFloat();
    float swingHeight = ui->lineEditSwingHeight->text().toFloat();
    if(walkingController_ != nullptr){

        if (swingTime != swingTime_ || transferTime != transferTime_){
            walkingController_->setWalkParams(transferTime, swingTime, 0);
            swingTime_ = swingTime;
            transferTime_ = transferTime;
        }

        if (swingHeight != swingHeight_) {
            walkingController_->setSwingHeight(swingHeight);
            swingHeight_ = swingHeight;
        }

        walkingController_->walkNStepsWRTPelvis(numOfSteps, xOffset, yOffset, false, side, false);
    }
}

void ToughGUI::approveSteps()
{
    std_msgs::Empty msg;
    approveStepsPub_.publish(msg);
}

void ToughGUI::abortSteps()
{
    walkingController_->abortWalk();
}
void ToughGUI::changePelvisHeight(){

    float height = ui->sliderPelvisHeight->value()*(PELVIS_HEIGHT_MAX-PELVIS_HEIGHT_MIN)/100+PELVIS_HEIGHT_MIN;
    if(pelvisHeightController_ != nullptr){
        pelvisHeightController_->controlPelvisHeight(height);
    }

}

void ToughGUI::moveArmJoints(){
    if(armJointController_ == nullptr) {
        return;
    }
    float shoulderRollValue ;
    float shoulderPitchValue;
    float shoulderYawValue  ;

    float wristRollValue    ;
    float wristPitchValue   ;
    float wristYawValue     ;

    float elbowValue        ;

    RobotSide side = ui->radioArmSideLeft->isChecked() ? LEFT : RIGHT;
    if(side == LEFT)
    {
        shoulderRollValue  = ui->sliderShoulderRoll->value()*(LEFT_SHOULDER_ROLL_MAX-LEFT_SHOULDER_ROLL_MIN)/100+LEFT_SHOULDER_ROLL_MIN;
        shoulderPitchValue = ui->sliderShoulderPitch->value()*(LEFT_SHOULDER_PITCH_MAX-LEFT_SHOULDER_PITCH_MIN)/100+LEFT_SHOULDER_PITCH_MIN;
        shoulderYawValue   = ui->sliderShoulderYaw->value()*(LEFT_SHOULDER_YAW_MAX-LEFT_SHOULDER_YAW_MIN)/100+LEFT_SHOULDER_YAW_MIN;

        wristRollValue     = ui->sliderWristRoll->value()*(LEFT_WRIST_ROLL_MAX-LEFT_WRIST_ROLL_MIN)/100+LEFT_WRIST_ROLL_MIN;
        wristPitchValue    = ui->sliderWristPitch->value()*(LEFT_WRIST_PITCH_MAX-LEFT_WRIST_PITCH_MIN)/100+LEFT_WRIST_PITCH_MIN;
        wristYawValue      = ui->sliderWristYaw->value()*(LEFT_WRIST_YAW_MAX-LEFT_WRIST_YAW_MIN)/100+LEFT_WRIST_YAW_MIN;

        elbowValue         = ui->sliderElbow->value()*(LEFT_ELBOW_MAX-LEFT_ELBOW_MIN)/100+LEFT_ELBOW_MIN;
    }
    else {
        // This is for the Right shoulder

        shoulderRollValue  = ui->sliderShoulderRoll->value()*(RIGHT_SHOULDER_ROLL_MAX-RIGHT_SHOULDER_ROLL_MIN)/100+RIGHT_SHOULDER_ROLL_MIN;
        shoulderPitchValue = ui->sliderShoulderPitch->value()*(RIGHT_SHOULDER_PITCH_MAX-RIGHT_SHOULDER_PITCH_MIN)/100+RIGHT_SHOULDER_PITCH_MIN;
        shoulderYawValue   = ui->sliderShoulderYaw->value()*(RIGHT_SHOULDER_YAW_MAX-RIGHT_SHOULDER_YAW_MIN)/100+RIGHT_SHOULDER_YAW_MIN;
        wristRollValue     = ui->sliderWristRoll->value()*(RIGHT_WRIST_ROLL_MAX-RIGHT_WRIST_ROLL_MIN)/100+RIGHT_WRIST_ROLL_MIN;
        wristPitchValue    = ui->sliderWristPitch->value()*(RIGHT_WRIST_PITCH_MAX-RIGHT_WRIST_PITCH_MIN)/100+RIGHT_WRIST_PITCH_MIN;
        wristYawValue      = ui->sliderWristYaw->value()*(RIGHT_WRIST_YAW_MAX-RIGHT_WRIST_YAW_MIN)/100+RIGHT_WRIST_YAW_MIN;

        elbowValue         = ui->sliderElbow->value()*(RIGHT_ELBOW_MAX-RIGHT_ELBOW_MIN)/100+RIGHT_ELBOW_MIN;
    }


    std::vector<ArmControlInterface::ArmJointData> data;
    ArmControlInterface::ArmJointData msg;

    //sequence of joints for sending arm data
    //        std::vector<std::string> joints = {"leftShoulderPitch", "leftShoulderRoll", "leftShoulderYaw", "leftElbowPitch", "leftForearmYaw", "leftWristRoll", "leftWristPitch"};

    msg.arm_pose = {shoulderPitchValue * TO_RADIANS, shoulderRollValue* TO_RADIANS, shoulderYawValue* TO_RADIANS,
                    elbowValue* TO_RADIANS, wristYawValue* TO_RADIANS, wristRollValue* TO_RADIANS, wristPitchValue* TO_RADIANS};
    msg.side = side;
    msg.time = 0.0;
    //    for (size_t i = 0; i< msg.arm_pose.size(); i++){
    //        ROS_INFO("%s : %0.2f", joints[i].c_str(), msg.arm_pose[i]);
    //    }
    data.push_back(msg);

    //this is a non-blocking call
    armJointController_->moveArmJoints(data);
}

void ToughGUI::moveChestJoints()
{
    float chestRollSliderValue = ui->sliderChestRoll->value()*(CHEST_ROLL_MAX-CHEST_ROLL_MIN)/100+CHEST_ROLL_MIN;
    float chestPitchSliderValue = ui->sliderChestPitch->value()*(CHEST_PITCH_MAX-CHEST_PITCH_MIN)/100 + CHEST_PITCH_MIN;
    float chestYawSliderValue = -1.0f * (ui->sliderChestYaw->value()*(CHEST_YAW_MAX-CHEST_YAW_MIN)/100 + CHEST_YAW_MIN); // this is to align yaw with sliding direction
    if(chestController_ != nullptr){
        chestController_->controlChest(chestRollSliderValue, chestPitchSliderValue, chestYawSliderValue);
        ros::spinOnce();
    }
}

void ToughGUI::moveHeadJoints()
{
    float upperNeckPitchSliderValue = ui->sliderUpperNeckPitch->value()*(UPPER_NECK_PITCH_MAX-UPPER_NECK_PITCH_MIN)/100+UPPER_NECK_PITCH_MIN;
    float lowerNeckPitchSliderValue = ui->sliderLowerNeckPitch->value()*(LOWER_NECK_PITCH_MAX-LOWER_NECK_PITCH_MIN)/100+LOWER_NECK_PITCH_MIN;
    float neckYawSliderValue =  -1*(ui->sliderNeckYaw->value()*(NECK_YAW_MAX-NECK_YAW_MIN)/100 + CHEST_YAW_MIN);
    if(headController_ != nullptr){
        headController_->moveHead(lowerNeckPitchSliderValue, upperNeckPitchSliderValue, neckYawSliderValue);
        ros::spinOnce();
    }
}


/*
//Image :
    grid_ = manager_->createDisplay( "rviz/Image", "Image View", true );
    ROS_ASSERT( grid_ != NULL );
    grid_->subProp( "Image Topic" )->setValue( "/camera/rgb/image_raw" );
    grid_->subProp( "Transport Hint" )->setValue( "theora" );


//Depth Cloud :
    grid_ = manager_->createDisplay( "rviz/DepthCloud", "Image View", true );
    ROS_ASSERT( grid_ != NULL );

    grid_->subProp( "Depth Map Topic" )->setValue( "/camera/depth/image_raw" );
    grid_->subProp( "Depth Map Transport Hint" )->setValue( "raw" );
    grid_->subProp( "Color Image Topic" )->setValue( "/camera/rgb/image_raw" );
    grid_->subProp( "Color Transport Hint" )->setValue( "raw" );
    grid_->subProp("Queue Size")->setValue(5);
    grid_->subProp("Style")->setValue("Flat Squares");

//    mainDisplay_ = manager_->createDisplay( "rviz/PointCloud2", "3D Pointcloud view", true );
//    ROS_ASSERT( mainDisplay_ != NULL );

//    mainDisplay_->subProp( "Topic" )->setValue( pointCloudTopic_ );
//    mainDisplay_->subProp( "Selectable" )->setValue( "true" );
//    mainDisplay_->subProp( "Style" )->setValue( "Boxes" );
//    mainDisplay_->subProp("Alpha")->setValue(0.5);

  manager_->createDisplay( "rviz/Grid", "Grid", true );

//MarkerArray :
rviz::Display* octomapDisplay_ = manager_->createDisplay( "rviz/MarkerArray", "Octomap view", true );
ROS_ASSERT( octomapDisplay_ != NULL );

octomapDisplay_->subProp( "Marker Topic" )->setValue( "/occupied_cells_vis_array" );


*/

