//******************************************************************************
//* File:   oat calibrate main.cpp
//* Author: Jon Newman <jpnewman snail mit dot edu>
//
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
#include <csignal>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <opencv2/core.hpp>

#include "../../lib/utility/IOFormat.h"
#include <cpptoml.h>

#include "Calibrator.h"
#include "CameraCalibrator.h"
#include "HomographyGenerator.h"

namespace po = boost::program_options;
namespace bfs = boost::filesystem;

volatile sig_atomic_t quit = 0;
volatile sig_atomic_t source_eof = 0;

void printUsage(po::options_description options){
    std::cout << "Usage: calibrate [INFO]\n"
              << "   or: calibrate SOURCE [CONFIGURATION]\n"
              << "Generate camera calibration and homography transform for a "
              << "frame SOURCE.\n\n"
              << "TYPE\n"
              << "  camera: Generate calibration parameters (camera matrix and distortion coefficients).\n"
              << "  homography: Generate homography transform between pixels and world units.\n\n"
              << "SOURCE:\n"
              << "  User-supplied name of the memory segment to receive frames "
              << "from (e.g. raw).\n\n"
              << options << "\n";
}

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int s) {
    quit = 1;
}

// Processing loop
void run(const std::shared_ptr<Calibrator>& calibrator) {

    while (!quit && !source_eof) {
        source_eof = calibrator->process();
    }
}

