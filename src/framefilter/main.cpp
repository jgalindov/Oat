//******************************************************************************
//* File:   oat framefilt main.cpp
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

#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/program_options.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <cpptoml.h>

#include "../../lib/utility/IOFormat.h"
#include "../../lib/utility/ProgramOptions.h"

#include "FrameFilter.h"
#include "BackgroundSubtractor.h"
#include "BackgroundSubtractorMOG.h"
#include "FrameMasker.h"
#include "Undistorter.h"

#define REQ_POSITIONAL_ARGS 3

namespace po = boost::program_options;

volatile sig_atomic_t quit = 0;
volatile sig_atomic_t source_eof = 0;

const char usage_string[] =
    "Usage: framefilt [INFO]\n"                                                  
    "   or: framefilt TYPE SOURCE SINK [CONFIGURATION]\n"
    "Filter frames from SOURCE and published filtered frames "
    "to SINK.\n\n"
    "TYPE\n"
    "  bsub: Background subtraction\n"
    "  mask: Binary mask\n"
    "  mog: Mixture of Gaussians background segmentation.\n"
    "  undistort: Compensate for lens distortion using distortion model.\n\n"
    "SOURCE:\n"
    "  User-supplied name of the memory segment to receive frames "
    "from (e.g. raw).\n\n"
    "SINK:\n"
    "  User-supplied name of the memory segment to publish frames "
    "to (e.g. filt).\n";


const char usage_string_special[] =
    "Filter frames from SOURCE and published filtered frames "
    "to SINK.\n\n"
    "SOURCE:\n"
    "  User-supplied name of the memory segment to receive frames "
    "from (e.g. raw).\n\n"
    "SINK:\n"
    "  User-supplied name of the memory segment to publish frames "
    "to (e.g. filt).\n";



void printUsage(const po::options_description &options,
                const std::string &type="") {

    if (type.empty())
        std::cout << usage_string;
    else
        std::cout << "Usage: framefilt " << type << " SOURCE SINK [CONFIGURATION]\n" 
                  << usage_string_special;

    // Generated options
    std::cout << options << "\n";
}

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int) {
    quit = 1;
}

// Processing loop
void run(const std::shared_ptr<oat::FrameFilter> &filter) {

    try {

        filter->connectToNode();

        while (!quit && !source_eof) {
            source_eof = filter->processFrame();
        }

    } catch (const boost::interprocess::interprocess_exception &ex) {

        // Error code 1 indicates a SIGNINT during a call to wait(), which
        // is normal behavior
        if (ex.get_error_code() != 1)
            throw;
    }
}

