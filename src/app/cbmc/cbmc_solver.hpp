#ifndef MALLOB_CBMC_SOLVER_HPP
#define MALLOB_CBMC_SOLVER_HPP

#include "interface/api/api_connector.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include "data/job_result.hpp"
#include "cbmc/cbmc_parse_options.h"
#include "solvers/sat/satcheck_mallob.h"
#include "util/sys/thread_pool.hpp"
#include <iostream>
#include <vector>

#include "app/2ls/cbmc_sat_connector.hpp"
#include "app/cbmc/cbmc_sat_solver.hpp"

class CBMCSolver
{

private:
    Parameters _params;
    APIConnector &_api;
    JobDescription &_desc;
    std::string _filename;
    DTaskTracker _dTaskTracker;

    std::tuple<int, bool, bool> runCBMC(const std::vector<std::string>& cbmcOptions, int unwind = -1, bool print = true)
    {
        //std::cout << "START UNWIND=" << unwind << std::endl;
        int size_options = cbmcOptions.size();
        int argc = 2 + size_options;

        std::vector<const char *> argv;
        argv.reserve(argc + 1); 

        std::string name = "cbmc";
        argv.push_back(name.c_str());
        argv.push_back(_filename.c_str());

        for (const auto &opt : cbmcOptions)
        {
            argv.push_back(opt.c_str());
        }

        argv.push_back(nullptr);

        for (int i = 0; i < argc; i++)
        {
            LOG(V2_INFO, "CBMC argv[%d]: %s\n", i, argv[i]);
        }

        cbmc_parse_optionst parse_options(argc, argv.data());

        // Run CBMC
        std::stringstream buffer;
        std::stringstream buffer_err;
        std::streambuf *old;
        std::streambuf *old_err;

        old = std::cout.rdbuf(buffer.rdbuf());
        old_err = std::cerr.rdbuf(buffer_err.rdbuf());

        int res = 42;

        try {
            res = parse_options.main();    // isTerminating is already catched in 2LS and 6 is returned       
        } catch (const std::runtime_error &e) {
            LOG(V0_CRIT, "Error in CBMC %s: exiting with 42\n", e.what());         
        } catch (...) {
            LOG(V0_CRIT, "Unknown error in CBMC: exiting with 42\n");
        }

        bool contains_successful = false;
        bool contains_failed = false;
        std::string cbmc_output = "";
        
        std::cout.rdbuf(old);
        std::cerr.rdbuf(old_err);
        cbmc_output = buffer.str() + buffer_err.str() + "\n";
        if (_params.parallelUnwind().empty()) {
           LOG(V0_CRIT, "Unwind: %s\n%s", (unwind == -1 ? "" : std::to_string(unwind)).c_str(), cbmc_output.c_str());
        }
        
        if (!_params.cbmcLog().empty())
        {
            std::ofstream outputFile(_params.cbmcLog() + ((_params.parallelUnwind().empty() || unwind == -1) ? 
                                                                "" : "_" + std::to_string(unwind)), std::ios::app);
            outputFile << "Unwind: " << (unwind == -1 ? "" : std::to_string(unwind)) << "\n";
            outputFile << cbmc_output;
            outputFile.close();
        }

        if (cbmc_output.find("VERIFICATION SUCCESSFUL") != std::string::npos)
        {
            contains_successful = true;
        }
        if (cbmc_output.find("VERIFICATION FAILED") != std::string::npos)
        {
            contains_failed = true;
        }
        if (print) {
            postprocess(res, false, unwind);
        }
        
        return std::make_tuple(res, contains_successful, contains_failed);
    }

