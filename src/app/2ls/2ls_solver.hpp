#ifndef MALLOB_2LS_SOLVER_HPP
#define MALLOB_2LS_SOLVER_HPP

#include "interface/api/api_connector.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include "data/job_result.hpp"
#include "2ls/2ls_parse_options.h"
#include "util/sys/thread_pool.hpp"
#include <iostream>
#include <vector>

#include "cbmc_sat_connector.hpp"

class TwoLSSolver
{

private:
    Parameters _params;
    APIConnector &_api;
    JobDescription &_desc;
    std::string _filename;

    std::tuple<int, bool, bool> runCBMC(const std::vector<std::string>& cbmcOptions)
    {
        int size_options = cbmcOptions.size();
        int argc = 2 + size_options;

        std::vector<const char *> argv;
        argv.reserve(argc + 1); // +1 for null terminator

        std::string name = "2ls";
        argv.push_back(name.c_str());
        argv.push_back(_filename.c_str());

        for (const auto &opt : cbmcOptions)
        {
            argv.push_back(opt.c_str());
        }

        argv.push_back(nullptr);

        for (int i = 0; i < argc; i++)
        {
            LOG(V2_INFO, "2LS argv[%d]: %s\n", i, argv[i]);
        }

        twols_parse_optionst parse_options(argc, argv.data());

        // Run CBMC
        std::stringstream buffer;
        std::stringstream buffer_err;
        std::streambuf *old;
        std::streambuf *old_err;

        if (!_params.solutionToFile().empty())
        {
            old = std::cout.rdbuf(buffer.rdbuf());
            old_err = std::cerr.rdbuf(buffer_err.rdbuf());
        }

        int res = 5;

        // std::promise<int> promise;
        // std::future<int> future_res = promise.get_future();

        // ProcessWideThreadPool::get().addTask([&]() {
        //     try {
        //         res = parse_options.main();
        //         promise.set_value(res);
        //     } catch (const std::exception &e) {
        //         LOG(V0_CRIT, "Error in 2LS %s: exiting with 5\n", e.what());
        //     } catch (...) {
        //         LOG(V0_CRIT, "Unknown error in 2LS: exiting with 5\n");
        //     }
        // });

        // while (future_res.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout) {
        //     if (Terminator::isTerminating()) {
        //         LOG(V0_CRIT, "2LS was terminated by the main thread\n");
        //         return std::make_tuple(5, false, false);
        //     }
        // }
        

        try {
            res = parse_options.main();    // isTerminating is already catched in 2LS and 6 is returned       
        } catch (const std::runtime_error &e) {
            LOG(V0_CRIT, "Error in 2LS %s: exiting with 5\n", e.what());         
        } catch (...) {
            LOG(V0_CRIT, "Unknown error in 2LS: exiting with 5\n");
        }
        
        bool contains_successful = false;
        bool contains_failed = false;

        if (!_params.solutionToFile().empty())
        {
            std::cout.rdbuf(old);
            std::cerr.rdbuf(old_err);
            std::string cbmc_output = buffer.str() + buffer_err.str() + "\n";

            if (!lastLinesContains(10, _params.solutionToFile(), "termination") &&
            !lastLinesContains(10, _params.solutionToFile(), "nontermination"))
            {   
                std::ofstream outputFile(_params.solutionToFile(), std::ios::app);
                outputFile << cbmc_output;
                outputFile.close();
            }
        }

        return std::make_tuple(res, contains_successful, contains_failed);
    }

    JobResult postprocess(int res) {
        std::string jobType = _desc.getAppConfiguration().map["__TA"];
        if (jobType == "false" || jobType == "parent") {
            jobType = "FINAL";
        }

        std::cout << "s " << jobType << " EC=" << res << std::endl;
        std::cout << "t " << jobType << " SAT_TIME: " << CBMCSatConnector::getGlobalSatTime() << std::endl;
        std::cout << "t " << jobType << " SAT_CALLS: " << CBMCSatConnector::getSatCalls() << std::endl;

        if (!_params.solutionToFile().empty())
        {
            std::ofstream outputFile(_params.solutionToFile(), std::ios::app);
            outputFile << "s " + jobType + " EC=" + std::to_string(res) + "\n";
            outputFile << "t " + jobType + " SAT_TIME: " + std::to_string(CBMCSatConnector::getGlobalSatTime()) + "\n";
            outputFile << "t " + jobType + " SAT_CALLS: " + std::to_string(CBMCSatConnector::getSatCalls()) + "\n";
            outputFile.close();
        }

        JobResult r;
        r.id = _desc.getId();
        r.revision = 0;
        if (res == 0)
        {
            r.result = 20;
        }
        else if (res == 10)
        {
            r.result = 10;
        }
        else
        {
            r.result = res;
        }

        return r;
    }

