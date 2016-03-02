//******************************************************************************
//* File:   oat record main.cpp
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

#include <algorithm>
#include <csignal>
#include <thread>
#include <pthread.h> // TODO: POSIX specific
#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <zmq.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/program_options.hpp>

#include "../../lib/utility/ZMQStream.h"
#include "../../lib/utility/IOFormat.h"
#include "../../lib/utility/IOUtility.h"

#include "RecordControl.h"
#include "Recorder.h"

namespace po = boost::program_options;

volatile sig_atomic_t quit = 0;
volatile sig_atomic_t source_eof = 0;

// Needed by both threads
std::string file_name;
std::string save_path;
bool allow_overwrite = false;
bool prepend_timestamp = false;

// ZMQ stream
using zmq_istream_t = boost::iostreams::stream<oat::zmq_istream>;
using zmq_ostream_t = boost::iostreams::stream<oat::zmq_ostream>;

// Interactive commands
bool recording_on = true;
enum class ControlMode : int16_t
{
    NONE = 0,
    LOCAL = 1,
    RPC = 2,
} control_mode;

void printUsage(std::ostream& out, po::options_description options) {
    out << "Usage: record [INFO]\n"
        << "   or: record [CONFIGURATION]\n"
        << "Record frame and/or position streams.\n\n"
        << options << "\n";
}

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int) {
    quit = 1;
}

// Cleanup procedure for interactive sessions
void cleanup(std::thread &proc_thread) {

    // Reinstall SIGINT handler and trigger it on both threads
    std::signal(SIGINT, sigHandler);
    pthread_kill(proc_thread.native_handle(), SIGINT);

    // Join recorder and UI threads
    proc_thread.join();
}

// Processing loop
void run(std::shared_ptr<oat::Recorder>& recorder) {

    try {

        recorder->connectToNodes();

        // Initialize recording
        recorder->initializeRecording(save_path,
                                      file_name,
                                      prepend_timestamp,
                                      allow_overwrite);

        while (!quit && !source_eof) {
            source_eof = recorder->writeStreams();
        }

    } catch (const boost::interprocess::interprocess_exception &ex) {

        // Error code 1 indicates a SIGINT during a call to wait(), which
        // is normal behavior
        if (ex.get_error_code() != 1)
            throw;
    }
}

