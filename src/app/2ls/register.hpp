
#pragma once

#include "app/app_message_subscription.hpp"
#include "app/app_registry.hpp"
#include "data/job_description.hpp"
#include "data/job_processing_statistics.hpp"
#include "interface/api/api_connector.hpp"
#include "app/2ls/2ls_solver.hpp"
#include "app/2ls/2ls_reader.hpp"

struct ClientSide2lsProgram : public app_registry::ClientSideProgram {
    std::unique_ptr<TwoLSSolver> solver;
    ClientSide2lsProgram(const Parameters& params, APIConnector& api, JobDescription& desc) :
        app_registry::ClientSideProgram(), solver(new TwoLSSolver(params, api, desc)) {
        function = [&]() {return solver->solve();};
    }
    virtual ~ClientSide2lsProgram() {}
};

void register_mallob_app_2ls() {
    app_registry::registerClientSideApplication("2LS",
        // Job reader
        [](const Parameters& params, const std::vector<std::string>& files, JobDescription& desc) {
            return TwoLSReader().read(files.front(), desc);
        },
        // Client-side program
        [](const Parameters& params, APIConnector& api, JobDescription& desc) {
            return new ClientSide2lsProgram(params, api, desc);
        },
        // Job solution formatter
        [](const Parameters& params, const JobResult& result, const JobProcessingStatistics& stat) {
            auto json = nlohmann::json::array();

            // if (!params.s2f().empty())
            // {
            //     std::ofstream outputFile(params.s2f(), std::ios::app);
            //     outputFile << "\nEC=" + std::to_string(result.result == 20 ? 0 : result.result) + "\n";
            //     outputFile.close();
            // }
            
            json.push_back({
                {"EXITCODE", result.result == 20 ? 0 : result.result},         
                {"result", result.result == 10 ? "VERIFICATION FAILED" : 
                    (result.result == 20 ? "VERIFICATION SUCCESSFULL": "UNKNOWN")},
                {"application", "2ls"},
                {"stats", {
                    {"timeOfSubmission", stat.timeOfSubmission},
                    {"timeOfScheduling", stat.timeOfScheduling},
                    {"parseTime", stat.parseTime},
                    {"schedulingTime", stat.schedulingTime},
                    {"processingTime", stat.processingTime},
                    {"totalResponseTime", stat.totalResponseTime},
                    {"usedWallclockSeconds", stat.usedWallclockSeconds},
                    {"usedCpuSeconds", stat.usedCpuSeconds},
                    {"latencyOf1stVolumeUpdate", stat.latencyOf1stVolumeUpdate}
                }},
            });
            LOG(V2_INFO, "Job result: %s\n", json.dump().c_str());
            return json;
        }
    );
}
