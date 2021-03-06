/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2010, 2011 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "couchbase_ext.h"

#ifndef HAVE_GETHRTIME

#include <stdlib.h>
#include <time.h>
#include <assert.h>

#ifdef HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/*
 * OS X doesn't have clock_gettime, but for monotonic, we can build
 * one with mach_absolute_time as shown below.
 *
 * Most of the idea came from
 * http://www.wand.net.nz/~smr26/wordpress/2009/01/19/monotonic-time-in-mac-os-x/
 */

#if defined(HAVE_MACH_ABSOLUTE_TIME) && !defined(HAVE_CLOCK_GETTIME)

#define CLOCK_MONOTONIC 192996728

static void mach_absolute_difference(uint64_t start, uint64_t end,
                                     struct timespec *tp)
{
    uint64_t difference = end - start;
    static mach_timebase_info_data_t info = {0, 0};

    if (info.denom == 0) {
        mach_timebase_info(&info);
    }

    uint64_t elapsednano = difference * (info.numer / info.denom);

    tp->tv_sec = elapsednano * 1e-9;
    tp->tv_nsec = elapsednano - (tp->tv_sec * 1e9);
}

static int clock_gettime(int which, struct timespec *tp)
{
    assert(which == CLOCK_MONOTONIC);

    static uint64_t epoch = 0;

    if (epoch == 0) {
        epoch = mach_absolute_time();
    }

    uint64_t now = mach_absolute_time();

    mach_absolute_difference(epoch, now, tp);

    return 0;
}

#define HAVE_CLOCK_GETTIME 1
#endif

hrtime_t gethrtime(void)
{
#ifdef HAVE_CLOCK_GETTIME
    struct timespec tm;
    if (clock_gettime(CLOCK_MONOTONIC, &tm) == -1) {
        abort();
    }
    return (((hrtime_t)tm.tv_sec) * 1000000000) + (hrtime_t)tm.tv_nsec;
#elif HAVE_GETTIMEOFDAY

    hrtime_t ret;
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        return (-1ULL);
    }

    ret = (hrtime_t)tv.tv_sec * 1000000000;
    ret += tv.tv_usec * 1000;
    return ret;
#elif defined(HAVE_QUERYPERFORMANCECOUNTER)
    double ret;
    // To fix the potential race condition for the local static variable,
    // gethrtime should be called in a global static variable first.
    // It will guarantee the local static variable will be initialized
    // before any thread calls the function.
    static LARGE_INTEGER pf = { 0 };
    static double freq;
    LARGE_INTEGER currtime;

    if (pf.QuadPart == 0) {
        if (QueryPerformanceFrequency(&pf)) {
            assert(pf.QuadPart != 0);
            freq = 1.0e9 / (double)pf.QuadPart;
        } else {
            abort();
        }
    }

    QueryPerformanceCounter(&currtime);

    ret = (double)currtime.QuadPart * freq ;
    return (hrtime_t)ret;
#else
#error "I don't know how to build a highres clock..."
#endif
}

#endif