int main(int argc, char *argv[]) {

    std::signal(SIGINT, sigHandler);

    // Switches and options
    std::string type;
    std::string source;
    std::string save_path;
    std::string homography_method {"robust"};
    std::string camera_model {"pinhole"};
    int chessboard_width {6};
    int chessboard_height {9};
    double square_length {1.0};
    std::string config_file;
    std::string config_key;
    bool config_used {false};
    
    // Visible program options
    po::options_description visible_options("OPTIONS");

    // Component sub-type
    std::unordered_map<std::string, char> type_hash;
    type_hash["camera"] = 'a';
    type_hash["homography"] = 'b';
    
    // Homography estimation method
    std::unordered_map<std::string, HomographyGenerator::EstimationMethod> homo_meth_hash;
    homo_meth_hash["robust"] = HomographyGenerator::EstimationMethod::ROBUST;
    homo_meth_hash["regular"] = HomographyGenerator::EstimationMethod::REGULAR;
    homo_meth_hash["exact"] = HomographyGenerator::EstimationMethod::EXACT;
    
    // Camera calibration method
    std::unordered_map<std::string,CameraCalibrator::CameraModel> camera_model_hash;
    camera_model_hash["pinhole"] = CameraCalibrator::CameraModel::PINHOLE;
    camera_model_hash["fisheye"] = CameraCalibrator::CameraModel::FISHEYE;

    try {

        po::options_description options("INFO");
        options.add_options()
                ("help", "Produce help message.")
                ("version,v", "Print version information.")
                ;

        po::options_description config("CONFIGURATION");
        config.add_options()
                ("calibration-path,f", po::value<std::string>(&save_path),
                "The base configuration file location.\n"
                "The timestamp of the calibration will be prepended to th name."
                "If not provided, the calibration info will be printed to STDOUT.")
                ("homography-method", po::value<std::string>(&homography_method),
                "Homography estimation method.\n\n"
                "Values:\n"
                "  robust (default): RANSAC-based robust estimation method (automatic outlier rejection).\n"
                "  regular: Best-fit using all data points.\n"
                "  exact: Compute the homography that fits four points. Useful when frames contain know fiducial marks.\n")
                ("camera-model", po::value<std::string>(&camera_model),
                "Model used for camera calibration.\n\n"
                "Values:\n"
                "  pinhole (default): Pinhole camera model.\n"
                "  fisheye: Fisheye camera model (ultra wide-angle lens with pronounced radial distortion.\n")
                ("chessboard-height,h", po::value<int>(&chessboard_height),
                "The number of vertical black squares in the chessboard used for calibration.\n")
                ("chessboard-width,w", po::value<int>(&chessboard_width),
                "The number of horizontal black squares in the chessboard used for calibration.\n")
                ("square-width,W", po::value<double>(&square_length),
                "The length/width of a single chessboard square in meters.\n")
                ("config-file,c", po::value<std::string>(&config_file), "Configuration file.")
                ("config-key,k", po::value<std::string>(&config_key), "Configuration key.")
                ;

        po::options_description hidden("HIDDEN OPTIONS");
        hidden.add_options()
                ("type", po::value<std::string>(&type),
                "Type of frame calibrator to use.\n\n"
                "Values:\n"
                "  camera: Generate calibration parameters (camera matrix and distortion coefficients).\n"
                "  homography: Generate homography transform between pixels and world units.")
                
                ("source", po::value<std::string>(&source),
                "The name of the SOURCE that supplies images on which to perform background subtraction."
                "The server must be of type SMServer<SharedCVMatHeader>\n")
                ;

        po::positional_options_description positional_options;
        positional_options.add("type", 1);
        positional_options.add("source", 1);

        visible_options.add(options).add(config);

        po::options_description all_options("All options");
        all_options.add(options).add(config).add(hidden);

        po::variables_map variable_map;
        po::store(po::command_line_parser(argc, argv)
                .options(all_options)
                .positional(positional_options)
                .run(),
                variable_map);
        po::notify(variable_map);

        // Use the parsed options
        if (variable_map.count("help")) {
            printUsage(visible_options);
            return 0;
        }

        if (variable_map.count("version")) {
            std::cout << "Oat calibrator version "
                      << Oat_VERSION_MAJOR
                      << "."
                      << Oat_VERSION_MINOR
                      << "\n";
            std::cout << "Written by Jonathan P. Newman in the MWL@MIT.\n";
            std::cout << "Licensed under the GPL3.0.\n";
            return 0;
        }

        if (!variable_map.count("type")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A TYPE must be specified.\n");
            return -1;
        }

        if (!variable_map.count("source")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A SOURCE must be specified.\n");
            return -1;
        }

        if (!variable_map.count("calibration-path")) {
            save_path = bfs::current_path().string();
        }
        
        // Check to see that homography-method is valid
        if (homo_meth_hash.find(homography_method) == homo_meth_hash.end()) {
            printUsage(visible_options);
            std::cerr << oat::Error("Unrecognized homography-method.\n");
            return -1;
        }
        
        // Check that chessboard height and width are valid
        if (type_hash[type] == 'a' && (chessboard_height < 2 || chessboard_width < 2)) {
            printUsage(visible_options);
            std::cerr << oat::Error("Camera calibration requires chessboard to be at least 2x2.\n");
            return -1;
        }

        if ((variable_map.count("config-file") && !variable_map.count("config-key")) ||
                (!variable_map.count("config-file") && variable_map.count("config-key"))) {
            printUsage(visible_options);
            std::cerr << oat::Error("A configuration file must be supplied with a corresponding config-key.\n");
            return -1;
        } else if (variable_map.count("config-file")) {
            config_used = true;
        }

    } catch (std::exception& e) {
        std::cerr << oat::Error(e.what()) << "\n";
        return -1;
    } catch (...) {
        std::cerr << oat::Error("Exception of unknown type.\n");
        return -1;
    }

    // Create component
    std::shared_ptr<Calibrator> calibrator;

    // Refine component type
    switch (type_hash[type]) {
        case 'a':
        {
            auto chessboard_size = cv::Size(chessboard_height, chessboard_width);
            auto model =camera_model_hash.at(camera_model);
            calibrator = std::make_shared<CameraCalibrator>(source, model, chessboard_size, square_length);
            break;
        }
        case 'b':
        {
            auto meth = homo_meth_hash.at(homography_method);
            calibrator = std::make_shared<HomographyGenerator>(source, meth);
            break;
        }
        default:
        {
            printUsage(visible_options);
            std::cerr << oat::Error("Invalid TYPE specified.\n");
            return -1;
        }
    }

    try{

        if (config_used)
            calibrator->configure(config_file, config_key);

        // Generate the save path and check validity
        calibrator->generateSavePath(save_path);

        // Tell user
        std::cout << oat::whoMessage(calibrator->name(),
                "Listening to source " + oat::sourceText(source) + ".\n")
                << oat::whoMessage(calibrator->name(),
                "Press CTRL+C to exit.\n");

        // Infinite loop until ctrl-c or end of stream signal
        run(calibrator);

        // Tell user
        std::cout << oat::whoMessage(calibrator->name(), "Exiting.\n");

        // Exit success
        return 0;

    } catch (const cpptoml::parse_exception& ex) {
        std::cerr << oat::whoError(calibrator->name(),
                     "Failed to parse configuration file "
                     + config_file + "\n")
                  << oat::whoError(calibrator->name(), ex.what())
                  << "\n";
    } catch (const std::runtime_error& ex) {
        std::cerr << oat::whoError(calibrator->name(),ex.what())
                  << "\n";
    } catch (const cv::Exception& ex) {
        std::cerr << oat::whoError(calibrator->name(), ex.what())
                  << "\n";
    } catch (...) {
        std::cerr << oat::whoError(calibrator->name(), "Unknown exception.\n");
    }

    // Exit failure
    return -1;
}
