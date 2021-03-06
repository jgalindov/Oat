//******************************************************************************
//* File:   HomographyGenerator.h
//* Author: Jon Newman <jpnewman snail mit dot edu>
//*
//* Copyright (c) Jon Newman (jpnewman snail mit dot edu)
//* All right reserved.
//* This file is part of the Oat project.
//* This is free software: you can redistribute it and/or modify
//* it under the terms of the GNU General Public License as published by
//* the Free Software Foundation, either version 3 of the License, or
//* (at your option) any later version.
//* This software is distributed in the hope that it will be useful,
//* but WITHOUT ANY WARRANTY; without even the implied warranty of
//* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//* GNU General Public License for more details.
//* You should have received a copy of the GNU General Public License
//* along with this source code.  If not, see <http://www.gnu.org/licenses/>.
//****************************************************************************

#ifndef OAT_HOMOGRAPHYGENERATOR_H
#define OAT_HOMOGRAPHYGENERATOR_H

#include <iosfwd>
#include <string>
#include <opencv2/core/mat.hpp>

#include "Calibrator.h"
#include "CalibratorVisitor.h"

namespace oat {

/**
 * Interactive homography generator.
 */
class HomographyGenerator : public Calibrator {

    using point_size_t = std::vector<cv::Point2f>::size_type;

public:

    // Homography estimation procedure
    enum class EstimationMethod
    {
        ROBUST = 0, //!< RANSAC-based outlier rejection
        REGULAR,    //!< Best bit without outlier rejection
        EXACT       //!< Solve an exact homography for 4 points
    };

    /**
     * Interactive homography transform generator.  The user is presented with
     * a video display of the frame stream. The user then select points on the
     * video feed and enter their equivalent world-unit equivalent. Upon each
     * selection, the a best-fit homography matrix relating pixels to work
     * coordinates is calulated and the MSE between the transformed and
     * user-supplied positions is displayed.
     * @param frame_source_name imaging setup frame source name
     */
    HomographyGenerator(const std::string &frame_source_name,
                        const std::string &calibration_key,
                        EstimationMethod method);

    // Overridden methods
    void configure(const std::string& config_file, const std::string& config_key) override;

    // Accept visitors
    void accept(CalibratorVisitor* visitor) override;
    void accept(OutputVisitor* visitor, std::ostream& out) override;

    // Accessors
    bool homography_valid() const { return homography_valid_; }
    cv::Mat homography() const { return homography_; }

protected:

    /**
     * Perform homography matrix generation routine.
     * @param frame current frame to use for running calibration
     */
    void calibrate(cv::Mat& frame) override;

private:

    // Is homography well-defined?
    bool homography_valid_;
    cv::Mat homography_;

    // Default esimation method
    EstimationMethod method_ {EstimationMethod::ROBUST};

    // Data used to create homography
    std::vector<cv::Point2f> pixels_;
    // TEST DATA
    //{
    //    cv::Point2f(678, 349),
    //    cv::Point2f(672, 25),
    //    cv::Point2f(687, 682),
    //    cv::Point2f(352, 364),
    //    cv::Point2f(1010, 353)
    //};


    std::vector<cv::Point2f> world_points_;
    // TEST DATA
    //{
    //    cv::Point2f(0, 0),
    //    cv::Point2f(0, 1),
    //    cv::Point2f(0, -1),
    //    cv::Point2f(-1, 0),
    //    cv::Point2f(1, 0)
    //};

    // Current mouse point
    bool clicked_ {false};
    cv::Point mouse_pt_;

    // Methods for interactive session to obtain data points related pixel
    // coordinates and world coordinates, generate a homography, and display
    // the results
    int addDataPoint(void);
    int removeDataPoint(void);
    void printDataPoints(std::ostream&);
    int selectHomographyMethod(void);
    int generateHomography(void);
    cv::Mat drawMousePoint(cv::Mat& frame);
    void onMouseEvent(int event, int x, int y);
    static void onMouseEvent(int event, int x, int y, int, void * _this);

};

}      /* namespace oat */
#endif /* OAT_HOMOGRAPHYGENERATOR_H */
