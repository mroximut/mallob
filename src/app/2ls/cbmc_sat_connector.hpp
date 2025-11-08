#pragma once

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "app/sat/stream/internal_sat_job_stream_processor.hpp"
#include "app/sat/stream/mallob_sat_job_stream_processor.hpp"
#include "app/sat/stream/sat_job_stream.hpp"
#include "data/job_description.hpp"
#include "interface/api/api_connector.hpp"
#include "robin_set.h"
#include "util/assert.hpp"
#include "util/logger.hpp"
#include "util/sys/terminator.hpp"
#include "util/sys/timer.hpp"
#include "interface/api/api_registry.hpp"
#include "app/cbmc/old_job_stream.hpp"
#include "app/sat/data/model_string_compressor.hpp"

#include "app/cbmc/cbmc_sat_solver.hpp"

class CBMCSatConnector : public CBMCSatSolver
{

private:

    inline static float global_sat_time {0.0f};
    inline static int sat_calls {0};

    int _stream_id;
    std::string _name;

    SatJobStream _job_stream;
    MallobSatJobStreamProcessor* _mallob_processor {nullptr};
    OldJobStream* _old_job_stream {nullptr};
    bool _old_job_stream_used {false};

    std::vector<int> _lits;
    std::vector<int> _assumptions;

    int _nb_vars {0};
    int _nb_clauses {0};
    int _revision {-1};

    std::vector<int> _solution;
    tsl::robin_set<int> _failed_lits;
    bool _terminate {false};

    float _sat_time {0.0f};

public:

    static int getNextStreamId()
    {
        static int _stream_id = 1;
        return _stream_id++;
    }

    CBMCSatConnector(const std::string& name, Parameters& params, JobDescription& desc, bool oldJobStream = false):
        _stream_id(getNextStreamId()),
        _name(name + ":" + std::to_string(_stream_id) + "(SAT)"),
        _job_stream(_name),
        _old_job_stream_used(oldJobStream) {

        APIConnector& api = APIRegistry::get();

        if (!_old_job_stream_used) {
            _mallob_processor = new MallobSatJobStreamProcessor(params, api, desc, _name, _stream_id, true, _job_stream.getSynchronizer());
            _job_stream.addProcessor(_mallob_processor);
            LOG(V2_INFO, "New: %s\n", _name.c_str());

            auto internalProcessor = new InternalSatJobStreamProcessor(true, _job_stream.getSynchronizer());
            _job_stream.addProcessor(internalProcessor);
        } else {
            _old_job_stream = new OldJobStream(api, _stream_id, true);
        }
    }

    void addLiteral(int lit) {
        _lits.push_back(lit);
        _nb_vars = std::max(_nb_vars, std::abs(lit));
        _nb_clauses += lit == 0;
    }
    void assumeLiteral(int lit) {
        _assumptions.push_back(lit);
    }

    void setFormula(std::vector<int>&& lits, int nb_vars, int nb_clauses) {
        _lits = std::move(lits);
        _nb_vars = nb_vars;
        _nb_clauses = nb_clauses;
        LOG(V2_INFO, "%s set formula with %i vars and %i clauses\n", _name.c_str(), _nb_vars, _nb_clauses);
    }
    void setAssumptions(std::vector<int> assumptions) {
        _assumptions = std::move(assumptions);
        LOG(V2_INFO, "%s set %lu assumptions\n", _name.c_str(), _assumptions.size());
    }

