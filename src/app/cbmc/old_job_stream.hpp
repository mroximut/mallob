
#pragma once

#include "data/checksum.hpp"
#include "data/job_description.hpp"
#include "interface/api/api_connector.hpp"
#include "interface/json_interface.hpp"
#include "util/json.hpp"
#include "util/logger.hpp"
#include "util/static_store.hpp"
#include "util/params.hpp"

class OldJobStream {

private:
    //const Parameters& _params;
    APIConnector& _api;
    //JobDescription& _desc;

    bool _incremental {true};
    const std::string _username;
    std::string _base_job_name;
    nlohmann::json _json_base;
    int _subjob_counter {0};
    bool _pending {false};
    bool _interrupt_set {false};
    nlohmann::json _json_result;
    bool _rejected {false};
    std::string _expected_result_job_name;

    int _nb_vars {0};
    int _nb_clauses {0};

public:
    OldJobStream(APIConnector& api, int streamId, bool incremental) :
        _api(api),_incremental(incremental),
        _username("cbmc#") {

        _base_job_name = "satjob-" + std::to_string(streamId) + "-rev-";
        _json_base = nlohmann::json {
            {"user", _username},
            {"incremental", incremental},
            {"priority", 1},
            {"application", "SAT"}
        };
        _json_base["files"] = std::vector<std::string>();
    }

    void setGroupId(const std::string& groupId) {
        LOG(V2_INFO, "CBMC %s group ID %s V=[%i,%i]\n", _base_job_name.c_str(), groupId.c_str());
        _json_base["group-id"] = groupId;
    }

    void submitNext(std::vector<int>&& newLiterals, const std::vector<int>& assumptions,
            const std::string& descriptionLabel = "", float priority = 1.0) {
        assert(!_pending);
        assert(newLiterals.empty() || newLiterals.front() != 0);
        assert(newLiterals.empty() || newLiterals.back() == 0);
        
        // std::string cnfFilename = "/home/oguz/Desktop/ipasir/" + _base_job_name + std::to_string(_subjob_counter) + ".cnf";
        // std::ofstream cnfFile(cnfFilename);
        // for (size_t i = 0; i < newLiterals.size(); i++) {
        //     if (newLiterals[i] == 0) {
        //         cnfFile << "0\n";
        //     } else {
        //         cnfFile << newLiterals[i] << " ";
        //     }
        // }        
        // if (!assumptions.empty()) {
        //     cnfFile << "a ";
        //     for (int assumption : assumptions) {
        //         cnfFile << assumption << " ";
        //     }
        //     cnfFile << "0\n";
        // }
        
        // cnfFile.close();

        //for (auto key : {"__NV", "__NC", "__NO"})
        //    _json_base["configuration"][key] = _desc.getAppConfiguration().map.at(key);
        //if (_params.useChecksums()) _json_base["checksum"] = {chksum.count(), chksum.get()};
        _json_base["configuration"]["__XL"] = "-1";
        _json_base["configuration"]["__XU"] = "-1";
        _json_base["configuration"]["__NV"] = std::to_string(_nb_vars);
        _json_base["configuration"]["__NC"] = std::to_string(_nb_clauses);
        std::ofstream os("/tmp/oldjob.cnf", std::ios::app);
        if (!os) return;
        os << "c revision " << _subjob_counter << "\n";
        os << "c new clauses " << newLiterals.size() << "\n";
        os << "c assumptions " << assumptions.size() << "\n";
        for (size_t i = 0; i < newLiterals.size(); ++i) {
            int lit = newLiterals[i];
            os << lit;
            os << (lit == 0 ? "\n" : " ");
        }
        if (!assumptions.empty()) {
            os << "a";
            for (int a : assumptions) os << " " << a;
            os << "\n";
        }
        os.close();

        if (_incremental && _json_base.contains("name")) {
            _json_base["precursor"] = _username + std::string(".") + _json_base["name"].get<std::string>();
        }
        _json_base["priority"] = priority > 0 ? priority : 1;
        const int subjob = _subjob_counter++;
        _json_base["name"] = _base_job_name + std::to_string(subjob);

        nlohmann::json copy(_json_base);
        newLiterals.push_back(INT32_MAX);
        for (int a : assumptions) newLiterals.push_back(a);
        newLiterals.push_back(0);
        StaticStore<std::vector<int>>::insert(_json_base["name"].get<std::string>(), std::move(newLiterals));
        copy["internalliterals"] = _json_base["name"].get<std::string>();
        //copy["literals"] = std::move(newLiterals);
        //copy["assumptions"] = assumptions;
        if (!descriptionLabel.empty()) {
            copy["description-id"] = descriptionLabel;
        }
        _expected_result_job_name = copy["name"].get<std::string>();
        _pending = true;
        _rejected = false;
        _interrupt_set = false;

        LOG(V2_INFO, "Submitting job: %s\n", copy.dump().c_str());
        auto response = _api.submit(copy, [&](nlohmann::json& result) {
            if (result["name"].get<std::string>() != _expected_result_job_name) {
                LOG(V0_CRIT, "[ERROR] CBMC Result for unexpected job \"%s\" (expected: %s)!\n",
                    result["name"].get<std::string>().c_str(), _expected_result_job_name.c_str());
                abort();
            }
            _json_result = std::move(result);
            _pending = false;
        });
        if (response == JsonInterface::Result::DISCARD) {
            _rejected = true;
            _pending = false;
        }
    }
    bool interrupt() {
        if (!_pending || _interrupt_set) return false;
        _interrupt_set = true;
        nlohmann::json jsonInterrupt {
            {"name", _json_base["name"]},
            {"user", _json_base["user"]},
            {"application", _json_base["application"]},
            {"incremental", _json_base["incremental"]},
            {"interrupt", true}
        };
        // In this particular case, the callback is never called.
        // Instead, the callback of the job's original submission is called.
        auto response = _api.submit(jsonInterrupt, [&](nlohmann::json& result) {assert(false);});
        if (response == JsonInterface::Result::DISCARD) {
            _rejected = true;
            _pending = false;
        }
        return true;
    }
    void finalize() {
        if (_pending) return; // if a job is still running, then we must be terminating alltogether.
        if (!_incremental) return;
        if (!_json_base.contains("name")) return;
        _json_base["precursor"] = _username + std::string(".") + _json_base["name"].get<std::string>();
        _json_base["name"] = _base_job_name + std::to_string(_subjob_counter++);
        nlohmann::json copy(_json_base);
        copy["done"] = true;
        // The callback is never called.
        _api.submit(copy, [&](nlohmann::json& result) {assert(false);});
    }

    bool isPending() {
        if (_pending && !_api.active()) {
            _rejected = true;
            _pending = false;
        }
        return _pending;
    }
    bool isRejected() const {
        return _rejected;
    }
    nlohmann::json& getResult() {
        assert(!_pending);
        return _json_result;
    }
    void setNbVarsAndClauses(int nbVars, int nbClauses) {
        _nb_vars = nbVars;
        _nb_clauses = nbClauses;
    }

};
