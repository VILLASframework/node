package main

//// #include "log.h"
import "C"
import "fmt"

type LogLevel int

const (
	// https://github.com/gabime/spdlog/blob/a51b4856377a71f81b6d74b9af459305c4c644f8/include/spdlog/common.h#L76
	LogLevelTrace    LogLevel = iota
	LogLevelDebug    LogLevel = iota
	LogLevelInfo     LogLevel = iota
	LogLevelWarn     LogLevel = iota
	LogLevelError    LogLevel = iota
	LogLevelCritical LogLevel = iota
	LogLevelOff      LogLevel = iota
)

type Logger struct {
	Name string

	cName *C.char
}

func GetLogger(name string) *Logger {
	return &Logger{
		Name: name,
	}
}

func (l *Logger) log(lvl LogLevel, msg string) {
	cMsg := C.CString(msg)
	C.log(l.cName, lvl, cMsg)
	C.free(cMsg)
}

func (l *Logger) Trace(msg string) {
	l.log(LogLevelTrace, msg)
}

func (l *Logger) Info(msg string) {
	l.log(LogLevelInfo, msg)
}

func (l *Logger) Warn(msg string) {
	l.log(LogLevelWarn, msg)
}

func (l *Logger) Error(msg string) {
	l.log(LogLevelError, msg)
}

func (l *Logger) Critical(msg string) {
	l.log(LogLevelCritical, msg)
}

func (l *Logger) Tracef(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	l.log(LogLevelTrace, msg)
}

func (l *Logger) Infof(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	l.log(LogLevelInfo, msg)
}

func (l *Logger) Warnf(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	l.log(LogLevelWarn, msg)
}

func (l *Logger) Errorf(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	l.log(LogLevelError, msg)
}

func (l *Logger) Criticalf(format string, args ...interface{}) {
	msg := fmt.Sprintf(format, args...)
	l.log(LogLevelCritical, msg)
}
