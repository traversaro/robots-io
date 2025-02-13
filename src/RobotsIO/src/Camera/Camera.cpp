/*
 * Copyright (C) 2019 Istituto Italiano di Tecnologia (IIT)
 *
 * This software may be modified and distributed under the terms of the
 * GPL-2+ license. See the accompanying LICENSE file for details.
 */

#ifdef _OPENMP
#include <omp.h>
#endif

#include <RobotsIO/Camera/Camera.h>

#include <iostream>

using namespace Eigen;
using namespace RobotsIO::Camera;


Camera::Camera()
{}


Camera::~Camera()
{}


bool Camera::status()
{
    return status_;
}


bool Camera::reset()
{
    if (is_offline())
        frame_index_ = -1;

    status_ = true;

    return true;
}


std::pair<bool, MatrixXd> Camera::deprojection_matrix() const
{
    if (!deprojection_matrix_initialized_)
        return std::make_pair(false, MatrixXd());

    return std::make_pair(true, deprojection_matrix_);
}



std::pair<bool, CameraParameters> Camera::parameters() const
{
    if (!parameters_.is_initialized())
        return std::make_pair(false, CameraParameters());

    return std::make_pair(true, parameters_);
}


std::pair<bool, Eigen::MatrixXd> Camera::point_cloud
(
    const bool& blocking,
    const double& maximum_depth,
    const bool& use_root_frame,
    const bool& enable_colors
)
{
    /* Get rgb, if required. */
    bool valid_rgb = false;
    cv::Mat rgb;
    if (enable_colors)
    {
        std::tie(valid_rgb, rgb) = this->rgb(blocking);
        if (!valid_rgb)
            return std::make_pair(false, MatrixXd());
    }

    /* Get depth. */
    bool valid_depth = false;
    MatrixXf depth;
    std::tie(valid_depth, depth) = this->depth(blocking);
    if (!valid_depth)
        return std::make_pair(false, MatrixXd());


    /* Get pose, if required. */
    bool valid_pose = false;
    Transform<double, 3, Affine> camera_pose;
    if (use_root_frame)
    {
        std::tie(valid_pose, camera_pose) = this->pose(blocking);
        if (!valid_pose)
            return std::make_pair(false, MatrixXd());
    }

    /* Find 3D points having positive and less than max_depth_ depth. */
    MatrixXi valid_points(parameters_.height, parameters_.width);
#pragma omp parallel for collapse(2)
    for (std::size_t v = 0; v < parameters_.height; v++)
    {
        for (std::size_t u = 0; u < parameters_.width; u++)
        {
            valid_points(v, u) = 0;

            float depth_u_v = depth(v, u);

            if ((depth_u_v > 0) && (depth_u_v < maximum_depth))
                valid_points(v, u) = 1;
        }
    }
    const std::size_t number_valids = valid_points.sum();

    if (number_valids == 0)
        return std::make_pair(false, MatrixXd());

    /* Get deprojection matrix. */
    bool valid_deprojection_matrix = false;
    MatrixXd deprojection_matrix;
    std::tie(valid_deprojection_matrix, deprojection_matrix) = this->deprojection_matrix();
    if (!valid_deprojection_matrix)
        return std::make_pair(false, MatrixXd());

    /* Store points in the output matrix. */
    const std::size_t number_rows = enable_colors ? 6 : 3;
    MatrixXd cloud(number_rows, number_valids);
    std::size_t counter = 0;
    for (std::size_t v = 0; v < parameters_.height; v++)
        for (std::size_t u = 0; u < parameters_.width; u++)
        {
            if (valid_points(v, u) == 1)
            {
                /* Set 3D point. */
                cloud.col(counter).head<3>() = deprojection_matrix.col(u * parameters_.height + v) * depth(v, u);

                if (enable_colors)
                {
                    /* Set RGB channels. */
                    cv::Vec3b cv_color = rgb.at<cv::Vec3b>(cv::Point2d(u, v));
                    cloud.col(counter)(3) = cv_color[2];
                    cloud.col(counter)(4) = cv_color[1];
                    cloud.col(counter)(5) = cv_color[0];
                }
                counter++;
            }
        }

    /* Express taking into account the camera pose, if required. */
    if (use_root_frame)
        cloud.topRows<3>() = camera_pose * cloud.topRows<3>().colwise().homogeneous();

    return std::make_pair(true, cloud);
}


std::pair<bool, VectorXd> Camera::auxiliary_data(const bool& blocking)
{
    return std::make_pair(false, VectorXd());
}


