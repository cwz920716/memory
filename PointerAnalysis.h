#ifndef CS380C_ASSIGNMENT1_H
#define CS380C_ASSIGNMENT1_H


#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/ValueHandle.h"
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
private:
	using Node = llvm::BasicBlock *;
	using InstSet = std::unordered_set<llvm::Instruction*>;
	using Value = InstSet;
	using ValueSet = std::unordered_map< Node, Value >;
	using BoundMap = std::unordered_map< llvm::Value *, uint64_t >;
	using LocalSummary = std::unordered_map< llvm::Function *, ValueSet >;
	
	// Private field declaration here
	llvm::Module *module;
	LocalSummary local;
	
	void localAnalysis(llvm::Function *);

	void setEnv(llvm::Module& M);
	
public:
	static char ID;
	PointerAnalysis() : llvm::ModulePass(ID) {}

	bool runOnModule(llvm::Module&);
	// We don't modify the program, so we preserve all analyses
	void getAnalysisUsage(llvm::AnalysisUsage &AU) const;
};

}

#endif