    JobResult postprocess(int res, bool final, int unwind, int rank = -1) {
        // std::string jobType = _desc.getAppConfiguration().map["__TA"];
        // if (jobType == "false" || jobType == "parent") {
        //     jobType = "FINAL";
        // }
        JobResult r;
        std::string jobType = "";
        if (final) {
            jobType = "FINAL";
            r.setSolution(std::vector<int>({-1}));
        } else {
            jobType = "UNWIND=" + std::to_string(unwind);
        }
        if (rank != -1) {
            jobType += " RANK=" + std::to_string(rank);
        }

        LOG_OMIT_PREFIX(V0_CRIT, "s %s EC=%d\n", jobType.c_str(), res);
        LOG_OMIT_PREFIX(V0_CRIT, "t %s SAT_TIME: %.3f\n", jobType.c_str(), CBMCSatConnector::getGlobalSatTime());
        LOG_OMIT_PREFIX(V0_CRIT, "t %s SAT_CALLS: %d\n", jobType.c_str(), CBMCSatConnector::getSatCalls());

        if (!_params.cbmcLog().empty() && final)
        {
            std::ofstream outputFile(_params.cbmcLog(), std::ios::app);
            outputFile << "s " + jobType + " EC=" + std::to_string(res) + "\n";
            outputFile << "t " + jobType + " SAT_TIME: " + std::to_string(CBMCSatConnector::getGlobalSatTime()) + "\n";
            outputFile << "t " + jobType + " SAT_CALLS: " + std::to_string(CBMCSatConnector::getSatCalls()) + "\n";
            outputFile.close();
        }

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

    int solveForUnwindValue(int unwind_value, std::vector<std::string>& cbmcOptions)
    {
        int ec = 42;
        cbmcOptions.push_back("--unwind");
        cbmcOptions.push_back(std::to_string(unwind_value));
        auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions, unwind_value);
        ec = res_unwind;

        if (ec == 0)
        {
            if (!contains_successful)
            {
                ec = 1;
            }
            else
            {
                cbmcOptions.push_back("--unwinding-assertions");
                auto [res_unwind2, contains_successful2, contains_failed2] = runCBMC(cbmcOptions, unwind_value);
    
                if (res_unwind2 != 0)
                {
                    ec = 42;
                }
            }
        }
        if (ec == 10)
        {
            if (!contains_failed)
            {
                ec = 1;
            }
        }

       return ec;
    }

    // static int getNewUnwind(std::shared_ptr<std::atomic<int>>& currentUnwind) {
    //     int old_val = currentUnwind->load();
    //     if (old_val == 268435456) {
    //         return -1; 
    //     }
    //     int new_val;
    //     do {
    //         double new_vald = old_val * 1.7;
    //         if (new_vald > 3000) {
    //             new_val = 268435456;
    //         } else {
    //             new_val = static_cast<int>(new_vald);
    //         }
    //     } while (!currentUnwind->compare_exchange_weak(old_val, new_val));
    //     return new_val;
    // }

    static int getNewUnwind(std::shared_ptr<std::atomic<int>>& currentIndex) {
        static const std::array<int, 11> unwind_values = {2, 6, 12, 17, 21, 40, 200, 400, 1025, 2049, 268435456};
        int idx = currentIndex->fetch_add(1);
        if (idx >= static_cast<int>(unwind_values.size())) {
            return -1;
        }
        return unwind_values[idx];
    }

    static void sendNextJobToRank(int rank, nlohmann::json json, std::shared_ptr<std::promise<std::tuple<int, int, int>>> promise, 
                              std::shared_ptr<std::atomic<bool>> done, std::shared_ptr<std::atomic<int>> currentUnwind) {
        int unwind = getNewUnwind(currentUnwind);
        if (unwind == -1 || done->load()) {
            return;
        }
        json["name"] = "unwind-" + std::to_string(rank);//std::to_string(unwind);
        json["configuration"]["__UN"] = std::to_string(unwind);
        json["configuration"]["RANK"] = std::to_string(rank);

        APIRegistry::sendJobSubmissionToRank(rank, json, [json, promise, done, currentUnwind, rank, unwind](JsonInterface::Result res, nlohmann::json& response) mutable {
            assert(res == JsonInterface::Result::ACCEPT);
            int result = response["result"]["solution"][0]["EXITCODE"].get<int>();
            if ((unwind == 268435456 || result != 42) && !done->exchange(true)) {
                promise->set_value(std::tuple<int, int, int>(result, unwind, rank));
                return;
            }
            if (result == 42 && !done->load()) {
                sendNextJobToRank(rank, json, promise, done, currentUnwind);
            }
        });
    }
    
