
#ifndef MALLOB_2LS_READER_HPP
#define MALLOB_2LS_READER_HPP

#include <string>
#include <vector>

#include "data/job_description.hpp"

class TwoLSReader {

public:
    TwoLSReader() {

    }

    ~TwoLSReader() = default;

    bool read(const std::string& filename, JobDescription& desc) {
        //auto& config = desc.getAppConfiguration();
        //desc.beginInitialization(desc.getRevision());
        //desc.endInitialization();
        return true;
    }
};

#endif