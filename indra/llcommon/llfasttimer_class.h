/**
 * @file llfasttimer_class.h
 * @brief Declaration of a fast timer.
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_FASTTIMER_CLASS_H
#define LL_FASTTIMER_CLASS_H

#include "llinstancetracker.h"

#define FAST_TIMER_ON 1
#define TIME_FAST_TIMERS 0
#define DEBUG_FAST_TIMER_THREADS 1

class LLMutex;

#include <queue>
#include "llsd.h"

LL_COMMON_API void assert_main_thread();

class LL_COMMON_API LLFastTimer
{
public:
	class NamedTimer;

	struct LL_COMMON_API FrameState
	{
		FrameState(NamedTimer* timerp);

		U32 				mSelfTimeCounter;
		U32 				mCalls;
		FrameState*			mParent;		// info for caller timer
		FrameState*			mLastCaller;	// used to bootstrap tree construction
		NamedTimer*			mTimer;
		U16					mActiveCount;	// number of timers with this ID active on stack
		bool				mMoveUpTree;	// needs to be moved up the tree of timers at the end of frame
	};

	// stores a "named" timer instance to be reused via multiple LLFastTimer stack instances
	class LL_COMMON_API NamedTimer
	:	public LLInstanceTracker<NamedTimer>
	{
		friend class DeclareTimer;
	public:
		~NamedTimer();

		enum { HISTORY_NUM = 60 };

		const std::string& getName() const { return mName; }
		NamedTimer* getParent() const { return mParent; }
		void setParent(NamedTimer* parent);
		S32 getDepth();
		std::string getToolTip(S32 history_index = -1);

		typedef std::vector<NamedTimer*>::const_iterator child_const_iter;
		child_const_iter beginChildren();
		child_const_iter endChildren();
		std::vector<NamedTimer*>& getChildren();

		void setCollapsed(bool collapsed) { mCollapsed = collapsed; }
		bool getCollapsed() const { return mCollapsed; }

		U32 getCountAverage() const { return mCountAverage; }
		U32 getCallAverage() const { return mCallAverage; }

		U32 getHistoricalCount(S32 history_index = 0) const;
		U32 getHistoricalCalls(S32 history_index = 0) const;

		static NamedTimer& getRootNamedTimer();

		S32 getFrameStateIndex() const { return mFrameStateIndex; }

		FrameState& getFrameState() const;

	private:
		friend class LLFastTimer;
		friend class NamedTimerFactory;

		//
		// methods
		//
		NamedTimer(const std::string& name);
		// recursive call to gather total time from children
		static void accumulateTimings();

		// updates cumulative times and hierarchy,
		// can be called multiple times in a frame, at any point
		static void processTimes();

		static void buildHierarchy();
		static void resetFrame();
		static void reset();

		//
		// members
		//
		S32			mFrameStateIndex;

		std::string	mName;

		U32 		mTotalTimeCounter;

		U32 		mCountAverage;
		U32			mCallAverage;

		U32*		mCountHistory;
		U32*		mCallHistory;

		// tree structure
		NamedTimer*					mParent;				// NamedTimer of caller(parent)
		std::vector<NamedTimer*>	mChildren;
		bool						mCollapsed;				// don't show children
		bool						mNeedsSorting;			// sort children whenever child added
	};

	// used to statically declare a new named timer
	class LL_COMMON_API DeclareTimer
	:	public LLInstanceTracker<DeclareTimer>
	{
		friend class LLFastTimer;
	public:
		DeclareTimer(const std::string& name, bool open);
		DeclareTimer(const std::string& name);

		static void updateCachedPointers();

	private:
		NamedTimer&		mTimer;
		FrameState*		mFrameState;
	};

public:
	LLFastTimer(LLFastTimer::FrameState* state);

	LL_FORCE_INLINE LLFastTimer(LLFastTimer::DeclareTimer& timer)
	:	mFrameState(timer.mFrameState)
	{
#if TIME_FAST_TIMERS
		U64 timer_start = getCPUClockCount64();
#endif
#if FAST_TIMER_ON
		LLFastTimer::FrameState* frame_state = mFrameState;
		mStartTime = getCPUClockCount32();

		frame_state->mActiveCount++;
		frame_state->mCalls++;
		// keep current parent as long as it is active when we are
		frame_state->mMoveUpTree |= (frame_state->mParent->mActiveCount == 0);

		LLFastTimer::CurTimerData* cur_timer_data = &LLFastTimer::sCurTimerData;
		mLastTimerData = *cur_timer_data;
		cur_timer_data->mCurTimer = this;
		cur_timer_data->mFrameState = frame_state;
		cur_timer_data->mChildTime = 0;
#endif
#if TIME_FAST_TIMERS
		U64 timer_end = getCPUClockCount64();
		sTimerCycles += timer_end - timer_start;
#endif
#if DEBUG_FAST_TIMER_THREADS
#if !LL_RELEASE
		assert_main_thread();
#endif
#endif
	}

	LL_FORCE_INLINE ~LLFastTimer()
	{
#if TIME_FAST_TIMERS
		U64 timer_start = getCPUClockCount64();
#endif
#if FAST_TIMER_ON
		LLFastTimer::FrameState* frame_state = mFrameState;
		U32 total_time = getCPUClockCount32() - mStartTime;

		frame_state->mSelfTimeCounter += total_time - LLFastTimer::sCurTimerData.mChildTime;
		frame_state->mActiveCount--;

		// store last caller to bootstrap tree creation
		// do this in the destructor in case of recursion to get topmost caller
		frame_state->mLastCaller = mLastTimerData.mFrameState;

		// we are only tracking self time, so subtract our total time delta from parents
		mLastTimerData.mChildTime += total_time;

		LLFastTimer::sCurTimerData = mLastTimerData;
#endif
#if TIME_FAST_TIMERS
		U64 timer_end = getCPUClockCount64();
		sTimerCycles += timer_end - timer_start;
		sTimerCalls++;
#endif
	}

public:
	static LLMutex*			sLogLock;
	static std::queue<LLSD> sLogQueue;
	static BOOL				sLog;
	static BOOL				sMetricLog;
	static std::string		sLogName;
	static bool 			sPauseHistory;
	static bool 			sResetHistory;
	static U64				sTimerCycles;
	static U32				sTimerCalls;

	typedef std::vector<FrameState> info_list_t;
	static info_list_t& getFrameStateList();


	// call this once a frame to reset timers
	static void nextFrame();

	// dumps current cumulative frame stats to log
	// call nextFrame() to reset timers
	static void dumpCurTimes();

	// call this to reset timer hierarchy, averages, etc.
	static void reset();

	static U64 countsPerSecond();
	static S32 getLastFrameIndex() { return sLastFrameIndex; }
	static S32 getCurFrameIndex() { return sCurFrameIndex; }

	static void writeLog(std::ostream& os);
	static const NamedTimer* getTimerByName(const std::string& name);

	struct CurTimerData
	{
		LLFastTimer*	mCurTimer;
		FrameState*		mFrameState;
		U32				mChildTime;
	};
	static CurTimerData		sCurTimerData;
	static std::string sClockType;

private:
	static U32 getCPUClockCount32();
	static U64 getCPUClockCount64();
	static U64 sClockResolution;

	static S32				sCurFrameIndex;
	static S32				sLastFrameIndex;
	static U64				sLastFrameTime;
	static info_list_t*		sTimerInfos;

	U32							mStartTime;
	LLFastTimer::FrameState*	mFrameState;
	LLFastTimer::CurTimerData	mLastTimerData;

};

typedef class LLFastTimer LLFastTimer;

#endif // LL_LLFASTTIMER_CLASS_H
