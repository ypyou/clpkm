//===----------------------------------------------------------------------===//
// CLPKMPP
//
// Modified from Eli's tooling example
//===----------------------------------------------------------------------===//
#include <algorithm>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "clpkmpp"

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory CLPKMPPCategory("CLPKMPP");



class Extractor : public RecursiveASTVisitor<Extractor> {
public:
	Extractor(CompilerInstance& CI, Rewriter& R)
	: TheCI(CI), TheRewriter(R) { }

	bool TraverseDecl(clang::Decl* D) {

		bool Ret = true;

		// Don't traverse headers
		if (D != nullptr && TheCI.getSourceManager().isInMainFile(D->getLocation()))
			Ret = clang::RecursiveASTVisitor<Extractor>::TraverseDecl(D);

		return Ret;

		}

	bool VisitReturnStmt(ReturnStmt* RS) {

		if (RS == nullptr || RS->getRetValue() == nullptr)
			return true;

		// Workaround a mystery behaviour of Clang. The source range of the
		// statement below stops right before "b;" under some circumstances
		//   return a = b;
		TheRewriter.InsertTextBefore(
				ExpandStartLoc(RS->getRetValue()->getLocStart()), " ( ");
		TheRewriter.InsertTextAfterToken(
				ExpandEndLoc(RS->getRetValue()->getLocEnd()), " ) ");

		return true;

		}

	// The semicolor is not a part of a Expr. Append brace for CC.
	bool VisitIfStmt(IfStmt* IS) {

		if (IS == nullptr)
			return true;

		return AppendBrace(IS->getThen()) && AppendBrace(IS->getElse());

		}

	bool VisitWhileStmt(WhileStmt* WS) {

		if (WS == nullptr)
			return true;

		return AppendBrace(WS->getBody());

		}

	bool VisitForStmt(ForStmt* FS) {

		if (FS == nullptr)
			return true;

		return AppendBrace(FS->getBody());

		}

private:
	CompilerInstance& TheCI;
	Rewriter&         TheRewriter;

	SourceLocation ExpandStartLoc(SourceLocation StartLoc) {

		auto& SM = TheRewriter.getSourceMgr();

		if(StartLoc.isMacroID()) {
			auto ExpansionRange = SM.getImmediateExpansionRange(StartLoc);
			StartLoc = ExpansionRange.first;
			}

		return StartLoc;

		}

	SourceLocation ExpandEndLoc(SourceLocation EndLoc) {

		auto& SM = TheRewriter.getSourceMgr();

		if(EndLoc.isMacroID()) {
			auto ExpansionRange = SM.getImmediateExpansionRange(EndLoc);
			EndLoc = ExpansionRange.second;
			}

		return EndLoc;

		}

	SourceRange ExpandRange(SourceRange Range) {
		Range.setBegin(ExpandStartLoc(Range.getBegin()));
		Range.setEnd(ExpandEndLoc(Range.getEnd()));
		return Range;
		}

	bool AppendBrace(Stmt* S) {

		if (S == nullptr || isa<CompoundStmt>(S))
			return true;

		if (!S->getStmtLocEnd().isValid())
			llvm_unreachable("getStmtLocEnd returned invalid location :(");

		TheRewriter.InsertTextBefore(ExpandStartLoc(S->getLocStart()), " { ");
		TheRewriter.InsertTextAfterToken(ExpandEndLoc(S->getStmtLocEnd()), " } ");

		return true;

		}

	};



class ExtractorDriver : public ASTConsumer {
public:
	template <class ... P>
	ExtractorDriver(P&& ... Param) :
		Visitor(std::forward<P>(Param)...) {

		}

	bool HandleTopLevelDecl(DeclGroupRef DeclGroup) override {

		for (auto& Decl : DeclGroup)
			Visitor.TraverseDecl(Decl);

		return true;

		}

private:
	Extractor Visitor;

	};



class CLPKMPPFrontendAction : public ASTFrontendAction {
public:
	CLPKMPPFrontendAction() = default;

	void EndSourceFileAction() override {

		if (getCompilerInstance().getDiagnostics().hasErrorOccurred())
			return;

		SourceManager &SM = PPRewriter.getSourceMgr();
		PPRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());

		}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
	                                               StringRef file) override {

		PPRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return llvm::make_unique<ExtractorDriver>(CI, PPRewriter);

		}

	 bool BeginInvocation(CompilerInstance &CI) override {

		CI.getDiagnostics().setIgnoreAllWarnings(true);
		return true;
		
		}

private:
	Rewriter PPRewriter;

	};



int main(int ArgCount, const char* ArgVar[]) {

	CommonOptionsParser Options(ArgCount, ArgVar, CLPKMPPCategory);
	ClangTool Tool(Options.getCompilations(), Options.getSourcePathList());

	return Tool.run(newFrontendActionFactory<CLPKMPPFrontendAction>().get());

	}
