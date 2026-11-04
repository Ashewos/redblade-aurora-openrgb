// Test assertions report a failed check without Windows abort/WER dialogs.
// Firmware never includes this host-only header.
#pragma once
#include <stdio.h>
#include <stdlib.h>
#ifdef NDEBUG
#define assert(condition) ((void)0)
#else
#define assert(condition) ((condition) ? (void)0 : (fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__), fflush(stderr), exit(EXIT_FAILURE)))
#endif
