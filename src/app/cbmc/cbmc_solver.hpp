#ifndef MALLOB_CBMC_SOLVER_HPP
#define MALLOB_CBMC_SOLVER_HPP

#include "interface/api/api_connector.hpp"
#include "util/params.hpp"
#include "data/job_description.hpp"
#include "data/job_result.hpp"
#include "cbmc/cbmc_parse_options.h"
#include "util/sys/thread_pool.hpp"
#include <iostream>
#include <vector>

#include "app/2ls/cbmc_sat_connector.hpp"

class CBMCSolver
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
        std::cout << cbmc_output;
        
        if (!_params.solutionToFile().empty())
        {
            std::ofstream outputFile(_params.solutionToFile(), std::ios::app);
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

        postprocess(res, false);
        return std::make_tuple(res, contains_successful, contains_failed);
    }

    JobResult postprocess(int res, bool final) {
        // std::string jobType = _desc.getAppConfiguration().map["__TA"];
        // if (jobType == "false" || jobType == "parent") {
        //     jobType = "FINAL";
        // }
        std::string jobType = "";
        if (final)
            jobType = "FINAL";

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

public:
    CBMCSolver(const Parameters &params, APIConnector &api, JobDescription &desc, const std::string& programFile) : 
    _params(params), _api(api), _desc(desc), _filename(programFile)
    {
        LOG(V2_INFO, "CBMC Solver initialized for job #%i with file %s\n", desc.getId(), _filename.c_str());
    }
    ~CBMCSolver() {
        LOG(V2_INFO, "CBMC Solver for job #%i with file %s destroyed\n", _desc.getId(), _filename.c_str());
    }

    JobResult solve()
    {

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
                auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions);
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
                        auto [res_unwind2, contains_successful2, contains_failed2] = runCBMC(cbmcOptions);
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

        return postprocess(res, true);
    }
};
#endif