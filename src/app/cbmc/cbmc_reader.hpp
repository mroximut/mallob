
#ifndef MALLOB_CBMC_READER_HPP
#define MALLOB_CBMC_READER_HPP

#include <string>
#include <vector>

#include "data/job_description.hpp"

class CBMCReader {

public:
    CBMCReader() {

    }

    ~CBMCReader() = default;

    bool read(const std::string& filename, JobDescription& desc) {
        //auto& config = desc.getAppConfiguration();
        //desc.beginInitialization(desc.getRevision());
        //desc.endInitialization();
        return true;
    }
};

#endif