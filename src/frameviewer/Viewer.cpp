//******************************************************************************
//* File:   Viewer.cpp
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

#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui/highgui.hpp>

#include "../../lib/utility/IOFormat.h"
#include "../../lib/shmemdf/Source.h"
#include "../../lib/shmemdf/SharedCVMat.h"

#include "Viewer.h"

namespace oat {

namespace bfs = boost::filesystem;
using namespace boost::interprocess;

// Constant definitions
constexpr Viewer::Milliseconds Viewer::MIN_UPDATE_PERIOD_MS;
constexpr int Viewer::COMPRESSION_LEVEL;

Viewer::Viewer(const std::string& frame_source_address,
               const std::string& snapshot_path) :
  name_("viewer[" + frame_source_address + "]")
, frame_source_address_(frame_source_address)
, snapshot_path_(snapshot_path)
{

    // Initialize GUI update timers
    tick_ = Clock::now();
    tock_ = Clock::now();

#ifdef OAT_USE_OPENGL
    try {
        cv::namedWindow(name_, cv::WINDOW_OPENGL & cv::WINDOW_KEEPRATIO);
    } catch (cv::Exception& ex) {
        oat::whoWarn(name_, "OpenCV not compiled with OpenGL support. "
                           "Falling back to OpenCV's display driver.\n");
        cv::namedWindow(name_, cv::WINDOW_NORMAL & cv::WINDOW_KEEPRATIO);
    }
#else
    cv::namedWindow(name_, cv::WINDOW_NORMAL & cv::WINDOW_KEEPRATIO);
#endif
}

void Viewer::connectToNode() {

    frame_source_.connect(frame_source_address_);
}

bool Viewer::showImage() {

    // START CRITICAL SECTION //
    ////////////////////////////

    // Wait for sink to write to node
    node_state_ = frame_source_.wait();
    if (node_state_ == oat::NodeState::END)
        return true;

    // Clone the shared frame
    //internal_frame_ = frame_source_.clone();
    frame_source_.copyTo(internal_frame_);

    // Tell sink it can continue
    frame_source_.post();

    ////////////////////////////
    //  END CRITICAL SECTION  //

    // Get current time
    tick_ = Clock::now();

    // Figure out the time since we last updated the viewer
    Milliseconds duration = std::chrono::duration_cast<Milliseconds>(tick_ - tock_);

    // If the minimum update period has passed, show frame.
    if (duration > MIN_UPDATE_PERIOD_MS) {

        cv::imshow(name_, internal_frame_);
        tock_ = Clock::now();

        char command = cv::waitKey(1);

        if (command == 's')
            cv::imwrite(makeFileName(), internal_frame_, compression_params_);
    }

    // Sink was not at END state
    return false;
}

void Viewer::generateSnapshotPath() {

    // Snapshot file saving
    // Check that the snapshot save folder is valid
    bfs::path path(snapshot_path_.c_str());
    if (!bfs::exists(path.parent_path())) {
        throw (std::runtime_error ("Requested snapshot save path, " +
               snapshot_path_ + ", does not exist.\n"));
    }

    file_name_ = path.stem().string();
    if (file_name_.empty())
        file_name_ = frame_source_address_;

    // Snapshot encoding
    compression_params_.push_back(CV_IMWRITE_PNG_COMPRESSION);
    compression_params_.push_back(COMPRESSION_LEVEL);
}

std::string Viewer::makeFileName() {

    // Create file name
    std::time_t raw_time;
    struct tm * time_info;
    char buffer[100];
    std::time(&raw_time);
    time_info = std::localtime(&raw_time);
    std::strftime(buffer, 80, "%F-%H-%M-%S", time_info);
    std::string date_now = std::string(buffer);

    // Generate file name for this snapshop
    std::string folder = bfs::path(snapshot_path_.c_str()).parent_path().string();
    std::string frame_fid = folder + "/" + date_now + "_" + file_name_ + ".png";

    // Check for existence
    int i = 0;
    std::string file = frame_fid;

    while (bfs::exists(file.c_str())) {

        ++i;
        bfs::path path(frame_fid.c_str());
        bfs::path folder = path.parent_path();
        bfs::path stem = path.stem();
        bfs::path extension = path.extension();

        std::string append = "_" + std::to_string(i);
        stem += append.c_str();

        // Recreate file name
        file = folder.string() + "/" + stem.string() + "." + extension.string();

    }

    return file;
}

} /* namespace oat */
