#include "branch_predictor.hpp"
using namespace std;

reg_t NeverTaken::predict(reg_t pc, inst_t inst)
{
    return pc + ((inst & 3) != 3 ? 2 : 4);
}

reg_t AlwaysTaken::predict(reg_t pc, inst_t inst)
{

}
