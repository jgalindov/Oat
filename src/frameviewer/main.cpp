//******************************************************************************
//* Copyright (c) Jon Newman (jpnewman at mit snail edu) 
//* All right reserved.
//* This file is part of the Simple Tracker project.
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

#include "Viewer.h"

#include <string>
#include <csignal>
#include <boost/program_options.hpp>

#include "../../lib/shmem/Signals.h"

namespace po = boost::program_options;

volatile sig_atomic_t quit = 0;

// Signal handler to ensure shared resources are cleaned on exit due to ctrl-c
void sigHandler(int s) {
    quit = 1;
}

void run(Viewer* viewer) {

    while ((viewer->showImage() != oat::ServerRunState::END) && !quit) { }
}

void printUsage(po::options_description options) {
    std::cout << "Usage: view [INFO]\n"
            << "     or: view SOURCE [CONFIGURATION]\n"
            << "View the output of a frame SOURCE.\n\n"
            << options << "\n";
}

int main(int argc, char *argv[]) {
    
    std::signal(SIGINT, sigHandler);

    std::string source;
    std::string file_name;
    std::string save_path;
    po::options_description visible_options("OPTIONS");

    try {

        po::options_description options("INFO");
        options.add_options()
                ("help", "Produce help message.")
                ("version,v", "Print version information.")
                ;

        po::options_description config("CONFIGURATION");
        config.add_options()
                ("filename,n", po::value<std::string>(&file_name),
                "The base snapshot file name.\n"
                " - The the name of the SOURCE for this viewer will be appended to this name.\n"
                " - The timestamp of the snapshot will be prepended to this name.")
                ("folder,f", po::value<std::string>(&save_path),
                "The path to the folder to which the video stream and position information will be saved.")
                ;

        po::options_description hidden("HIDDEN OPTIONS");
        hidden.add_options()
                ("source", po::value<std::string>(&source),
                "The name of the server that supplies images to view."
                "The server must be of type server<SharedCVMatHeader>\n")
                ;

        po::positional_options_description positional_options;
        positional_options.add("source", -1);

        po::options_description all_options("ALL OPTIONS");
        all_options.add(options).add(hidden);

        visible_options.add(options).add(config);

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
            std::cout << "Simple-Tracker Viewer, version 1.0\n"; //TODO: Cmake managed versioning
            std::cout << "Written by Jonathan P. Newman in the MWL@MIT.\n";
            std::cout << "Licensed under the GPL3.0.\n";
            return 0;
        }

        if (!variable_map.count("source")) {
            printUsage(visible_options);
            std::cout << "Error: a SOURCE must be specified. Exiting.\n";
            return -1;
        }

        if (!variable_map.count("folder")) {
            save_path = ".";
            std::cout << "Warning: saving files to the current directory.\n";
        }

        if (!variable_map.count("filename")) {
            file_name = "";
            std::cout << "Warning: no base filename was provided.\n";
        }

    } catch (std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Exception of unknown type! " << std::endl;
    }

    // Make the viewer
    Viewer viewer(source, save_path, file_name);

        // Tell user
    std::cout << "\n"
              << "Viewer has begun listening to source \"" + source + "\".\n"
              << "Press 's' on the viewer window to take a snapshot of the currently displayed frame.\n"
              << "Use CTRL+C to exit.\n";

    // Infinite loop until ctrl-c or end of stream signal
    run(&viewer);

    // Tell user
    std::cout << "Viewer is exiting." << std::endl;

    // Exit
    return 0;
}