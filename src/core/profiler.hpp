#ifndef MGE_PROFILER_HPP
#define MGE_PROFILER_HPP

// Отдельная обертка под профайлер, чтобы движок не зависел от него напрямую

#ifdef MGE_PROFILER_ENABLED
	#include <tracy/Tracy.hpp>
	#define MGE_PROFILE_FRAME()	FrameMark
	#define MGE_PROFILE_SCOPE()	ZoneScoped
	#define MGE_PROFILE_SCOPE_N(name)	ZoneScopedN(name)
	#define MGE_PROFILE_SCOPE_NC(name, color)	ZoneScopedNC(name, color)
	#define MGE_PROFILE_TEXT(txt, len)	ZoneText(txt, len)
	#define MGE_PROFILE_PLOT(name, value)	TracyPlot(name, value)
	#define MGE_PROFILE_MESSAGE(txt, len)	TracyMessage(txt, len)
	#define MGE_PROFILE_THREAD(name)	tracy::SetThreadName(name)
	#define MGE_PROFILE_LOCKABLE(type, var)	TracyLockable(type, var)
	#define MGE_PROFILE_LOCK_TYPE(type)	LockableBase(type)
#else
	#define MGE_PROFILE_FRAME()
	#define MGE_PROFILE_SCOPE()
	#define MGE_PROFILE_SCOPE_N(name)
	#define MGE_PROFILE_SCOPE_NC(name, color)
	#define MGE_PROFILE_TEXT(txt, len)
	#define MGE_PROFILE_PLOT(name, value)
	#define MGE_PROFILE_MESSAGE(txt, len)
	#define MGE_PROFILE_THREAD(name)
	#define MGE_PROFILE_LOCKABLE(type, var)   type var
	#define MGE_PROFILE_LOCK_TYPE(type)       type
#endif // MGE_PROFILER_ENABLED

#endif