    static void sendInterruptToRank(int rank, nlohmann::json json) {
        json["name"] = "unwind-" + std::to_string(rank);
        json["interrupt"] = true;
        APIRegistry::sendJobSubmissionToRank(rank, json, [rank, json](JsonInterface::Result res, nlohmann::json& response) {
            assert(res == JsonInterface::Result::ACCEPT);
            LOG(V0_CRIT, "Interrupt job sent to rank %d\n", rank);
        });
    }
    // static void sendJobsIncrementally(int rank, nlohmann::json json, std::shared_ptr<std::promise<std::pair<int, int>>> promise,
    //                               std::shared_ptr<std::atomic<bool>> done, std::shared_ptr<std::atomic<int>> currentUnwind) {
    //     while (true) {
    //         int unwind = getNewUnwind(currentUnwind);
    //         if (unwind == -1 || done->load()) {
    //             break;
    //         }
    //         nlohmann::json job_json = json;
    //         job_json["name"] = "unwind-" + std::to_string(unwind);
    //         job_json["configuration"]["__UN"] = std::to_string(unwind);
    //         job_json["configuration"]["RANK"] = std::to_string(rank);

    //         std::atomic<bool> job_finished(false);

    //         APIRegistry::sendJobSubmissionToRank(rank, job_json, 
    //             [promise, done, &job_finished, rank, unwind](JsonInterface::Result res, nlohmann::json& response) mutable {
    //                 assert(res == JsonInterface::Result::ACCEPT);
    //                 int result = response["result"]["solution"][0]["EXITCODE"].get<int>();
    //                 if (unwind == 268435456 && !done->exchange(true)) {
    //                     promise->set_value(std::pair<int, int>(result, unwind));
    //                 } else if (result != 42 && !done->exchange(true)) {
    //                     promise->set_value(std::pair<int, int>(result, unwind));
    //                 }
    //                 job_finished = true;
    //             }
    //         );

    //         while (!job_finished.load()) {
    //             std::this_thread::sleep_for(std::chrono::milliseconds(10));
    //         }

    //         if (done->load()) {
    //             break;
    //         }
    //     }
    // }

    JobResult solveParallel()
    {
        auto promise = std::make_shared<std::promise<std::tuple<int, int, int>>>();
        std::shared_future<std::tuple<int, int, int>> result_future(promise->get_future());
        auto done = std::make_shared<std::atomic<bool>>(false);

        std::string parallelWorkers = _params.parallelUnwind();
        int numWorkers = std::stoi(parallelWorkers);

        nlohmann::json base_json = {
            {"user", "admin"},
            {"name", "unwind"},
            {"files", {_filename}},
            {"priority", 1.000},
            {"application", "CBMC"},
            {"configuration", {
                {"__UN", ""},
                {"RANK", ""}
                }
            }
        };

        //auto currentUnwind = std::make_shared<std::atomic<int>>(2);
        auto currentUnwind = std::make_shared<std::atomic<int>>(0);

        for (int i = 0; i < numWorkers; i++) {
            sendNextJobToRank(i, base_json, promise, done, currentUnwind);
            //ProcessWideThreadPool::get().addTask([=]() {
            //    sendJobsIncrementally(i, base_json, promise, done, currentUnwind);
            //});  
        }
        int res = std::get<0>(result_future.get());
        int unwind = std::get<1>(result_future.get());
        int rank = std::get<2>(result_future.get());
        LOG(V0_CRIT, "Parallel unwind finished with result %d and unwind value %d\n", res, unwind);

        nlohmann::json interrupt_json = {
            {"user", "admin"},
            {"name", "unwind"},
            {"files", {_filename}},
            {"priority", 1.000},
            {"application", "CBMC"},
            {"interrupt", true}
        };

        //for (int i = 0; i < numWorkers; i++) {
            //if (i == rank) {
            //    continue;
            //}
            //sendInterruptToRank(i, interrupt_json);
        //}
        
        if (!_params.cbmcLog().empty())
        {
            std::ifstream inputFile(_params.cbmcLog() + "_" + std::to_string(unwind));
            //std::cout << "----------------CBMC OUTPUT---------------------" << std::endl;
            //std::cout << inputFile.rdbuf();
            //std::cout << "-------------------------------------------------" << std::endl;

            std::ofstream outputFile(_params.cbmcLog(), std::ios::app);
            outputFile << inputFile.rdbuf();
            outputFile.close();
            inputFile.close();
        }
        auto result = postprocess(res, true, unwind, -1);
        return result; 
    }

public:
    CBMCSolver(const Parameters &params, APIConnector &api, JobDescription &desc, const std::string& programFile) : 
    _params(params), _api(api), _desc(desc), _filename(programFile), _dTaskTracker(params)
    {
        LOG(V2_INFO, "CBMC Solver initialized for job #%i with file %s\n", desc.getId(), _filename.c_str());
        satcheck_mallobt::createCBMCSatSolver = [this]() {
            return new CBMCSatConnector("Mallob SAT Solver", _params, _desc, _dTaskTracker);
        };
    }
    ~CBMCSolver() {
        LOG(V2_INFO, "CBMC Solver for job #%i with file %s destroyed\n", _desc.getId(), _filename.c_str());
    }

