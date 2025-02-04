#include <stdlib.h>
#include <stdio.h>
#include <std_msgs/String.h>
#include <tough_perception_common/MultisenseImage.h>
#include <opencv2/core.hpp>
#include <opencv/highgui.h>

#define SCS_SIMULATION

void showImage(cv::Mat &image,std::string name)
{
    cv::namedWindow(name,cv::WINDOW_AUTOSIZE);
    cv::imshow(name,image);
    ROS_INFO("Press ESC or q to continue");
    while(cv::waitKey(1)!=27 && cv::waitKey(1)!='q');
    ROS_INFO("closing window");
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "test_multisense_image");
    ros::NodeHandle nh;
    bool status;

    tough_perception::MultisenseImage imageHandler(nh);

#ifdef SCS_SIMULATION
    imageHandler.setImageTopic("/multisense/left/image_rect_color/compressed");
#endif

    ROS_INFO_STREAM("[Height]" << imageHandler.giveHeight() << " [width]" <<imageHandler.giveWidth());

    cv::Mat image;
    status = imageHandler.giveImage(image);
    ROS_INFO("image status %s",status ? "true" : "false");

    if(status)
        showImage(image,"RGB Image");

    return 0;
}
