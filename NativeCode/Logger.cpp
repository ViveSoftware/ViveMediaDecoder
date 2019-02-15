//========= Copyright 2015-2019, HTC Corporation. All rights reserved. ===========

#include "Logger.h"

#pragma warning(disable:4996)

Logger* Logger::_instance;
Logger::Logger() {
	fclose(stdout);
	freopen("NativeLog.txt", "a", stdout);
}

Logger* Logger::instance() {
	if (!_instance) {
		_instance = new Logger();
	}
	return _instance;
}

void Logger::log(const char* str, ...) {
	va_list args;
	va_start(args, str);
	vprintf(str, args);
	va_end(args);

	fflush(stdout);
}