    JobResult solve()
    {
        LOG(V2_INFO, "Submitting CBMC job with UN: %s\n", _desc.getAppConfiguration().map["__UN"].c_str());

        if (_desc.getAppConfiguration().map["__UN"] == "parent") {
            return solveParallel();
        }

        std::string cbmcOptionsStr = "";
        std::vector<std::string> cbmcOptions;
        cbmcOptions.reserve(20);

        if (_params.cbmcOptions().empty())
        {
            LOG(V0_CRIT, "No additional CBMC options provided!\n");
        }
        else
        {
            cbmcOptionsStr = _params.cbmcOptions();

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

        if (_desc.getAppConfiguration().map["__UN"] != "false") 
        {
            int unwind = std::stoi(_desc.getAppConfiguration().map["__UN"]);
            int rank = std::stoi(_desc.getAppConfiguration().map["RANK"]);
            LOG(V0_CRIT, "START UNWIND=%d RANK=%d\n", unwind, rank);
            int ec = solveForUnwindValue(unwind, cbmcOptions);
            LOG(V2_INFO, "CBMC result for unwind value %d\n", unwind);
            return postprocess(ec, false, unwind, rank);
        }    
        else 
        {
            int res = 42;
            if (!_params.unwindLoops().empty())
            {
                int EC = 42;
                int ec = 42;
                std::vector<int> unwind_values = {2, 6, 12, 17, 21, 40, 200, 400, 1025, 2049, 268435456};
                cbmcOptions.push_back("--unwind");

                for (int unwind_value : unwind_values)
                {
                    cbmcOptions.push_back(std::to_string(unwind_value));
                    auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions, unwind_value);
                    ec = res_unwind;

                    if (ec == 0)
                    {
                        if (!contains_successful)
                        {
                            ec = 1;
                        }
                        else
                        {
                            cbmcOptions.push_back("--unwinding-assertions");
                            auto [res_unwind2, contains_successful2, contains_failed2] = runCBMC(cbmcOptions, unwind_value);
                            cbmcOptions.pop_back();
                            if (res_unwind2 != 0)
                            {
                                ec = 42;
                            }
                        }
                    }
                    if (ec == 10)
                    {
                        if (!contains_failed)
                        {
                            ec = 1;
                        }
                    }

                    cbmcOptions.pop_back();
                    if (ec == 42)
                    {
                        continue;
                    }
                    else
                    {
                        EC = ec;
                        break;
                    }
                }
                res = EC;
            }
            else
            {
                auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions);
                res = res_unwind;
            }

            return postprocess(res, true, -1);
        }
    }
};
#endif