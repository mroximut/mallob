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

class CBMCSatConnector
{

private:

    inline static float global_sat_time {0.0f};
    inline static int sat_calls {0};

    int _stream_id;
    std::string _name;

    SatJobStream _job_stream;
    MallobSatJobStreamProcessor* _mallob_processor {nullptr};

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

    CBMCSatConnector(const std::string& name) :
        _stream_id(getNextStreamId()),
        _name(name + ":" + std::to_string(_stream_id) + "(SAT)"),
        _job_stream(_name)
        {
        Parameters params;
        APIConnector* api = APIRegistry::get();
        JobDescription desc;

        _mallob_processor = new MallobSatJobStreamProcessor(params, *api, desc,
            _name, _stream_id, true, _job_stream.getSynchronizer());
        _job_stream.addProcessor(_mallob_processor);
        LOG(V2_INFO, "New: %s\n", _name.c_str());

        auto internalProcessor = new InternalSatJobStreamProcessor(true, _job_stream.getSynchronizer());
        _job_stream.addProcessor(internalProcessor);
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
    void setAssumptions(std::vector<int>&& assumptions) {
        _assumptions = std::move(assumptions);
        LOG(V2_INFO, "%s set %lu assumptions\n", _name.c_str(), _assumptions.size());
    }

    int solve() {
        _job_stream.setTerminator([&]() {return isTerminating();});

        _revision++;
        sat_calls++;
        if (_revision == 0 && _mallob_processor) {
            _mallob_processor->setInitialSize(_nb_vars, _nb_clauses);
        }
        auto time = Timer::elapsedSeconds();
        LOG(V2_INFO, "%s submit rev. %i (%i lits)\n", _name.c_str(), _revision, _lits.size());

        auto [resultCode, solution] = _job_stream.solve(std::move(_lits), _assumptions);
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
        std::cout << "t " << _stream_id << " " << _revision << " " << result << " " << time << std::endl;

        return result;
    }

    std::vector<int> getSolution() {
        if (_solution.empty()) {
            return {};
        }
        return std::move(_solution);
    }

    std::set<int> getFailedLiterals() {
        std::set<int> failed;
        for (int lit : _failed_lits) {
            if (lit > 0) failed.insert(lit);
            else failed.insert(-lit);
        }
        return failed;
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
        std::cout << "t SAT_TIME: " << global_sat_time << std::endl;
        _job_stream.interrupt();
        _job_stream.finalize();
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

};