#pragma once

// Platform-specific fixes and macro handling

#ifdef _WIN32
// Ensure Windows headers use lean and mean mode
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Define a macro to undefine problematic Windows macros
#define UNDEFINE_WINDOWS_MACROS()                            \
  do {                                                       \
    /* Undefine Windows macros that conflict with logging */ \
  } while (0)

#else
// Non-Windows platforms - no action needed
#define UNDEFINE_WINDOWS_MACROS()
#endif

// Force undefine these macros if they exist
#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef FATAL
#undef FATAL
#endif
#ifdef TRACE
#undef TRACE
#endif