std::size_t Camera::auxiliary_data_size() const
{
    return 0;
}


std::int32_t Camera::frame_index() const
{
    if (is_offline())
        return frame_index_;

    return -1;
}


bool Camera::is_offline() const
{
    return offline_mode_;
}


bool Camera::set_frame_index(const std::int32_t& index)
{
    if (int(index) < 0)
        frame_index_ = -1;
    else
        frame_index_ = index;

    return true;
}


bool Camera::step_frame()
{
    if (is_offline())
    {
        frame_index_++;

        if ((frame_index_ + 1) > data_.cols())
        {
            status_ = false;

            return false;
        }
    }

    return true;
}


bool Camera::log_frame(const bool& log_depth)
{
    /* Get rgb image. */
    bool valid_rgb = false;
    cv::Mat rgb_image;
    std::tie(valid_rgb, rgb_image) = rgb(true);
    if (!valid_rgb)
        return false;

    /* TODO: complete implementation. */
    /* Get depth image. */
    bool valid_depth = false;
    MatrixXf depth;
    if (log_depth)
    {}

    /* Get camera pose .*/
    bool valid_pose = false;
    Transform<double, 3, Affine> camera_pose;
    std::tie(valid_pose, camera_pose) = pose(true);
    if (!valid_pose)
        return false;

    /* Get auxiliary data. */
    bool is_aux_data = false;
    VectorXd aux_data;
    std::tie(is_aux_data, aux_data) = auxiliary_data(true);

    /* Eigen precision format .*/
    IOFormat full_precision(FullPrecision);

    /* Save frame .*/
    AngleAxisd angle_axis(camera_pose.rotation());
    VectorXd angle(1);
    angle(0) = angle_axis.angle();

    if (valid_rgb)
        cv::imwrite(log_path_ + "rgb_" + std::to_string(log_index_) + ".png", rgb_image);
    if (valid_depth)
        ;
    log_ << log_index_ << " "
         << camera_pose.translation().transpose().format(full_precision) << " "
         << angle_axis.axis().transpose().format(full_precision) << " "
         << angle.format(full_precision);

    if (is_aux_data)
        log_ << " " << aux_data.transpose().format(full_precision);

    log_ << std::endl;

    log_index_++;

    return true;
}


bool Camera::start_log(const std::string& path)
{
    log_path_ = path;
    if (log_path_.back() != '/')
        log_path_ += "/";

    log_.open(log_path_ + "data.txt");

    log_index_ = 0;

    return log_.is_open();
}


bool Camera::stop_log()
{
    log_.close();

    return !log_.fail();
}


bool Camera::initialize()
{
    bool ok = true;

    /* Cache the deprojection matrix once for all. */
    ok &= evaluate_deprojection_matrix();

    /* If offline mode, load data from file. */
    if (is_offline())
    {
        bool valid_data = false;
        std::tie(valid_data, data_) = load_data();
        if (!valid_data)
            throw(std::runtime_error(log_name_ + "::initialize. Cannot load offline data from " + data_path_));
    }

    return ok;
}


bool Camera::evaluate_deprojection_matrix()
{
    if (!parameters_.is_initialized())
        throw(std::runtime_error(log_name_ + "::reset. Camera parameters not initialized. Did you initialize the class member 'parameters_' in the derived class?."));

    /* Allocate storage. */
    deprojection_matrix_.resize(3, parameters_.width * parameters_.height);

    // Evaluate deprojection matrix
    int i = 0;
    for (std::size_t u = 0; u < parameters_.width; u++)
    {
        for (std::size_t v = 0; v < parameters_.height; v++)
        {
            deprojection_matrix_(0, i) = (u - parameters_.cx) / parameters_.fx;
            deprojection_matrix_(1, i) = (v - parameters_.cy) / parameters_.fy;
            deprojection_matrix_(2, i) = 1.0;

            i++;
        }
    }

    deprojection_matrix_initialized_ = true;

    return true;
}


Camera::Camera
(
    const std::string& data_path,
    const std::size_t& width,
    const double& height,
    const double& fx,
    const double& cx,
    const double& fy,
    const double& cy
) :
    data_path_(data_path),
    offline_mode_(true)
{
    /* Set intrinsic parameters. */
    parameters_.width = width;
    parameters_.height = height;
    parameters_.fx = fx;
    parameters_.cx = cx;
    parameters_.fy = fy;
    parameters_.cy = cy;
    parameters_.set_initialized();

    /* Fix data path. */
    if (data_path_.back() != '/')
        data_path_ += '/';

    /* Log parameters. */
    std::cout << log_name_ + "::ctor. Camera parameters:" << std::endl;
    std::cout << log_name_ + "    - width: " << parameters_.width << std::endl;
    std::cout << log_name_ + "    - height: " << parameters_.height << std::endl;
    std::cout << log_name_ + "    - fx: " << parameters_.fx << std::endl;
    std::cout << log_name_ + "    - fy: " << parameters_.fy << std::endl;
    std::cout << log_name_ + "    - cx: " << parameters_.cx << std::endl;
}


