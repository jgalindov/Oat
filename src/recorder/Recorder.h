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

#ifndef RECORDER_H
#define RECORDER_H

#include <string>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include "../../lib/rapidjson/filewritestream.h"
#include "../../lib/rapidjson/prettywriter.h"
#include "../../lib/shmem/MatClient.h"
#include "../../lib/shmem/SMClient.h"
#include "../../lib/datatypes/Position2D.h"

class Recorder {
public: 

    // Both positions and images
    Recorder(const std::vector<std::string>& position_source_names,
            const std::vector<std::string>& frame_source_names,
            std::string save_path = ".",
            std::string file_name = "",
            const bool& append_date = false,
            const int& frames_per_second = 30);

    ~Recorder();

    // Recorder must be configurable
    void configure(const std::string& config_file, const std::string& config_key);

    // Recorder must be able to write all requested streams to file(s)
    void writeStreams(void);

private:

    // General file name formatting
    std::string save_path;
    std::string file_name;
    const bool append_date;
    
    // Video files
    const int frames_per_second;
    std::vector<std::string> video_file_names;
    std::vector<cv::VideoWriter*> video_writers;
    
    // Position file
    FILE* position_fp;
    char position_write_buffer[65536];
    rapidjson::FileWriteStream * file_stream;
    rapidjson::Writer<rapidjson::FileWriteStream> json_writer;

    // Image sources
    std::vector<shmem::MatClient*> frame_sources;
    std::vector<cv::Mat*> frames;
    std::vector<shmem::MatClient>::size_type frame_client_idx;
    
    // Position sources
    std::vector<shmem::SMClient<datatypes::Position2D>* > position_sources;
    std::vector<datatypes::Position2D* > source_positions;
    std::vector<shmem::SMClient<datatypes::Position2D> >::size_type position_client_idx;
    std::vector<std::string> position_labels;

    void openFiles(const std::vector<std::string>& save_path,
            const bool& save_positions,
            const bool& save_images);

    void initializeWriter(cv::VideoWriter& writer,
            const std::string& file_name,
            const cv::Mat& image);

    bool checkFile(std::string& file);

    void writeFramesToFile(void);
    void writePositionsToFile(void);

};

#endif // RECORDER_H