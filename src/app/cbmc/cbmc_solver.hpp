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

class CBMCSolver
{

private:
    std::string _filename;
    Parameters _params;
    APIConnector &_api;
    JobDescription &_desc;

public:
    CBMCSolver(const Parameters &params, APIConnector &api, JobDescription &desc) : _params(params), _api(api), _desc(desc)
    {
        //_filename = desc.getAppConfiguration().map.at("file");
        _filename = params.monoFilename();
    }
    ~CBMCSolver() = default;

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

        if (!_params.s2f().empty())
        {
            std::ofstream outputFile(_params.s2f(), std::ios::app);
            outputFile << "\nEC=" + std::to_string(res) + "\n";
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

        argv.push_back(strdup("cbmc"));
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
        std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());
        std::streambuf *old_err = std::cerr.rdbuf(buffer_err.rdbuf());

        int res = parse_options.main();
        bool contains_successful = false;
        bool contains_failed = false;

        std::cout.rdbuf(old);
        std::cerr.rdbuf(old_err);

        // Write output immediately after run
        std::string cbmc_output = buffer.str() + buffer_err.str() + "\n" + "CBMCexitcode" 
                                               + "(" + argv[argc - 2] + " " + argv[argc - 1] 
                                               +  "): " + std::to_string(res) + "\n";

        if (cbmc_output.find("VERIFICATION SUCCESSFUL") != std::string::npos)
        {
            contains_successful = true;
        }
        if (cbmc_output.find("VERIFICATION FAILED") != std::string::npos)
        {
            contains_failed = true;
        }

        if (!_params.s2f().empty())
        {
            std::ofstream outputFile(_params.s2f(), std::ios::app);
            outputFile << cbmc_output;
            //outputFile << "\nEC=" + std::to_string(res) + "\n";
            outputFile.close();
        }
        else
        {
            LOG(V2_INFO, "CBMC output----------------------------------\n%s\n", cbmc_output.c_str());
            LOG(V2_INFO, "End of CBMC output----------------------------------\n");
        }

        return std::make_tuple(res, contains_successful, contains_failed);
    }
};
#endif