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
      std::string cbmcOptionsStr = "";
      int size_options = 0;
      std::vector<std::string> cbmcOptions;

      if (_params.cbmcOptions().empty()) {
          LOG(V0_CRIT, "No additional CBMC options provided!\n");
      } else {
          cbmcOptionsStr = _params.cbmcOptions();
          
          size_t start = 0, end = 0;
          
          if (cbmcOptionsStr.back() == ',') {
            cbmcOptionsStr.pop_back();
          }
          while ((end = cbmcOptionsStr.find(',', start)) != std::string::npos) {
              cbmcOptions.push_back(cbmcOptionsStr.substr(start, end - start));
              start = end + 1;
          }
          cbmcOptions.push_back(cbmcOptionsStr.substr(start));
          size_options = cbmcOptions.size();
      }
      
      int argc = 2 + size_options;
      const char* argv[argc];
      argv[0] = strdup("cbmc");
      argv[1] = strdup(_filename.c_str());
      //argv[2] = strdup("--verbosity");
      //argv[3] = strdup("9");
      
      if (size_options > 0) {
        for (size_t i = 0; i < size_options; i++) {
          argv[i+2] = strdup((cbmcOptions[i]).c_str());
        }
      }
      LOG(V2_INFO, "CBMC options: %s\n", cbmcOptionsStr.c_str());
      // Log all arguments
      for (int i = 0; i < argc; i++) {
        LOG(V2_INFO, "CBMC argv[%d]: %s\n", i, argv[i]);
      }
      cbmc_parse_optionst parse_options(argc, argv);
      //cbmc_parse_optionst parse_options2(argc, argv);
      
      // // Redirect stdout to capture CBMC output
      // std::stringstream buffer;
      // std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());

      // Run CBMC
      // Redirect stdout to capture CBMC output
      std::stringstream buffer;
      std::stringstream buffer_err;
      std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
      std::streambuf* old_err = std::cerr.rdbuf(buffer_err.rdbuf());
      
      int res = parse_options.main();
      
      // Restore stdout/stderr and get output
      std::cout.rdbuf(old);
      std::cerr.rdbuf(old_err);
      std::string cbmc_output = buffer.str() + buffer_err.str() + "\n" + "CBMCexitcode:" + std::to_string(res) + "\n";

      if (!_params.s2f().empty()) {
        std::ofstream outputFile(_params.s2f());
        outputFile << cbmc_output;
        outputFile.close();
      } else {
        LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", cbmc_output.c_str());
        LOG(V2_INFO, "End of CBMC output----------------------------------\n");
      }
      //int res2 = parse_options2.main(); 

      // // Restore stdout
      // std::cout.rdbuf(old);

      // // Extract runtime information if available
      // std::string bufferStr = buffer.str();

      // // Look for Runtime Solver information
      // std::size_t runtimePos = bufferStr.find("Runtime Solver:");
      // if (runtimePos != std::string::npos) {
      //   std::istringstream lineStream(bufferStr.substr(runtimePos));
      //   std::string label;
      //   double runtime;
      //   lineStream >> label >> label >> runtime; 
        
      //   LOG(V2_INFO, "Extracted solver runtime: %f seconds\n", runtime);
      // } else {
        
      // }
   
      // LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", buffer.str().c_str());
     
      for (int i = 0; i < argc; i++) {
        free((void*)argv[i]);
      }
      
      JobResult r;
      r.id = _desc.getId();
      r.revision = 0;
      r.result = res;
      
      return r;
    }
};

#endif // CBMC_SOLVER_HPP