//******************************************************************************
//* File:   CameraCalibrator.cpp
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

#include "OatConfig.h" // Generated by CMake

#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/calib3d.hpp>

#include "Saver.h"
#include "UsagePrinter.h"
#include "PathChanger.h"
#include "CameraCalibrator.h"

CameraCalibrator::CameraCalibrator(const std::string& frame_source_name,
        const CameraModel& model, cv::Size& chessboard_size) :
  Calibrator(frame_source_name)
, chessboard_size_(chessboard_size)
, calibration_valid_(false)
, model_(model)
{

    // if (interactive_) { // TODO: Generalize to accept images specifed by a file without interactive session

    // Initialize corner detection update timers
    tick_ = Clock::now();
    tock_ = Clock::now();

#ifdef OAT_USE_OPENGL
        try {
            cv::namedWindow(name(), cv::WINDOW_OPENGL & cv::WINDOW_KEEPRATIO);
        } catch (cv::Exception& ex) {
            oat::whoWarn(name(), "OpenCV not compiled with OpenGL support."
                    "Falling back to OpenCV's display driver.\n");
            cv::namedWindow(name(), cv::WINDOW_NORMAL & cv::WINDOW_KEEPRATIO);
        }
#else
        cv::namedWindow(name(), cv::WINDOW_NORMAL & cv::WINDOW_KEEPRATIO);
#endif

        std::cout << "Starting interactive session.\n";
        printUsage(std::cout);
   // }
}

void CameraCalibrator::configure(const std::string& config_file, const std::string& config_key) {

    // TODO: Provide list of image paths to perform calibraiton directly from file.

    // Available options
    std::vector<std::string> options {""};

    // This will throw cpptoml::parse_exception if a file
    // with invalid TOML is provided
    cpptoml::table config;
    config = cpptoml::parse_file(config_file);

    // See if a camera configuration was provided
    if (config.contains(config_key)) {

        // Get this components configuration table
        auto this_config = config.get_table(config_key);

        // Check for unknown options in the table and throw if you find them
        oat::config::checkKeys(options, this_config);

    } else {
        throw (std::runtime_error(oat::configNoTableError(config_key, config_file)));
    }
}

void CameraCalibrator::calibrate(cv::Mat& frame) {

    tick_ = Clock::now();

    if (in_capture_mode_) {
        detectChessboard(frame);
    }

    cv::imshow(name(), frame);
    char command = cv::waitKey(1);

    switch (command) {

        case 'c': // Enter/exit chessboard corner capture mode
        {
            in_capture_mode_ = !(in_capture_mode_);
            break;
        }
        case 'f': // Change the calibration save path
        {
            PathChanger changer;
            accept(&changer);
            break;
        }
        case 'g': // Generate calibration parameters
        {
            generateCalibrationParameters();
            break;
        }
        case 'h': // Display help dialog
        {
            UsagePrinter usage;
            accept(&usage, std::cout);
            break;
        }
        case 'm': // Select homography estimation method
        {
            selectCalibrationMethod();
            break;
        }
        case 'p': // Print calibration results
        {
            printCalibrationResults(std::cout);
            break;
        }
        case 's': // Save homography info
        {
            Saver saver("calibration", calibration_save_path_);
            accept(&saver);
            break;
        }
    }
}

void CameraCalibrator::detectChessboard(cv::Mat& frame) {

    // Extract the chessboard from the current image
    std::vector<cv::Point2f> point_buffer;
    bool detected =
        cv::findChessboardCorners(frame, chessboard_size_, point_buffer,
        CALIB_CB_ADAPTIVE_THRESH + CALIB_CB_FAST_CHECK + CALIB_CB_NORMALIZE_IMAGE);

    // Draw corners on the frame
    cv::drawChessboardCorners(frame, chessboard_size_, cv::Mat(point_buffer), detected);

    // Caculate elapsed time since last detection
    if (detected) {

        Milliseconds elapsed_time =
            std::chrono::duration_cast<Milliseconds>(tick_ - tock_);

        // TODO: Do I want to do subpix detection even if min_detection_delay_
        // not satisfied?
        if (elapsed_time > min_detection_delay_) {

            // Reset timer
            tock_ = Clock::now();

            // Subpixel corner location estimation termination criteria
            // Max iterations = 30;
            // Desired accuracy of pixel resolution = 0.1
            cv::TermCriteria term(
                    cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1);

            // Generate grey-scale image
            cv::Mat frame_grey;
            cv::cvtColor(frame, frame_grey, COLOR_BGR2GRAY);

            // Find exact corner locations
            cv::cornerSubPix(frame_grey, point_buffer, cv::Size(11, 11),
                    cv::Size(-1, -1), term);

            // Push the new corners into storage
            corners_.push_back(point_buffer);

            // Note that we have added new corners to our data set
            frame = cv::bitwise_not(frame, frame);
        }
    }
}