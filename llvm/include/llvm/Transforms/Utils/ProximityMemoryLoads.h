#ifndef LLVM_TRANSFORMS_PROXIMITYMEMORYLOADS_H
#define LLVM_TRANSFORMS_PROXIMITYMEMORYLOADS_H

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class ProximityMemoryLoadsPass
    : public PassInfoMixin<ProximityMemoryLoadsPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  void printProximityMemoryLoads(BasicBlock &BB, const DataLayout &DL,
                                 const uint64_t N);
  bool isProximity(const uint64_t ElemSizeA, Value *PtrA,
                   const uint64_t ElemSizeB, Value *PtrB, const uint64_t N);
  uint64_t getPtrConst(const uint64_t ElemSize, Value *Ptr,
                       const bool SameBaseAddr);
  uint64_t calcPtrWithOffset(const uint64_t ElemSize, GetElementPtrInst *GEP,
                             const bool SameBaseAddr);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_PROXIMITYMEMORYLOADS_H
