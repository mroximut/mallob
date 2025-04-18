#ifndef MALLOB_CBMC_SOLVER_HPP
#define MALLOB_CBMC_SOLVER_HPP

#include "interface/api/api_connector.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include "data/job_result.hpp"
#include "libcprover-cpp/api.h"
#include "util/sys/thread_pool.hpp"
#include <iostream>

class CBMCSolver {

private:
    std::string _filename;
    Parameters _params;
    APIConnector& _api;
    JobDescription& _desc;

public:
    CBMCSolver(const Parameters& params, APIConnector& api, JobDescription& desc) : 
        _params(params), _api(api), _desc(desc) {
        //_filename = desc.getAppConfiguration().map.at("file");
        _filename = params.monoFilename();
    }
    ~CBMCSolver() = default;

    JobResult solve() {

      api_sessiont api(api_optionst::create());
          
      std::vector<std::string> output;
      const auto write_output =
        [](const api_messaget &message, api_call_back_contextt context) {
          std::vector<std::string> &output =
            *static_cast<std::vector<std::string> *>(context);
          output.emplace_back(api_message_get_string(message));
        };
    
      // Set the callback for the API
      api.set_message_callback(write_output, &output);

      LOG(V2_INFO, "Loading model from %s\n", _filename.c_str()); 

      api.load_model_from_files({_filename});
    
      LOG(V2_INFO, "Running verification...\n");
      
      // ProcessWideThreadPool::get().addTask([&api]() {
      //     auto result = api.verify_model();
      // });
      api.verify_model();
      
      for(const auto &message : output)
      {
        LOG(V2_INFO, "%s\n", message.c_str());  
      }
      
      JobResult r;
      r.id = _desc.getId();
      r.revision = 0;
      r.result = 0;
      return r;
    }
};

#endif // CBMC_SOLVER_HPP