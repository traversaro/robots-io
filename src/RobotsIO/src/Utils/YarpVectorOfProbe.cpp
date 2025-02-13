/*
 * Copyright (C) 2019 Istituto Italiano di Tecnologia (IIT)
 *
 * This software may be modified and distributed under the terms of the
 * GPL-2+ license. See the accompanying LICENSE file for details.
 */

#include <RobotsIO/Utils/YarpVectorOfProbe.hpp>

using namespace RobotsIO::Utils;


template <>
yarp::sig::VectorOf<double> RobotsIO::Utils::YarpVectorOfProbe<double, Eigen::VectorXd>::convert_from(const Eigen::VectorXd& data)
{
    yarp::sig::VectorOf<double> tmp(data.size());
    yarp::eigen::toEigen(tmp) = data;

    return tmp;
}


template <>
yarp::sig::VectorOf<double> RobotsIO::Utils::YarpVectorOfProbe<double, Eigen::Transform<double, 3, Eigen::Affine>>::convert_from(const Eigen::Transform<double, 3, Eigen::Affine>& data)
{
    /* Assume by default transformation to x-y-z-axis-angle. */
    yarp::sig::VectorOf<double> tmp(7);
    yarp::eigen::toEigen(tmp).head<3>() = data.translation();

    Eigen::AngleAxisd axis_angle(data.rotation());
    yarp::eigen::toEigen(tmp).segment<3>(3) = axis_angle.axis();
    yarp::eigen::toEigen(tmp)(6) = axis_angle.angle();

    return tmp;
}