    JobResult solveTerminationAnalysis() {

        std::promise<int> promise;
        std::shared_future<int> result_future(promise.get_future());
        std::atomic<bool> first_finished{false};

        nlohmann::json base_json = {
            {"user", "admin"},
            {"name", "mono-job"},
            {"files", {_filename}},
            {"priority", 1.000},
            {"application", "2LS"},
            {"configuration", {
                {"__TA", ""}
            }}
        };

        nlohmann::json json1 = base_json;
        json1["name"] = "mono-job-termination";
        json1["configuration"]["__TA"] = "termination";
        
        nlohmann::json json2 = base_json;
        json2["name"] = "mono-job-nontermination";
        json2["configuration"]["__TA"] = "nontermination";

        // send 1st sub-job to rank 0
        APIRegistry::sendJobSubmissionToRank(0, json1, [&](JsonInterface::Result res, nlohmann::json& response) mutable {
            assert(res == JsonInterface::Result::ACCEPT);
            LOG(V2_INFO, "Received response for job 1: %s\n", response.dump().c_str());
            int result = response["result"]["solution"][0]["EXITCODE"].get<int>();
            if (!first_finished.exchange(true)) // I am the first to finish 
            {
                if (result != 5) {
                    promise.set_value(result);
                } else {
                    // other one's result will be used
                }
            } 
            else // apparently, other one finished first with result 5 if we are here
            {  
                promise.set_value(result);
            }
        });
        
        // send 2nd sub-job to rank 1
        APIRegistry::sendJobSubmissionToRank(1, json2, [&](JsonInterface::Result res, nlohmann::json& response) {
            assert(res == JsonInterface::Result::ACCEPT);
            LOG(V2_INFO, "Received response for job 2: %s\n", response.dump().c_str());
            int result = response["result"]["solution"][0]["EXITCODE"].get<int>();
            if (!first_finished.exchange(true)) // I am the first to finish 
            {
                if (result != 5) {
                    promise.set_value(result);
                } else {
                    // other one's result will be used
                }
            } 
            else // apparently, other one finished first with result 5 if we are here
            {  
                promise.set_value(result);
            }
        });

        int res = result_future.get();
        
        return postprocess(res);
    }

    bool lastLinesContains(int x, const std::string& filename, const std::string& search) {
        std::ifstream infile(filename);
        std::deque<std::string> last;
        std::string line;
        while (std::getline(infile, line)) {
            last.push_back(line);
            if (last.size() > x)
                last.pop_front();
        }
        for (const auto& l : last) {
            if (l.find(search) != std::string::npos)
                return true;
        }
        return false;
    }

public:
    TwoLSSolver(const Parameters &params, APIConnector &api, JobDescription &desc, const std::string& programFile) : 
    _params(params), _api(api), _desc(desc), _filename(programFile)
    {
        //_filename = params.monoFilename();
        LOG(V2_INFO, "2LS Solver initialized for job #%i with file %s\n", desc.getId(), _filename.c_str());
    }
    ~TwoLSSolver() {
        LOG(V2_INFO, "2LS Solver for job #%i with file %s destroyed\n", _desc.getId(), _filename.c_str());
    }

    JobResult solve()
    {
        LOG(V2_INFO, "Submitting 2LS job with TA: %s\n", _desc.getAppConfiguration().map["__TA"].c_str());

        if (_desc.getAppConfiguration().map["__TA"] == "parent") {
            return solveTerminationAnalysis();
        }

        std::string cbmcOptionsStr = "";
        std::vector<std::string> cbmcOptions;
        cbmcOptions.reserve(20);

        if (_params.twolsOptions().empty())
        {
            LOG(V0_CRIT, "No additional 2LS options provided!\n");
        }
        else
        {
            cbmcOptionsStr = _params.twolsOptions();

            size_t start = 0, end = 0;

            if (cbmcOptionsStr.back() == ',')
            {
                cbmcOptionsStr.pop_back();
            }
            while ((end = cbmcOptionsStr.find(',', start)) != std::string::npos)
            {
                cbmcOptions.push_back(cbmcOptionsStr.substr(start, end - start));
                start = end + 1;
            }
            cbmcOptions.push_back(cbmcOptionsStr.substr(start));
        }

        int res = 5;

        if (_desc.getAppConfiguration().map["__TA"] == "termination") {
            cbmcOptions.push_back("--termination");
        } else if (_desc.getAppConfiguration().map["__TA"] == "nontermination") {
            cbmcOptions.push_back("--nontermination");
        } else {
            assert(_desc.getAppConfiguration().map["__TA"] == "false"); 
        }  

    
        auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions);
        res = res_unwind;

        return postprocess(res);
    }

    
};
#endif
