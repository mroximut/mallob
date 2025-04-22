#ifndef MALLOB_CBMC_SOLVER_HPP
#define MALLOB_CBMC_SOLVER_HPP

#include "interface/api/api_connector.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include "data/job_result.hpp"
#include "libcprover-cpp/api.h"
#include "cbmc/cbmc_parse_options.h"
#include "util/sys/thread_pool.hpp"
#include <iostream>
#include <vector>

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

      // api_sessiont api(api_optionst::create());
          
      // std::vector<std::string> output;
      // const auto write_output =
      //   [](const api_messaget &message, api_call_back_contextt context) {
      //     std::vector<std::string> &output =
      //       *static_cast<std::vector<std::string> *>(context);
      //     output.emplace_back(api_message_get_string(message));
      //   };
    
      // // Set the callback for the API
      // api.set_message_callback(write_output, &output);

      // LOG(V2_INFO, "Loading model from %s\n", _filename.c_str()); 

      // api.load_model_from_files({_filename});
    
      // LOG(V2_INFO, "Running verification...\n");
      
      // // ProcessWideThreadPool::get().addTask([&api]() {
      // //     auto result = api.verify_model();
      // // });
      // api.verify_model();
      
      // for(const auto &message : output)
      // {
      //   LOG(V2_INFO, "%s\n", message.c_str());  
      // }
      std::string cbmcOptionsStr = _params.cbmcOptions();
      std::vector<std::string> cbmcOptions;
      size_t start = 0, end = 0;
      while ((end = cbmcOptionsStr.find(',', start)) != std::string::npos) {
          cbmcOptions.push_back(cbmcOptionsStr.substr(start, end - start));
          start = end + 1;
      }
      cbmcOptions.push_back(cbmcOptionsStr.substr(start));

      int argc = 4 + cbmcOptions.size();
      const char* argv[argc];
      argv[0] = strdup("cbmc");
      argv[1] = strdup(_filename.c_str());
      argv[2] = strdup("--verbosity");
      argv[3] = strdup("9");
      for (size_t i = 0; i < cbmcOptions.size(); i++) {
          argv[i+4] = strdup((cbmcOptions[i]).c_str());
      }

      cbmc_parse_optionst parse_options(argc, argv);
      
      // Redirect stdout to capture CBMC output
      std::stringstream buffer;
      std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

      // Run CBMC
      int res = parse_options.main();

      // Restore stdout
      std::cout.rdbuf(old);

      // Extract runtime information if available
      std::string bufferStr = buffer.str();

      // Look for Runtime Solver information
      std::size_t runtimePos = bufferStr.find("Runtime Solver:");
      if (runtimePos != std::string::npos) {
        std::istringstream lineStream(bufferStr.substr(runtimePos));
        std::string label;
        double runtime;
        lineStream >> label >> label >> runtime; 
        
        LOG(V2_INFO, "Extracted solver runtime: %f seconds\n", runtime);
      } else {
        
      }
   
      LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", buffer.str().c_str());
     
      for (int i = 0; i < argc; i++) {
        free((void*)argv[i]);
      }
      
      JobResult r;
      r.id = _desc.getId();
      r.revision = 0;
      r.result = 0;
      
      return r;
    }
};

#endif // CBMC_SOLVER_HPP