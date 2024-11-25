#include "llvm/Transforms/Utils/ProximityMemoryLoads.h"
#include "llvm/Analysis/LoopAccessAnalysis.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

static llvm::cl::opt<int> MaxProximity(
    "max-proximity", 
    llvm::cl::desc("Set max proximity between loads."), 
    llvm::cl::init(-1)
); 

PreservedAnalyses ProximityMemoryLoadsPass::run(Function &F,
                                                FunctionAnalysisManager &AM) { 
    const DataLayout &DL = F.getDataLayout();
    const TargetTransformInfo &TTI = AM.getResult<TargetIRAnalysis>(F);
    const uint64_t N = (MaxProximity < 0) ? TTI.getCacheLineSize() : MaxProximity;
    errs() << "N: " << N << "\n";
    for (BasicBlock &BB : F) {
        printProximityMemoryLoads(BB, DL, N);
    }
    return PreservedAnalyses::all();
}

void ProximityMemoryLoadsPass::printProximityMemoryLoads(BasicBlock &BB, 
                                                         const DataLayout &DL, 
                                                         const uint64_t N) {
    std::vector<std::pair<LoadInst*, Value*>> Loads;
    for (Instruction &I : BB) {
        if(LoadInst *LoadI = dyn_cast<LoadInst>(&I)) {
            Value* Ptr = LoadI->getPointerOperand();
            if (Ptr->getType()->isPointerTy()) {
                Loads.push_back({LoadI, Ptr});
            }
        }
    }
    std::sort(Loads.begin(), Loads.end(), [](const auto& Lhs, const auto& Rhs) {
        return Lhs.second < Rhs.second;
    });
    for (auto OuterIt = Loads.begin(); OuterIt != Loads.end(); ++OuterIt) {
        LoadInst *OuterLoad = OuterIt->first;
        Value *OuterPtr = OuterIt->second;
        uint64_t OuterSize = DL.getTypeStoreSize(OuterLoad->getType());


        for (auto InnerIt = std::next(OuterIt); InnerIt != Loads.end(); ++InnerIt) {
            LoadInst *InnerLoad = InnerIt->first;
            Value* InnerPtr = InnerIt->second;
            uint64_t InnerSize = DL.getTypeStoreSize(InnerLoad->getType());

            if (isProximity(OuterSize, OuterPtr, InnerSize, InnerPtr, N)) {
                errs() << "Pair of memory loads are within proximity: \n" 
                       << *OuterLoad << "\n and \n" << *InnerLoad << ".\n";
            }
            /*getPointersDiff*/
        }
    }
}

bool ProximityMemoryLoadsPass::isProximity(const uint64_t ElemSizeA, Value *PtrA, 
                                           const uint64_t ElemSizeB, Value *PtrB, 
                                           const uint64_t N) {
    unsigned ASA = PtrA->getType()->getPointerAddressSpace();
    unsigned ASB = PtrB->getType()->getPointerAddressSpace();

    if (ASA != ASB) return false;

    bool SameBaseAddr = PtrA->getValueID() == PtrB->getValueID();

    errs() << "Same Base Address: " << SameBaseAddr << "\n";

    uint64_t StartAddrA = getPtrConst(ElemSizeA, PtrA, SameBaseAddr);
    uint64_t StartAddrB = getPtrConst(ElemSizeB, PtrB, SameBaseAddr);    

    errs() << "StartA: "<< StartAddrA << "\nStartB: " << StartAddrB <<"\n";

    if (StartAddrA == 0 || StartAddrB == 0) return false;

    uint64_t EndAddrA = StartAddrA + ElemSizeA;
    uint64_t EndAddrB = StartAddrB + ElemSizeB;

    errs() << "EndsA: " << EndAddrA << "\nEndsB: " << EndAddrB <<"\n";

    uint64_t OffsetAtoB = (StartAddrB > EndAddrA) ? StartAddrB - EndAddrA : 0;
    uint64_t OffsetBtoA = (StartAddrA > EndAddrB) ? StartAddrA - EndAddrB : 0;
    uint64_t MinOffset = std::max(OffsetAtoB, OffsetBtoA);

    errs() << "Min Offset: " << MinOffset << "\n";
    return MinOffset <= N;
}

uint64_t ProximityMemoryLoadsPass::getPtrConst(const uint64_t ElemSize, Value *Ptr, const bool SameBaseAddr) {
    if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
        errs() << "GEP Detected!\n";
        return calcPtrWithOffset(ElemSize, GEP, SameBaseAddr);
    } 

    if (auto *Const = dyn_cast<ConstantInt>(Ptr)) {
        errs() << "Const Detected!\n";
        return Const->getZExtValue();
    }
    if (auto *ConstExpr = dyn_cast<ConstantExpr>(Ptr)) {
        if (ConstExpr->getOpcode() == Instruction::IntToPtr) {
            errs() << "IntToPtr ConstantExpr Detected!\n"; 
            if (auto *Const = dyn_cast<ConstantInt>(ConstExpr->getOperand(0))) {
                errs() << "Const from IntToPtr Detected!\n"; 
                return Const->getZExtValue();
            }
        }
    }
    errs() << "None Detected! Type: "; 
    Ptr->getType()->dump();
    Ptr->dump();
    errs() << "\n";

    return 0; 
}

uint64_t ProximityMemoryLoadsPass::calcPtrWithOffset(const uint64_t ElemSize, GetElementPtrInst *GEP, const bool SameBaseAddr) {
    uint64_t BaseAddr = 0;
    if (auto *BasePtr = dyn_cast<ConstantInt>(GEP->getPointerOperand())) {
        BaseAddr = BasePtr->getZExtValue(); 
    } else if (!SameBaseAddr) return 0;

    uint64_t Offset = 0;
    for (unsigned OpIndex = 1; OpIndex < GEP->getNumOperands(); ++OpIndex) {
        Value *Index = GEP->getOperand(OpIndex);
        if (auto *ConstIndex = dyn_cast<ConstantInt>(Index)) {
            Offset += ConstIndex->getZExtValue() * ElemSize; 
        } else {
            return 0;
        }
    }
    return BaseAddr + Offset;
}
