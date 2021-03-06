#ifndef CS380C_ASSIGNMENT1_H
#define CS380C_ASSIGNMENT1_H


#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/DataLayout.h"

#include <unordered_set>
#include <unordered_map>

namespace cs380c
{

using namespace llvm;
using namespace std;

template < typename T >
class WorkList {
private:
	std::unordered_set<T> s;
	
public:
	WorkList() = default;

	bool enqueue(T elem) {
		if (has(elem))
			return false;

		s.insert(elem);
		return true;
	}

	T dequeue() {
		if (empty()) {
			errs() << "deque a empty worklist!\n";
			exit(-1);
		}

		auto it = s.begin();
		T elem = *it;
		s.erase(it);
		return elem;
	}

	bool has(T elem) {
		auto got = s.find(elem);
		return got != s.end();
	}

	bool empty() {
		return s.empty();
	}

};

class PointerAnalysis: public llvm::ModulePass {
public:
	using Node = llvm::BasicBlock *;
	using InstSet = std::unordered_set<llvm::Instruction*>;
	using ValueSet = std::unordered_set<llvm::Value*>;
	using NodeSet = std::unordered_set<Node>;
	using ValueMap = std::unordered_map< llvm::Value *, ValueSet >; 
	using BoundMap = std::unordered_map< llvm::Value *, uint64_t >;

	using ArgumentAttribute = uint64_t;
	using ArgumentAttributes = std::vector<uint64_t>;

	using FunctSet = std::unordered_set<llvm::Function*>;
	using CallSet = std::unordered_set<llvm::CallInst*>;
	using CallMap = std::unordered_map< llvm::CallInst *, ArgumentAttributes>;
	using ReturnMap = std::unordered_map< llvm::CallInst *, ArgumentAttributes>;
	using ContextFreeSummary = std::unordered_map< llvm::Function *, InstSet >;
	using LocalSummary = std::unordered_map< llvm::Function *, ValueSet >;

	static const ArgumentAttribute DYNBOUND_ATTR = 0;
	static const ArgumentAttribute UNBOUND_ATTR = 0xffffffffffffffffUL;
	
private:
	// Private field declaration here
	llvm::Module *module;
	ContextFreeSummary contextFree;

	// FSCS analysis, cannot solve point-to facts
	ValueSet globals; // all globals are exact pointer
	BoundMap globalBounds;

	FunctSet functs;
	CallSet callsites;
	CallMap argsCall;
	ReturnMap retCall;
	LocalSummary local;

	ValueSet EmptySet;

	void setEnv(llvm::Module& M);
	bool isaPointer(llvm::Instruction *inst);
	void contextFreeAnalysis(llvm::Function *funct);
	bool testAndInsert(llvm::Instruction *inst, InstSet &set);
	bool test(llvm::Value *v, InstSet &set);
	uint64_t getAllocaArraySize(llvm::AllocaInst *alloca_inst);
	uint64_t getConstantAllocSize(llvm::Instruction *inst);
	bool isAllocation(llvm::Instruction *inst);
	void printX(InstSet &set);

	bool test(Value *v, ValueSet &set);
	ValueSet merge(ValueSet a, ValueSet b);
	ValueSet getOrInsert(const Node &n, ValueMap &s);
	ArgumentAttributes objectPass(Function *funct, ArgumentAttributes argAttrs);
	void objectAnalysis();
	void printX(ValueSet &set);
	void printX(ArgumentAttributes &set);
	bool unEqual (const ValueSet &a, const ValueSet &b);
	bool unEqual (const ArgumentAttributes &a, const ArgumentAttributes &b);
	bool isExternalLibrary(Instruction *inst);
	PointerAnalysis::ValueSet join(PointerAnalysis::ValueSet a, PointerAnalysis::ValueSet b);
	void joinWith(PointerAnalysis::ValueSet a, llvm::Function *funct);	

	void annotateValue(Instruction* inst, Function *fp);

public:
	static char ID;
	PointerAnalysis() : llvm::ModulePass(ID) {}

	bool runOnModule(llvm::Module&);
	void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
};

}

#endif
