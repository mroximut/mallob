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
    std::string _filename;
    Parameters _params;
    APIConnector &_api;
    JobDescription &_desc;

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
        // if (!_params.terminationAnalysis().empty()) {
        //     std::vector<std::string> cbmcOptionsTermination = cbmcOptions;
        //     cbmcOptionsTermination.push_back("--termination");
        //     std::vector<std::string> cbmcOptionsNontermination = cbmcOptions;
        //     cbmcOptionsNontermination.push_back("--nontermination");

        //     std::promise<std::tuple<int, bool, bool>> promise;
        //     std::shared_future<std::tuple<int, bool, bool>> result_future(promise.get_future());
        //     std::atomic<bool> result_set{false};

        //     // Termination task
        //     ProcessWideThreadPool::get().addTask([&, cbmcOptionsTermination]() mutable {
        //         auto result = runCBMC(cbmcOptionsTermination);
        //         int res_term = std::get<0>(result);
        //         if (!result_set.exchange(true) && res_term != 5) {
        //             promise.set_value(result);
        //         } else if (res_term == 5) {
        //             // Wait for the other, but if both are 5, set anyway
        //             if (result_set.exchange(true)) {
        //                 promise.set_value(result);
        //             }
        //         }
        //     });

        //     // Nontermination task
        //     ProcessWideThreadPool::get().addTask([&, cbmcOptionsNontermination]() mutable {
        //         auto result = runCBMC(cbmcOptionsNontermination);
        //         int res_nonterm = std::get<0>(result);
        //         if (!result_set.exchange(true) && res_nonterm != 5) {
        //             promise.set_value(result);
        //         } else if (res_nonterm == 5) {
        //             // Wait for the other, but if both are 5, set anyway
        //             if (result_set.exchange(true)) {
        //                 promise.set_value(result);
        //             }
        //         }
        //     });

        //     // Wait for the first result to be set
        //     auto [res_any, contains_successful, contains_failed] = result_future.get();
        //     res = res_any;

    
        auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions);
        res = res_unwind;
        
        std::cout << "s EC=" << res << std::endl;
        std::cout << "t SAT_TIME: " << CBMCSatConnector::getGlobalSatTime() << std::endl;

        if (!_params.solutionToFile().empty())
        {
            std::ofstream outputFile(_params.solutionToFile(), std::ios::app);
            outputFile << "s EC=" + std::to_string(res) + "\n";
            outputFile << "t SAT_TIME: " + std::to_string(CBMCSatConnector::getGlobalSatTime()) + "\n";
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

        // std::string args;
        // args += _filename;
        // for (const auto& opt : cbmcOptions) {
        //     if (!args.empty()) args += " ";
        //     args += opt;
        // }
        // Subprocess proc(_params, "/home/oguz/Desktop/hiwi_code/cbmc_mallob_monolithic/mallob/lib/2ls/src/2ls/2ls", args); 
        // int pid = proc.start();

        // int status = 0;
        // if (pid > 0) {
        //     if (waitpid(pid, &status, 0) == -1) {
        //         LOG(V0_CRIT, "waitpid failed for 2LS subprocess\n");
        //         status = -1;
        //     }
        // }

        // // To get the exit code:
        // int exit_code = -1;
        // if (WIFEXITED(status)) {
        //     exit_code = WEXITSTATUS(status);
        // }
        // int res = exit_code;

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
            res = parse_options.main();           
        } catch (const std::exception &e) {
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
            std::ofstream outputFile(_params.solutionToFile(), std::ios::app);
            outputFile << cbmc_output;
            outputFile.close();
        }
        // // if (cbmc_output.find("VERIFICATION SUCCESSFUL") != std::string::npos)
        // // {
        // //     contains_successful = true;
        // // }
        // // if (cbmc_output.find("VERIFICATION FAILED") != std::string::npos)
        // // {
        // //     contains_failed = true;
        // // }

        // if (!_params.s2f2ls().empty())
        // {
        //     std::ofstream outputFile(_params.s2f2ls(), std::ios::app);
        //     outputFile << cbmc_output;
        //     //outputFile << "\nEC=" + std::to_string(res) + "\n";
        //     outputFile.close();
        // }
        // else
        // {
        //     LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", cbmc_output.c_str());
        //     LOG(V2_INFO, "End of CBMC output----------------------------------\n");
        // }

        return std::make_tuple(res, contains_successful, contains_failed);
    }
};
#endif
