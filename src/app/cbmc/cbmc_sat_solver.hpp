#ifndef MALLOB_CBMC_SAT_SOLVER_HPP
#define MALLOB_CBMC_SAT_SOLVER_HPP

class CBMCSatSolver
{

public:
    virtual ~CBMCSatSolver() = default;

    virtual void setFormula(std::vector<int>&& formula, int num_vars, int num_clauses) = 0;

    virtual void setAssumptions(std::vector<int> assumptions) = 0;

    virtual int solve() = 0;

    virtual std::vector<int> getSolution() = 0;

    virtual bool isTerminating() const = 0;

    virtual std::set<int> getFailedLiterals() = 0;

};


#endif // MALLOB_CBMC_SAT_SOLVER_HPP