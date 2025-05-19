#ifndef MALLOB_CBMC_SOLVER_HPP
#define MALLOB_CBMC_SOLVER_HPP

#include "interface/api/api_connector.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include "data/job_result.hpp"
//#include "libcprover-cpp/api.h"
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
      }

      int res = 42;
      
      if (!_params.unwindLoops().empty()) {
        int EC = 42;
        
        std::vector<int> unwind_values = {2, 6, 12, 17, 21, 40, 200, 400, 1025, 2049, 268435456};
        cbmcOptions.push_back("--unwind");

        for (int unwind_value : unwind_values) {
          cbmcOptions.push_back(std::to_string(unwind_value));
          auto [res_unwind, contains_successful, contains_failed] = run_cbmc(cbmcOptions);
          EC = res_unwind;

          if (EC == 0) {
            if (!contains_successful) {
              EC = 1;
            } else {
              cbmcOptions.push_back("--unwinding-assertions");
              auto [res_unwind2, contains_successful2, contains_failed2] = run_cbmc(cbmcOptions);
              cbmcOptions.pop_back();
              if (res_unwind2 != 0) {
                EC = 42;
              }
            }
          }
          if (EC == 10){
            if (!contains_failed) {
              EC = 1;
            }
          }

          cbmcOptions.pop_back();
          if (EC == 42) {
            continue;
          } else {
            break;
          }
        }
        res = EC;

      } else {
        auto [res_unwind, contains_successful, contains_failed] = run_cbmc(cbmcOptions);
        res = res_unwind;
      }


      // if (!_params.s2f().empty()) {
      //   std::ofstream outputFile(_params.s2f(), std::ios::app);
      //   outputFile << cbmc_output;
      //   outputFile.close();
      // } else {
      //   LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", cbmc_output.c_str());
      //   LOG(V2_INFO, "End of CBMC output----------------------------------\n");
      // }
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
      
      JobResult r;
      r.id = _desc.getId();
      r.revision = 0;
      if (res == 0) {
        r.result = 20;
      } else if (res == 10) {
        r.result = 10;
      } else {
        r.result = res;
      }
      
      return r;
    }


    std::tuple<int, bool, bool> run_cbmc(std::vector<std::string> cbmcOptions) {
      int size_options = cbmcOptions.size();
      int argc = 2 + size_options;
      
      // Use vector to manage memory safely
      std::vector<std::string> arg_strings;
      arg_strings.reserve(argc);
      std::vector<const char*> argv;
      argv.reserve(argc + 1);  // +1 for null terminator
      
      // Add cbmc command
      arg_strings.push_back("cbmc");
      argv.push_back(arg_strings.back().c_str());
      
      // Add filename
      arg_strings.push_back(_filename);
      argv.push_back(arg_strings.back().c_str());

      // Add options
      for (const auto& opt : cbmcOptions) {
          arg_strings.push_back(opt);
          argv.push_back(arg_strings.back().c_str());
      }
      
      // Add null terminator
      argv.push_back(nullptr);

      // Log all arguments
      for (int i = 0; i < argc; i++) {
        LOG(V2_INFO, "CBMC argv[%d]: %s\n", i, argv[i]);
      }

      cbmc_parse_optionst parse_options(argc, argv.data());
      
      // Run CBMC
      std::stringstream buffer;
      std::stringstream buffer_err;
      std::streambuf* old = std::cout.rdbuf(buffer.rdbuf());
      std::streambuf* old_err = std::cerr.rdbuf(buffer_err.rdbuf());

      int res = parse_options.main();
      bool contains_successful = false;
      bool contains_failed = false;

      std::cout.rdbuf(old);
      std::cerr.rdbuf(old_err);
      
      // Write output immediately after run
      std::string cbmc_output = buffer.str() + buffer_err.str() + "\n" + "CBMCexitcode:" + std::to_string(res) + "\n";
      
      if (cbmc_output.find("VERIFICATION SUCCESSFUL") != std::string::npos) {
        contains_successful = true;
      }
      if (cbmc_output.find("VERIFICATION FAILED") != std::string::npos) {
        contains_failed = true;
      }

      if (!_params.s2f().empty()) {
        std::ofstream outputFile(_params.s2f(), std::ios::app);
        outputFile << cbmc_output; 
        outputFile << "\nEC=" + std::to_string(res) + "\n";
        outputFile.close();
      } else {
        LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", cbmc_output.c_str());
        LOG(V2_INFO, "End of CBMC output----------------------------------\n");
      }
      
      // if (res == 0) {
      //   contains_successful = true;
      // } else if (res == 10) {
      //   contains_failed = true;
      // }

      return std::make_tuple(res, contains_successful, contains_failed);
    }
};

#endif // CBMC_SOLVER_HPP