int main(int argc, char *argv[]) {

    std::signal(SIGINT, sigHandler);

    std::vector<std::string> frame_sources;
    std::vector<std::string> position_sources;
    std::string rpc_endpoint;

    try {

        po::options_description options("INFO");
        options.add_options()
                ("help", "Produce help message.")
                ("version,v", "Print version information.")
                ;

        po::options_description configuration("CONFIGURATION");
        configuration.add_options()
                ("filename,n", po::value<std::string>(&file_name),
                "The base file name to which to source name will be appended")
                ("folder,f", po::value<std::string>(&save_path),
                "The path to the folder to which the video stream and position information will be saved.")
                ("date,d",
                "If specified, YYYY-MM-DD-hh-mm-ss_ will be prepended to the filename.")
                ("allow-overwrite,o",
                "If set and save path matches and existing file, the file will be overwritten instead of"
                "a numerical index being added to the file path.")
                ("position-sources,p", po::value< std::vector<std::string> >()->multitoken(),
                "The names of the POSITION SOURCES that supply object positions to be recorded.")
                ("interactive", "Start recorder with interactive controls enabled.")
                ("rpc-endpoint", po::value<std::string>(&rpc_endpoint),
                 "Yield interactive control of the recorder to a remote source.")
                ("frame-sources,s", po::value< std::vector<std::string> >()->multitoken(),
                "The names of the FRAME SOURCES that supply images to save to video.")
                ;

        po::options_description all_options("OPTIONS");
        all_options.add(options).add(configuration);

        po::variables_map variable_map;
        po::store(po::command_line_parser(argc, argv)
                .options(all_options)
                .run(),
                variable_map);
        po::notify(variable_map);

        // Use the parsed options
        if (variable_map.count("help")) {
            printUsage(std::cout, all_options);
            return 0;
        }

        if (variable_map.count("version")) {
            std::cout << "Oat Recorder version "
                      << Oat_VERSION_MAJOR
                      << "."
                      << Oat_VERSION_MINOR
                      << "\n";
            std::cout << "Written by Jonathan P. Newman in the MWL@MIT.\n";
            std::cout << "Licensed under the GPL3.0.\n";
            return 0;
        }

        if (!variable_map.count("position-sources") && !variable_map.count("frame-sources")) {
            printUsage(std::cout, all_options);
            std::cerr << oat::Error("At least a single POSITION SOURCE or FRAME SOURCE must be specified.\n");
            return -1;
        }

        if (!variable_map.count("folder")) {
            save_path = ".";
            std::cerr << oat::Warn("Warning: Saving files to the current directory.\n");
        }

        if (!variable_map.count("filename")) {
            file_name = "";
            std::cerr << oat::Warn("Warning: No base filename was provided.\n");
        }

        if (variable_map.count("interactive") && variable_map.count("rpc-endpoint")) {
            std::cerr << oat::Error("Recorder cannot be controlled both interactively and from a remote endpoint.\n");
            return -1;
        } else if (variable_map.count("interactive")) {
            control_mode = ControlMode::LOCAL;
        } else if (variable_map.count("rpc-endpoint")) {
            control_mode = ControlMode::RPC;
        } else {
            control_mode = ControlMode::NONE;
        }

        // May contain image source and sink information
        if (variable_map.count("position-sources")) {
            position_sources = variable_map["position-sources"].as< std::vector<std::string> >();

            // Assert that all positions sources are unique. If not, remove duplicates, and issue warning.
            std::vector<std::string>::iterator it;
            it = std::unique (position_sources.begin(), position_sources.end());
            if (it != position_sources.end()) {
                position_sources.resize(std::distance(position_sources.begin(),it));
                std::cerr << oat::Warn("Warning: duplicate position sources have been removed.\n");
            }
        }

        if (variable_map.count("frame-sources")) {
            frame_sources = variable_map["frame-sources"].as< std::vector<std::string> >();

            // Assert that all positions sources are unique. If not, remove duplicates, and issue warning.
            std::vector<std::string>::iterator it;
            it = std::unique (frame_sources.begin(), frame_sources.end());
            if (it != frame_sources.end()) {
                frame_sources.resize(std::distance(frame_sources.begin(),it));
                 std::cerr << oat::Warn("Warning: duplicate frame sources have been removed.\n");
            }
        }

        if (variable_map.count("date"))
            prepend_timestamp = true;

        if (variable_map.count("allow-overwrite"))
            allow_overwrite = true;


    } catch (std::exception& e) {
        std::cerr << oat::Error(e.what()) << "\n";
        return -1;
    } catch (...) {
        std::cerr << oat::Error("Exception of unknown type.\n");
        return -1;
    }

    // Create component
    auto recorder = std::make_shared<oat::Recorder>(position_sources, frame_sources);

    // Tell user
    if (!frame_sources.empty()) {

        std::cout << oat::whoMessage(recorder->name(),
                "Listening to frame sources ");

        for (auto s : frame_sources)
            std::cout << oat::sourceText(s) << " ";

        std::cout << ".\n";
    }

    if (!position_sources.empty()) {

        std::cout << oat::whoMessage(recorder->name(),
                "Listening to position sources ");

        for (auto s : position_sources)
            std::cout << oat::sourceText(s) << " ";

        std::cout << ".\n";
    }

    std::cout << oat::whoMessage(recorder->name(),
                 "Press CTRL+C to exit.\n");

    // The business
    try {

        switch (control_mode)
        {
            case ControlMode::NONE :
            {
                // Start the recorder w/o controls
                run(recorder);

                break;
            }
            case ControlMode::LOCAL :
            {
                // For interactive control, recorder must be started by user
                recorder->set_record_on(false);

                // Start recording in background
                std::thread process(run, std::ref(recorder));

                // Interact using stdin
                oat::printInteractiveUsage(std::cout);
                oat::controlRecorder(std::cin, std::cout, *recorder, true);

                // Interupt and join threads
                cleanup(process);

                break;
            }
            case ControlMode::RPC :
            {
                // For interactive control, recorder must be started by user
                recorder->set_record_on(false);

                // Start recording in background
                std::thread process(run, std::ref(recorder));

                try {
                    auto ctx = std::make_shared<zmq::context_t>(1);
                    auto sock = std::make_shared<zmq::socket_t>(*ctx, ZMQ_REP);
                    sock->bind(rpc_endpoint);
                    zmq_istream_t in(ctx, sock);
                    zmq_ostream_t out(ctx, sock);
                    oat::printRemoteUsage(std::cout);
                    oat::controlRecorder(in, out, *recorder, false);
                } catch (const zmq::error_t &ex) {
                    std::cerr << oat::whoError(recorder->name(), "zeromq error: "
                            + std::string(ex.what())) << "\n";
                    cleanup(process);
                    return -1;
                }

                // Interupt and join threads
                cleanup(process);

                break;
            }
        }

        // Exit
        std::cout << oat::whoMessage(recorder->name(), "Exiting.\n");
        return 0;

    } catch (const std::runtime_error &ex) {
        std::cerr << oat::whoError(recorder->name(), ex.what()) << "\n";
    } catch (const cv::Exception &ex) {
        std::cerr << oat::whoError(recorder->name(), ex.what()) << "\n";
    } catch (const boost::interprocess::interprocess_exception &ex) {
        std::cerr << oat::whoError(recorder->name(), ex.what()) << "\n";
    } catch (...) {
        std::cerr << oat::whoError(recorder->name(), "Unknown exception.\n");
    }

    // Exit failure
    return -1;
}
