#pragma once
#include <QtGlobal>

// qDebug() output from the code under test would dominate the fuzz loop; drop it.
static inline void install_quiet_qt_handler()
{
	qInstallMessageHandler([](QtMsgType, QMessageLogContext const &, QString const &) {});
}