std::pair<bool, MatrixXf> Camera::depth_offline()
{
    std::FILE* in;
    const std::string file_name = data_path_ + "depth_" + std::to_string(frame_index_) + ".float";

    if ((in = std::fopen(file_name.c_str(), "rb")) == nullptr)
    {
        std::cout << log_name_ << "::depth_offline. Error: cannot load depth frame " + file_name;
        return std::make_pair(true, MatrixXf());
    }

    /* Load image size .*/
    std::size_t dims[2];
    if (std::fread(dims, sizeof(dims), 1, in) != 1)
        return std::make_pair(false, MatrixXf());

    /* Load image. */
    float float_image_raw[dims[0] * dims[1]];
    if (std::fread(float_image_raw, sizeof(float), dims[0] * dims[1], in) != dims[0] * dims[1])
        return std::make_pair(false, MatrixXf());

    /* Store image. */
    MatrixXf float_image(dims[1], dims[0]);
    float_image = Map<Matrix<float, -1, -1, RowMajor>>(float_image_raw, dims[1], dims[0]);

    std::fclose(in);

    return std::make_pair(true, float_image);
}


std::pair<bool, Transform<double, 3, Affine>> Camera::pose_offline()
{
    VectorXd data = data_.col(frame_index_);

    Vector3d position = data.segment<3>(1);
    VectorXd axis_angle = data.segment<4>(1 + 3);
    AngleAxisd angle_axis(axis_angle(3), axis_angle.head<3>());

    Transform<double, 3, Affine> pose;
    pose = Translation<double, 3>(position);
    pose.rotate(angle_axis);

    return std::make_pair(true, pose);
}


std::pair<bool, cv::Mat> Camera::rgb_offline()
{
    const std::string file_name = data_path_ + "rgb_" + std::to_string(frame_index_) + ".png";
    cv::Mat image = cv::imread(data_path_ + "rgb_" + std::to_string(frame_index_) + ".png", cv::IMREAD_COLOR);

    if (image.empty())
    {
        std::cout << log_name_ << "::rgb_offline. Warning: frame " << file_name << " is empty!" << std::endl;
        return std::make_pair(false, cv::Mat());
    }
    cv::resize(image, image, cv::Size(parameters_.width, parameters_.height));

    return std::make_pair(true, image);
}


std::pair<bool, VectorXd> Camera::auxiliary_data_offline()
{
    VectorXd data = data_.col(frame_index_);

    if (auxiliary_data_size() == 0)
        return std::make_pair(false, VectorXd());

    return std::make_pair(true, data.segment(standard_data_offset_, auxiliary_data_size()));
}


std::pair<bool, MatrixXd> Camera::load_data()
{
    MatrixXd data;
    const std::string file_name = data_path_ + "data.txt";
    const std::size_t num_fields = standard_data_offset_ + auxiliary_data_size();

    std::ifstream istrm(file_name);
    if (!istrm.is_open())
    {
        std::cout << log_name_ + "::read_data_from_file. Error: failed to open " << file_name << std::endl;

        return std::make_pair(false, MatrixXd(0,0));
    }
    else
    {
        std::vector<std::string> istrm_strings;
        std::string line;
        while (std::getline(istrm, line))
        {
            istrm_strings.push_back(line);
        }

        data.resize(num_fields, istrm_strings.size());
        std::size_t found_lines = 0;
        for (auto line : istrm_strings)
        {
            std::size_t found_fields = 0;
            std::string number_str;
            std::istringstream iss(line);

            while (iss >> number_str)
            {
                std::size_t index = (num_fields * found_lines) + found_fields;
                *(data.data() + index) = std::stod(number_str);
                found_fields++;
            }
            if (num_fields != found_fields)
            {
                std::cout << log_name_ + "::read_data_from_file. Error: malformed input file " << file_name << std::endl;

                return std::make_pair(false, MatrixXd(0,0));
            }
            found_lines++;
        }

        istrm.close();

        return std::make_pair(true, data);
    }
}
