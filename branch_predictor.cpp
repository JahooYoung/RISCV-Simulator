#include "branch_predictor.hpp"
using namespace std;

reg_t NeverTaken::predict(reg_t pc, inst_t inst)
{
    return pc;
}

reg_t AlwaysTaken::predict(reg_t pc, inst_t inst)
{

}