    int solve() {
        _revision++;
        sat_calls++;

        if (!_old_job_stream_used) {
            _job_stream.setTerminator([&]() {return isTerminating();});
            if (_revision == 0 && _mallob_processor) {
                _mallob_processor->setInitialSize(_nb_vars, _nb_clauses);
            }
            //dumpCNF("/tmp/beforenewstream.cnf");
        } else {
            if (_revision == 0 && _old_job_stream) {
                _old_job_stream->setNbVarsAndClauses(_nb_vars, _nb_clauses);
            }
        }

        auto time = Timer::elapsedSeconds();
        LOG(V2_INFO, "%s submit rev. %i (%i lits)\n", _name.c_str(), _revision, _lits.size());

        // int max_lit = 0;
        // for (auto &lit : _lits) {
        //     max_lit = std::max(max_lit, std::abs(lit));
        // } 
        // LOG(V0_CRIT, "Max lit in rev. %i is %i, nb_vars is %i\n", _revision, max_lit, _nb_vars);
        // int max_ass = 0;
        // for (auto &lit : _assumptions) {
        //     max_ass = std::max(max_ass, std::abs(lit));
        //}
        //LOG(V0_CRIT, "Max ass in rev. %i is %i, nb_vars is %i\n", _revision, max_ass, _nb_vars);

        auto [resultCode, solution] = _old_job_stream_used ? solveOldJobStream(std::move(_lits), _assumptions) : _job_stream.solve(std::move(_lits), _assumptions);
        // auto copyLits1 = _lits;
        // auto copyLits2 = _lits;
        // auto copyLits3 = _lits;
        // auto copyAss1  = _assumptions;
        // auto copyAss2  = _assumptions;
        // auto [resultCode1, solution1] = solveOldJobStream(std::move(copyLits1), copyAss1);
        // auto [resultCode2, solution2] = _job_stream.solve(std::move(copyLits2), copyAss2);
        // if (resultCode1 != resultCode2) {
        //     LOG(V0_CRIT, "ERROR: %s rev. %i got different results from old and new stream: %i vs %i\n",
        //         _name.c_str(), _revision, resultCode1, resultCode2);
        // }
        // for (auto solution : {&solution1, &solution2}) {
        // if (resultCode1 == 10 && solution->size() > 0) {  // Only verify if SAT
        //     bool satisfies_all = true;
        //     std::vector<int> current_clause;
            
        //     for (int lit : copyLits3) {
        //         if (lit == 0) {  // End of clause
        //             bool clause_satisfied = false;
        //             for (int clause_lit : current_clause) {
        //                 int var = std::abs(clause_lit);
        //                 if (var >= solution->size()) {
        //                     LOG(V0_CRIT, "ERROR: Solution index out of bounds: var %d, solution size %lu\n", 
        //                         var, solution->size());
        //                     continue;
        //                 }
        //                 int val = (*solution)[var];
        //                 if ((clause_lit > 0 && val > 0) || (clause_lit < 0 && val < 0)) {
        //                     clause_satisfied = true;
        //                     break;
        //                 }
        //             }
        //             if (!clause_satisfied) {
        //                 LOG(V0_CRIT, "ERROR: %s rev. %i solution doesn't satisfy a clause\n", 
        //                     _name.c_str(), _revision);
        //                 satisfies_all = false;
        //                 break;
        //             }
        //             current_clause.clear();
        //         } else {
        //             current_clause.push_back(lit);
        //         }
        //     }
            
        //     if (satisfies_all) {
        //         LOG(V2_INFO, "%s rev. %i: solution verified - satisfies all clauses\n", 
        //             _name.c_str(), _revision);
        //     }
        // }
        //}
        // if (solution1.size() != solution2.size()) {
        //     LOG(V0_CRIT, "ERROR: %s rev. %i got different solution sizes from old and new stream: %lu vs %lu\n",
        //         _name.c_str(), _revision, solution1.size(), solution2.size());
        // }
        // for (size_t i = 0; i < solution1.size(); i++) {
        //     if (solution1[i] != solution2[i]) {
        //         LOG(V0_CRIT, "ERROR: %s rev. %i solutions differ at position %lu: %d vs %d\n",
        //             _name.c_str(), _revision, i, solution1[i], solution2[i]);
        //         break;
        //     }
        // }        
        // auto resultCode = resultCode1;
        // auto solution = solution1;
        _lits.clear();
        _assumptions.clear();

        int result = 0;
        time = Timer::elapsedSeconds() - time;
        LOG(V2_INFO, "%s rev. %i done - time=%.3fs res=%i\n", _name.c_str(), _revision, time, resultCode);
        result = resultCode;
        _sat_time += time;
        global_sat_time += time;

        if (result == 10) {
            _solution = std::move(solution);
        }
        if (result == 20) {
            _failed_lits.clear();
            for (int lit : solution) _failed_lits.insert(lit);
        }
        //std::cout << "t " << _stream_id << " " << _revision << " " << result << " " << time << std::endl;
        LOG_OMIT_PREFIX(V0_CRIT, "t %d %d %d %.3f\n", _stream_id, _revision, result, time);
        return result;
    }

