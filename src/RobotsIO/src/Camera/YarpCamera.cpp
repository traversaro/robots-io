/*
 * Copyright (C) 2019 Istituto Italiano di Tecnologia (IIT)
 *
 * This software may be modified and distributed under the terms of the
 * GPL-2+ license. See the accompanying LICENSE file for details.
 */

#include <RobotsIO/Camera/YarpCamera.h>
#include <RobotsIO/Utils/ParametersYarpPort.h>

#include <iostream>
#include <thread>

#include <yarp/cv/Cv.h>
#include <yarp/eigen/Eigen.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Value.h>

using namespace Eigen;
using namespace RobotsIO::Camera;
using namespace RobotsIO::Utils;
using namespace yarp::cv;
using namespace yarp::eigen;
using namespace yarp::os;
using namespace yarp::sig;


YarpCamera::YarpCamera(const std::string& port_prefix)
{
    /* Check YARP network. */
    if (!yarp_.checkNetwork())
    {
        throw(std::runtime_error(log_name_ + "::ctor. Error: YARP network is not available."));
    }

    /* Open rgb input port. */
    if (!(port_rgb_.open("/" + port_prefix + "/rgb:i")))
    {
        std::string err = log_name_ + "::ctor. Error: cannot open rgb input port.";
        throw(std::runtime_error(err));
    }

    /* Open depth input port. */
    if (!(port_depth_.open("/" + port_prefix + "/depth:i")))
    {
        std::string err = log_name_ + "::ctor. Error: cannot open depth input port.";
        throw(std::runtime_error(err));
    }

    /* Read camera parameters from network. */
    ParametersYarpPort network_parameters("/" + port_prefix + "/camera_parameters:i");
    while (!(network_parameters.receive_parameters()))
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        yInfo() << log_name_ + "::ctor. Waiting for camera parameters on port " + "/" + port_prefix + "/dataset/camera_parameters:i";
    }
    parameters_ = CameraParameters(network_parameters);

    Camera::initialize();

    /* Log parameters. */
    std::cout << log_name_ + "::ctor. Camera parameters:" << std::endl;
    std::cout << log_name_ + "    - width: " << parameters_.width() << std::endl;
    std::cout << log_name_ + "    - height: " << parameters_.height() << std::endl;
    std::cout << log_name_ + "    - fx: " << parameters_.fx() << std::endl;
    std::cout << log_name_ + "    - fy: " << parameters_.fy() << std::endl;
    std::cout << log_name_ + "    - cx: " << parameters_.cx() << std::endl;
    std::cout << log_name_ + "    - cx: " << parameters_.cy() << std::endl;
}


YarpCamera::YarpCamera
(
    const std::size_t& width,
    const double& height,
    const double& fx,
    const double& cx,
    const double& fy,
    const double& cy,
    const std::string& port_prefix
)
{
    /* Check YARP network. */
    if (!yarp_.checkNetwork())
    {
        throw(std::runtime_error(log_name_ + "::ctor. Error: YARP network is not available."));
    }

    /* Store parameters. */
    parameters_.width(width);
    parameters_.height(height);
    parameters_.fx(fx);
    parameters_.cx(cx);
    parameters_.fy(fy);
    parameters_.cy(cy);
    parameters_.initialized(true);

    /* Open rgb input port. */
    if (!(port_rgb_.open("/" + port_prefix + "/rgbImage:i")))
    {
        std::string err = log_name_ + "::ctor. Error: cannot open rgb input port.";
        throw(std::runtime_error(err));
    }

    /* Open depth input port. */
    if (!(port_depth_.open("/" + port_prefix + "/depthImage:i")))
    {
        std::string err = log_name_ + "::ctor. Error: cannot open depth input port.";
        throw(std::runtime_error(err));
    }

    Camera::initialize();

    /* Log parameters. */
    std::cout << log_name_ + "::ctor. Camera parameters:" << std::endl;
    std::cout << log_name_ + "    - width: " << parameters_.width() << std::endl;
    std::cout << log_name_ + "    - height: " << parameters_.height() << std::endl;
    std::cout << log_name_ + "    - fx: " << parameters_.fx() << std::endl;
    std::cout << log_name_ + "    - fy: " << parameters_.fy() << std::endl;
    std::cout << log_name_ + "    - cx: " << parameters_.cx() << std::endl;
    std::cout << log_name_ + "    - cx: " << parameters_.cy() << std::endl;
}

YarpCamera::YarpCamera
(
    const std::string& data_path,
    const std::size_t& width,
    const double& height,
    const double& fx,
    const double& cx,
    const double& fy,
    const double& cy
) :
    Camera(data_path, width, height, fx, cx, fy, cy)
{
    Camera::initialize();
}


YarpCamera::~YarpCamera()
{
    /* Close ports. */
    port_rgb_.close();

    port_depth_.close();
}


std::pair<bool, MatrixXf> YarpCamera::depth(const bool& blocking)
{
    ImageOf<PixelFloat>* image_in;
    image_in = port_depth_.read(blocking);

    if (image_in == nullptr)
        return std::make_pair(false, MatrixXf());

    cv::Mat image = yarp::cv::toCvMat(*image_in);
    Map<Eigen::Matrix<float, Dynamic, Dynamic, Eigen::RowMajor>> depth(image.ptr<float>(), image.rows, image.cols);

    return std::make_pair(true, depth);
}


std::pair<bool, Transform<double, 3, Affine>> YarpCamera::pose(const bool& blocking)
{
    return std::make_pair(true, Transform<double, 3, Affine>::Identity());
}


std::pair<bool, cv::Mat> YarpCamera::rgb(const bool& blocking)
{
    ImageOf<PixelRgb>* image_in;
    image_in = port_rgb_.read(blocking);

    if (image_in == nullptr)
        return std::make_pair(false, cv::Mat());

    cv::Mat image = yarp::cv::toCvMat(*image_in);

    return std::make_pair(true, image);
}
