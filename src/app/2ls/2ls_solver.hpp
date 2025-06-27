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
    TwoLSSolver(const Parameters &params, APIConnector &api, JobDescription &desc) : _params(params), _api(api), _desc(desc)
    {
        //_filename = desc.getAppConfiguration().map.at("file");
        _filename = params.monoFilename();
    }
    ~TwoLSSolver() = default;

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

        int res = 42;

        auto [res_unwind, contains_successful, contains_failed] = runCBMC(cbmcOptions);
        res = res_unwind;

        if (!_params.s2f2ls().empty())
        {
            std::ofstream outputFile(_params.s2f2ls(), std::ios::app);
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
        // std::stringstream buffer;
        // std::stringstream buffer_err;
        // std::streambuf *old = std::cout.rdbuf(buffer.rdbuf());
        // std::streambuf *old_err = std::cerr.rdbuf(buffer_err.rdbuf());

        int res = parse_options.main();
        bool contains_successful = false;
        bool contains_failed = false;

        // std::cout.rdbuf(old);
        // std::cerr.rdbuf(old_err);

        // // Write output immediately after run
        // std::string cbmc_output = buffer.str() + buffer_err.str() + "\n" + "CBMCexitcode" 
        //                                        + "(" + argv[argc - 2] + " " + argv[argc - 1] 
        //                                        +  "): " + std::to_string(res) + "\n";

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
