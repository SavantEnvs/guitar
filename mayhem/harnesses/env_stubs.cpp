// Link-time stand-ins for the GUI-side singletons that the git/pack code references but the harnesses never use.
#include "ApplicationGlobal.h"
#include "MainWindow.h"

ApplicationGlobal *global = nullptr;

void ApplicationGlobal::writeLog(std::string_view const &)
{
}

GitRunner MainWindow::git_for_submodule(GitRunner g, GitSubmoduleItem const &)
{
	return g;
}

void ApplicationGlobal::put_trace_event(TraceEventItem const &)
{
}