    std::pair<int, std::vector<int>> solveOldJobStream(std::vector<int>&& newLits, std::vector<int>& assumptions) {
        int resultcode;
        std::vector<int> solution;
        //dumpCNF("/tmp/beforeoldjob.cnf");
        _old_job_stream->submitNext(std::move(newLits), assumptions, "", 1.0);
        while (_old_job_stream->isPending()) {
            if (isTerminating()) {
                _old_job_stream->interrupt();
            }
            usleep(1000);
        }
        nlohmann::json j = _old_job_stream->getResult();
        resultcode = j["result"]["resultcode"];
        if (resultcode == 10) {
            // SAT

            if (j["result"]["solution"].is_array()) {
                if (j["result"]["solution"].size() > 0 && j["result"]["solution"][0].is_number()) {
                    solution = j["result"]["solution"].get<std::vector<int>>();
                } 
            } else if (j["result"]["solution"].is_string()) {
                std::string compressedModel = j["result"]["solution"].get<std::string>();
                solution = ModelStringCompressor::decompress(compressedModel);
            }

            //j["result"]["solution"] = "[solution data omitted]";
            //LOG(V2_INFO, "Mallob result: %s\n", j.dump().c_str());

        } else if (resultcode == 20) {
            // UNSAT
            if (j["result"]["solution"].is_array()) {
                if (j["result"]["solution"].size() > 0 && j["result"]["solution"][0].is_number()) {
                    solution = j["result"]["solution"].get<std::vector<int>>();
                }
            }
            //LOG(V2_INFO, "Mallob result: %s\n", j.dump().c_str());
        }

        return std::pair<int, std::vector<int>>(resultcode, std::move(solution));
    }

    std::vector<int> getSolution() {
        if (_solution.empty()) {
            return {};
        }
        return std::move(_solution);
    }

    std::set<int> getFailedLiterals() {
        return std::set<int>(_failed_lits.begin(), _failed_lits.end());
    }

    int getValue(int lit) {
        int var = std::abs(lit);
        assert(var < _solution.size() || log_return_false("[ERROR] Solution has size %lu - variable %i queried!\n", _solution.size(), var));
        int val = _solution[var];
        assert(std::abs(val) == var);
        if (val > 0) return 1;
        if (val < 0) return -1;
        return 0;
    }

    bool getFailed(int lit) {
        assert(!_failed_lits.count(-lit));
        return _failed_lits.count(lit);
    }

    ~CBMCSatConnector()
    {
        LOG(V2_INFO, "Done: %s\n", _name.c_str());
        //std::cout << "t SAT_TIME: " << global_sat_time << std::endl;
        LOG_OMIT_PREFIX(V0_CRIT, "t SAT_TIME: %.3f\n", global_sat_time);
        if (_old_job_stream_used) {
            _old_job_stream->interrupt();
            _old_job_stream->finalize();
            delete _old_job_stream;
            _old_job_stream = nullptr;
        } else {
            _job_stream.interrupt();
            _job_stream.finalize();
        }
    }

    void setTerminate() {
        _terminate = true;
        LOG(V2_INFO, "%s set terminate\n", _name.c_str());
    }

    bool isTerminating() const {
        if (Terminator::isTerminating())
            return true;
        return _terminate;
    }

    static float getGlobalSatTime() {
        return global_sat_time;
    }

    static int getSatCalls() {
        return sat_calls;
    }

    void dumpCNF(const std::string& filename) const {
        std::ofstream os(filename, std::ios::app);
        if (!os) return;
        os << "c revision " << _revision << "\n";
        os << "c new clauses " << _lits.size() << "\n";
        os << "c assumptions " << _assumptions.size() << "\n";
        for (size_t i = 0; i < _lits.size(); ++i) {
            int lit = _lits[i];
            os << lit;
            os << (lit == 0 ? "\n" : " ");
        }
        if (!_assumptions.empty()) {
            os << "a";
            for (int a : _assumptions) os << " " << a;
            os << "\n";
        }
        os.close();
    }

};
