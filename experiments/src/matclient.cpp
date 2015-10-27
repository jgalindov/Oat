//******************************************************************************
//* File:   main.cpp
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
//******************************************************************************

#include <opencv2/opencv.hpp>

#include <csignal>
#include <exception>

#include "SharedCVMat.h"
#include "NodeManager.h"
#include "Source.h"
#include "SharedCVMat.h"

volatile sig_atomic_t quit = 0;

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int s) {
    quit = 1;
}

/*
 * Demo program showing very efficient shared memory passing of cv::Mat. This
 * server side program should be executed first to load data into shmem.
 */
int main(int argc, char *argv[]) {

    std::signal(SIGINT, sigHandler);
    cv::namedWindow("window", cv::WINDOW_NORMAL & cv::WINDOW_KEEPRATIO);

    try {

        // Create sink to send matrix into
        oat::Source<oat::SharedCVMat> source;
        source.bind("exp_sh_mem", 10e6);

        void* mat_data;
        mat_data = source.read();
        cv::Mat shared_mat(source.object().size(),
                source.object().type(),
                mat_data,
                source.object().step());

        while (!quit) {

            if (mat_data != nullptr) {

                cv::imshow("window", shared_mat);
                cv::waitKey(1);

            }

        }

    } catch (const std::exception& ex) {

        std::cerr << ex.what();
        return -1;
    }

    return 0;
}