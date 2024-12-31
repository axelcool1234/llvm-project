#include "llvm/IR/PassManager.h"
using namespace llvm;
struct StrictOpt : public PassInfoMixin<StrictOpt> {
  PreservedAnalyses run(Function &F,
                        FunctionAnalysisManager &FAM);

};