int main(int argc, char *argv[]) {

    std::signal(SIGINT, sigHandler);

    // Results of command line input
    std::string type;
    std::string source;
    std::string sink;
    std::vector<std::string> config_fk;
    bool config_used = false;

    // Component specializations
    std::unordered_map<std::string, char> type_hash;
    type_hash["bsub"] = 'a';
    type_hash["mask"] = 'b';
    type_hash["mog"] = 'c';
    type_hash["undistort"] = 'd';

    // The component itself
    std::string comp_name = "framefilt";
    std::shared_ptr<oat::FrameFilter> filter;

    // Program options
    po::options_description visible_options;

    try {

        // Component configuration
        // TODO: config file/key is actually type-specific and should be
        // removed from this...
        po::options_description config_opt_desc("CONFIGURATION");
        config_opt_desc.add_options()
                ("config,c", po::value<std::vector<std::string> >()->multitoken(),
                "Configuration file/key pair.")
                ;

        // Required positional options
        po::options_description positional_opt_desc("POSITIONAL");
        positional_opt_desc.add_options()
                ("type", po::value<std::string>(&type),
                 "Type of frame filter to use.")
                ("source", po::value<std::string>(&source),
                 "User-supplied name of the memory segment to receive frames.")
                ("sink", po::value<std::string>(&sink),
                 "User-supplied name of the memory segment to publish frames.")
                ("type-args", po::value<std::vector<std::string> >(), 
                 "type-specifuc arguments.")
                ;

        // Required positional arguments and type-specific configuration
        po::positional_options_description positional_options;
        positional_options.add("type", 1);
        positional_options.add("source", 1);
        positional_options.add("sink", 1);
        positional_options.add("type-args", -1);

        // Visible options for help message
        visible_options.add(oat::ComponentInfo::instance()->get()); 

        // All options, including positional
        po::options_description options;
        options.add(config_opt_desc).add(positional_opt_desc).add(oat::ComponentInfo::instance()->get());

        // Parse options, including unrecongized options which may be TYPE-specific
        auto parsed_opt = po::command_line_parser(argc, argv)
            .options(options)
            .positional(positional_options)
            .allow_unregistered()
            .run();

        po::variables_map option_map;
        po::store(parsed_opt, option_map);

        // Check options for errors and bind options to local variables
        po::notify(option_map);

        // If a TYPE was provided, then specialize the filter and coresponding
        // program options
        if (option_map.count("type")) {

            // Refine component type
            switch (type_hash[type]) {
                case 'a':
                {
                    filter = std::make_shared<oat::BackgroundSubtractor>(source, sink);
                    break;
                }
                case 'b':
                {
                    filter = std::make_shared<oat::FrameMasker>(source, sink);
                    // TODO: Remove. Let the component worry about reporting
                    // this if the mask is not set when it starts
                    if (!config_used)
                         std::cerr << oat::whoWarn(comp_name,
                                 "No mask configuration was provided."
                                 " This filter does nothing but waste CPU cycles.\n");
                    break;
                }
                case 'c':
                {
                    filter = std::make_shared<oat::BackgroundSubtractorMOG>(source, sink);
                    break;
                }
                case 'd':
                {
                    filter = std::make_shared<oat::Undistorter>(source, sink);
                    // TODO: Remove. Let the component worry about reporting
                    // this if matricies are not defined when it starts
                    if (!config_used)
                         std::cerr << oat::whoWarn(comp_name,
                                 "No undistortion configuration was provided."
                                 " This filter does nothing but waste CPU cycles.\n");
                    break;
                }
                default:
                {
                    printUsage(visible_options);
                    std::cerr << oat::Error("Invalid TYPE specified.\n");
                    return -1;
                }
            }

            // Specialize program options for the selected TYPE
            filter->updateProgramOptions(config_opt_desc);
            visible_options.add(config_opt_desc);
        }

        if (option_map.count("help")) {
            printUsage(visible_options, type);
            return 0;
        }

        if (option_map.count("version")) {
            std::cout << oat::VERSION_STRING;
            return 0;
        }

        if (!option_map.count("type")) {
            printUsage(visible_options);
            std::cout << oat::Error("A TYPE must be specified.\n");
            return -1;
        }

        if (!option_map.count("source")) {
            printUsage(visible_options);
            std::cout << oat::Error("A SOURCE must be specified.\n");
            return -1;
        }

        if (!option_map.count("sink")) {
            printUsage(visible_options);
            std::cerr << oat::Error("A SINK must be specified.\n");
            return -1;
        }

        // Get specialized component name
        comp_name = filter->name();

        // Check for configuration file and key
        // TODO: To library routine
        if (!option_map["config"].empty()) {

            config_fk = option_map["config"].as<std::vector<std::string> >();

            if (config_fk.size() == 2) {
                config_used = true;
            } else {
                printUsage(visible_options);
                std::cerr << oat::Error("Configuration must be supplied as file key pair.\n");
                return -1;
            }
        }

        auto special_opt = 
            po::collect_unrecognized(parsed_opt.options, po::include_positional);
        special_opt.erase(special_opt.begin(),special_opt.begin() + REQ_POSITIONAL_ARGS);

        po::store(parsed_opt, option_map);
        po::notify(option_map);

        // TODO
        //filter->configure(special_opt);
        
        //// Add specialized options and reparse if needed
        //if (filter->updateProgramOptions(config_opt_desc)) {

        //    // Visible options for help message
        //    visible_options.add(info_opt_desc).add(config_opt_desc);

        //    // All options, including positional
        //    po::options_description options;
        //    options.add(info_opt_desc).add(config_opt_desc).add(positional_opt_desc);

        //    po::variables_map option_map;
        //    po::store(po::command_line_parser(argc, argv)
        //            .options(options)
        //            .positional(positional_options)
        //            .run(),
        //            option_map);

        //    // Check options for errors (must be after help and version checks)
        //    po::notify(option_map);

        //}


        // Process configuration file if provided
        if (config_used)
            filter->configure(config_fk[0], config_fk[1]);

        // Tell user
        std::cout << oat::whoMessage(comp_name,
                "Listening to source " + oat::sourceText(source) + ".\n")
                << oat::whoMessage(comp_name,
                "Steaming to sink " + oat::sinkText(sink) + ".\n")
                << oat::whoMessage(comp_name,
                "Press CTRL+C to exit.\n");

        // Infinite loop until ctrl-c or end of stream signal
        run(filter);

        // Tell user
        std::cout << oat::whoMessage(comp_name, "Exiting.")
                  << std::endl;

        // Exit success
        return 0;

    } catch (const po::error &ex) {
        printUsage(visible_options);
        std::cerr << oat::whoError(comp_name, ex.what()) << std::endl;
    } catch (const cpptoml::parse_exception &ex) {
        std::cerr << oat::whoError(comp_name,
                     "Failed to parse configuration file " + config_fk[0] + "\n")
                  << oat::whoError(comp_name, ex.what())
                  << std::endl;
    } catch (const std::runtime_error &ex) {
        std::cerr << oat::whoError(comp_name,ex.what()) << std::endl;
    } catch (const cv::Exception &ex) {
        std::cerr << oat::whoError(comp_name, ex.what()) << std::endl;
    } catch (...) {
        std::cerr << oat::whoError(comp_name, "Unknown exception.")
                  << std::endl;
    }

    // Exit failure
    return -1;

}
