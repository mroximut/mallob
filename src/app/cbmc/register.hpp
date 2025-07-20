
#pragma once

#include "app/app_message_subscription.hpp"
#include "app/app_registry.hpp"
#include "data/job_description.hpp"
#include "data/job_processing_statistics.hpp"
#include "interface/api/api_connector.hpp"
#include "app/cbmc/cbmc_solver.hpp"
#include "app/cbmc/cbmc_reader.hpp"

struct ClientSideCBMCProgram : public app_registry::ClientSideProgram {
    std::unique_ptr<CBMCSolver> solver;
    ClientSideCBMCProgram(const Parameters& params, APIConnector& api, JobDescription& desc, const std::string& programFile) :
        app_registry::ClientSideProgram(), solver(new CBMCSolver(params, api, desc, programFile)) {
        function = [&]() {return solver->solve();};
    }
    virtual ~ClientSideCBMCProgram() {}
};

void register_mallob_app_cbmc() {
    app_registry::registerClientSideApplication("CBMC",
        // Job reader
        [](const Parameters& params, const std::vector<std::string>& files, JobDescription& desc) {
            return CBMCReader().read(params, files.front(), desc);
        },
        // Client-side program
        [](const Parameters& params, APIConnector& api, JobDescription& desc) {
            std::string programFile = StaticStore<std::string>::extract("cbmc-jobdesc-#" + std::to_string(desc.getId()));
            return new ClientSideCBMCProgram(params, api, desc, programFile);
        },
        // Job solution formatter
        [](const Parameters& params, const JobResult& result, const JobProcessingStatistics& stat) {
            auto json = nlohmann::json::array();

            std::cout << "t FINAL PROCESSING_TIME: " << stat.processingTime << std::endl;

            if (!params.solutionToFile().empty())
            {
                std::ofstream outputFile(params.solutionToFile(), std::ios::app);
                outputFile << "t FINAL PROCESSING_TIME: " + std::to_string(stat.processingTime) + "\n";
                outputFile.close();
            }
            
            json.push_back({
                {"EXITCODE", result.result == 20 ? 0 : result.result},         
                {"result", result.result == 10 ? "VERIFICATION FAILED" : 
                    (result.result == 20 ? "VERIFICATION SUCCESSFUL": "UNKNOWN")},
                {"application", "CBMC"},
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
