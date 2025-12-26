#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <opencv2/opencv.hpp>
#include <cv_bridge/cv_bridge.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <iostream>
#include <chrono>
#include <math.h>

#include "axi_dma_controller.h"
#include "reserved_mem.hpp"

#define DEVICE_FILENAME "/dev/reservedmemLKM"
#define LENGTH 0x007fffff   // length of reserved memory region (bytes)
#define P_START 0x70000000  // physical base address (matches kernel module)
#define P_OFFSET  0x00000000

// DMA UIO index
#define UIO_DMA_N 0

// Assumed network input size (32x32); adjust if needed to match training
static const int NN_IMG_W = 32;
static const int NN_IMG_H = 32;
// You probably have: n_inputs == NN_IMG_W * NN_IMG_H

class ImageSubscriber : public rclcpp::Node
{
public:
    ImageSubscriber()
    : Node("image_subscriber"), dma(UIO_DMA_N, 0x10000)
    {
        init_dma();
        RCLCPP_INFO(this->get_logger(), "Initializing ImageSubscriber node");
        RCLCPP_INFO(this->get_logger(), "Starting camera subscription");

        camera_subscription_ = this->create_subscription<sensor_msgs::msg::Image>(
            "/image_raw",
            100,
            std::bind(&ImageSubscriber::onImageMsg, this, std::placeholders::_1)
        );

        pub_img_ = this->create_publisher<sensor_msgs::msg::Image>("/image_annotated", 10);
    }

private:
    uint32_t *u_buff;
    Reserved_Mem pmem;
    AXIDMAController dma;
    int ret;
    float time = 0.0;
    int loopcnt = 0;

    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr camera_subscription_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr pub_img_;

    void onImageMsg(const sensor_msgs::msg::Image::SharedPtr msg)
    {
        RCLCPP_INFO(this->get_logger(), "Received image!");

        // 1) Convert ROS image to OpenCV BGR
        cv_bridge::CvImagePtr cv_ptr;
        try {
            cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
        } catch (cv_bridge::Exception &e) {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge exception: %s", e.what());
            return;
        }

        cv::Mat img_bgr = cv_ptr->image;

        // 2) Convert to grayscale
        cv::Mat img_gray;
        cv::cvtColor(img_bgr, img_gray, cv::COLOR_BGR2GRAY);

        // 3) Resize to NN input size (e.g. 32x32)
        cv::Mat img_resized;
        cv::resize(img_gray, img_resized, cv::Size(NN_IMG_W, NN_IMG_H),0,0,cv::INTER_AREA);
        // Sanity check
        if (NN_IMG_W * NN_IMG_H > (int)(LENGTH / sizeof(uint32_t))) {
            RCLCPP_ERROR(this->get_logger(), "NN input size exceeds reserved buffer!");
            return;
        }

        // 4) Prepare input buffer: normalize [0,255] -> [0,1] float and pack as uint32_t
        //    Flatten in row-major order matching your training/testbench
        for (int y = 0; y < NN_IMG_H; ++y) {
            for (int x = 0; x < NN_IMG_W; ++x) {
                uint8_t pix = img_resized.at<uint8_t>(y, x);
                float f = static_cast<float>(pix) / 255.0f;

                union { float f; uint32_t u; } conv;
                conv.f = f;
                u_buff[y * NN_IMG_W + x] = conv.u;
            }
        }
	const size_t input_bytes = NN_IMG_W * NN_IMG_H * sizeof(uint32_t);
        const size_t output_bytes = sizeof(uint32_t);   // single prediction word
	
        // 5) Copy input to reserved physical memory
        
	pmem.transfer(u_buff, P_OFFSET, input_bytes);
	

        // 6) Configure DMA for NN IP (AXI-Stream)
        auto t_start = std::chrono::high_resolution_clock::now();
        dma.MM2SReset();
        dma.S2MMReset();

        dma.MM2SHalt();
        dma.S2MMHalt();

        dma.MM2SInterruptEnable();
        dma.S2MMInterruptEnable();

        dma.MM2SSetSourceAddress(P_START + P_OFFSET);
        dma.S2MMSetDestinationAddress(P_START + P_OFFSET);

        dma.S2MMStart();
        dma.MM2SStart();

        dma.MM2SSetLength(input_bytes);   // number of bytes sent TO NN
        dma.S2MMSetLength(output_bytes);  // number of bytes expected FROM NN

        while (!dma.MM2SIsSynced());
        while (!dma.S2MMIsSynced());

        // 7) Read prediction word back from output region
        ret = pmem.gather(u_buff, P_OFFSET, output_bytes);
        if (ret < 0) {
            RCLCPP_ERROR(this->get_logger(), "pmem.gather returned error");
            return;
        }

        auto t_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = t_end - t_start;
        RCLCPP_INFO(this->get_logger(), "NN DMA + IP time: %.3f ms", elapsed.count());
	time += elapsed.count();
	loopcnt += 1;
        RCLCPP_INFO(this->get_logger(), "AVG time: %.3f ms", time/loopcnt);
        // 8) Interpret prediction
        uint32_t raw_pred = u_buff[0];

        
	RCLCPP_INFO(this->get_logger(), "NN prediction raw: %i", raw_pred);
        // 9) (Optional) Overlay prediction on the original image and publish
        cv::Mat annotated = img_bgr.clone();
        std::string text = "NN bin: " + std::to_string(raw_pred);
        cv::putText(annotated, text, cv::Point(20, 40),
                    cv::FONT_HERSHEY_SIMPLEX, 1.0, CV_RGB(0, 255, 0), 2);
	float norm = ((float)raw_pred - 3.0) / 3.0f;
        float theta = norm * 90.0f * 3.14 / 180.0f;

        
        cv::Point start(annotated.cols / 2, annotated.rows - 40);
        int len = 100;

        cv::Point end(
            start.x + static_cast<int>(len * std::sin(theta)),
            start.y - static_cast<int>(len * std::cos(theta))
        );
        cv::arrowedLine(
            annotated, //image
            start, //staring point
            end, // ending point
            cv::Scalar(0, 255, 0),   // green color
            4,                         // thickness of line
            cv::LINE_AA,
            0,
            0.1                       // tip length (fraction of arrow length)
        );
        auto out_msg = cv_bridge::CvImage(msg->header, "bgr8", annotated).toImageMsg();
        pub_img_->publish(*out_msg);
	cv::imshow("Annotated Image", annotated);
	cv::waitKey(1);
    }

    int init_dma()
    {
        u_buff = (uint32_t *)malloc(LENGTH);
        if (u_buff == NULL) {
            RCLCPP_ERROR(this->get_logger(), "Failed to allocate u_buff");
            return -1;
        }
        return 0;
    }
};

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImageSubscriber>());
    rclcpp::shutdown();
    return 0;
}
