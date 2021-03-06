/*
  LiveVarHelper.hpp

  A Simple wrapper.

*/

#ifndef __CLPKM__LIVE_VAR_HELPER_HPP__
#define __CLPKM__LIVE_VAR_HELPER_HPP__

#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CFGStmtMap.h"
#include "clang/AST/ASTContext.h"
#include <map>



class LiveVarTracker {
private:
	struct SortBySize {
		bool operator()(clang::VarDecl* LHS, clang::VarDecl* RHS) const {
			auto LTI = LHS->getASTContext().getTypeInfo(LHS->getType());
			auto RTI = RHS->getASTContext().getTypeInfo(RHS->getType());
			if (LTI.Align != RTI.Align)
				return LTI.Align > RTI.Align;
			return LHS < RHS;
			}
		};

	using tracker_type = std::map<clang::VarDecl*, unsigned, SortBySize>;
	using tracker_iter = tracker_type::iterator;

public:
	class iterator {
		iterator(LiveVarTracker* InitLVT, clang::Stmt* InitS,
		         tracker_iter InitIt)
		: LVT(InitLVT), S(InitS), It(InitIt) { }

	public:
		iterator() = delete;
		iterator(const iterator& ) = default;
		iterator& operator=(const iterator& ) = default;

		auto& operator*() { return It->first; }
		auto* operator->() { return &(It->first); }
		iterator operator++(int) { auto Old(*this); ++(*this); return Old; }

		iterator& operator++() {
			while(++It != LVT->Tracker.end() && !LVT->IsLiveAfter(It->first, S));
			return (*this);
			}

		bool operator==(const iterator& RHS) const { return It == RHS.It; }
		bool operator!=(const iterator& RHS) const { return It != RHS.It; }

	private:
		LiveVarTracker* LVT;
		clang::Stmt*    S;
		tracker_iter    It;

		friend class LiveVarTracker;

		};

	// Helper class to generate iterator
	class liveness {
	private:
		liveness(LiveVarTracker* InitLVT, clang::Stmt* InitS)
		: LVT(InitLVT), S(InitS) { }

	public:
		liveness() = delete;
		liveness(const liveness& ) = default;
		liveness& operator=(const liveness& ) = default;

		iterator begin() const {
			tracker_iter It = LVT->Tracker.begin();
			while (It != LVT->Tracker.end() && !LVT->IsLiveAfter(It->first, S))
				++It;
			return iterator(LVT, S, It);
			}

		iterator end() const {
			return iterator(LVT, S, LVT->Tracker.end());
			}

	private:
		LiveVarTracker* LVT;
		clang::Stmt* S;

		friend class LiveVarTracker;

		};

	// Default stuff
	LiveVarTracker()
	: Manager(nullptr), Context(nullptr), Map(nullptr), LiveVar(nullptr),
	  Scope(0) { }

	~LiveVarTracker() { this->EndContext(); }
	LiveVarTracker(const LiveVarTracker& ) = delete;

	// Set context, e.g. FunctionDecl
	bool SetContext(const clang::Decl* );
	void EndContext();
	bool HasContext() const noexcept { return (Map != nullptr); }

	// Variable declarations to track liveness
	bool AddTrack(clang::VarDecl* VD) { return Tracker.emplace(VD, Scope).second; }
	bool RemoveTrack(clang::VarDecl* VD) { return (Tracker.erase(VD) > 0); }
	void ClearTrack() { Tracker.clear(); }

	// We don't need this if Clang's live value analysis works properly...
	// Sometimes Clang considers variables still alive outside of its scope,
	// especially those declared in StmtExprs...
	// Remove them from tracking manually to workaround this problem
	void NewScope() { ++Scope; }
	void PopScope();

	liveness GenLivenessAfter(clang::Stmt* S) { return liveness(this, S); }

private:
	bool IsLiveAfter(clang::VarDecl* , clang::Stmt* ) const;

	clang::AnalysisDeclContextManager* Manager;
	clang::AnalysisDeclContext*        Context;
	clang::CFGStmtMap*                 Map;
	clang::LiveVariables*              LiveVar;

	tracker_type Tracker;
	unsigned Scope;

	};



#endif
