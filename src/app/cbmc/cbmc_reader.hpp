
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

    bool read(const Parameters& params, const std::string& filename, JobDescription& desc) {
        //auto& config = desc.getAppConfiguration();
        //desc.beginInitialization(desc.getRevision());
        //desc.endInitialization();
        const std::string NC_DEFAULT_VAL = "BMMMKKK111";
        desc.setAppConfigurationEntry("__NV", NC_DEFAULT_VAL);
        desc.setAppConfigurationEntry("__NC", NC_DEFAULT_VAL);
        
        AppConfiguration& config = desc.getAppConfiguration();

        desc.beginInitialization(0);
        StaticStore<std::string>::insert("cbmc-jobdesc-#" + std::to_string(desc.getId()), filename);
        desc.endInitialization();
            
        return true;
    }
};

#endif