/** 
* @file llchiclet.h
* @brief LLChiclet class header file
*
* $LicenseInfo:firstyear=2002&license=viewergpl$
* 
* Copyright (c) 2002-2009, Linden Research, Inc.
* 
* Second Life Viewer Source Code
* The source code in this file ("Source Code") is provided by Linden Lab
* to you under the terms of the GNU General Public License, version 2.0
* ("GPL"), unless you have obtained a separate licensing agreement
* ("Other License"), formally executed by you and Linden Lab.  Terms of
* the GPL can be found in doc/GPL-license.txt in this distribution, or
* online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
* 
* There are special exceptions to the terms and conditions of the GPL as
* it is applied to this Source Code. View the full text of the exception
* in the file doc/FLOSS-exception.txt in this software distribution, or
* online at
* http://secondlifegrid.net/programs/open_source/licensing/flossexception
* 
* By copying, modifying or distributing this software, you acknowledge
* that you have read and understood your obligations described above,
* and agree to abide by those obligations.
* 
* ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
* WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
* COMPLETENESS OR PERFORMANCE.
* $/LicenseInfo$
*/

#ifndef LL_LLCHICLET_H
#define LL_LLCHICLET_H

#include "llavatariconctrl.h"
#include "llbutton.h"
#include "llpanel.h"
#include "lltextbox.h"
#include "lloutputmonitorctrl.h"
#include "llgroupmgr.h"

class LLVoiceControlPanel;
class LLMenuGL;
class LLIMFloater;

/*
 * Class for displaying amount of messages/notifications(unread).
*/
class LLChicletNotificationCounterCtrl : public LLTextBox
{
public:

	struct Params :	public LLInitParam::Block<Params, LLTextBox::Params>
	{
		Params()
		{};
	};

	/*
	 * Sets number of notifications
	*/
	virtual void setCounter(S32 counter);

	/*
	 * Returns number of notifications
	*/
	virtual S32 getCounter() const { return mCounter; }

	/*
	 * Returns width, required to display amount of notifications in text form.
	 * Width is the only valid value.
	*/
	/*virtual*/ LLRect getRequiredRect();

	/*
	 * Sets number of notifications using LLSD
	*/
	/*virtual*/ void setValue(const LLSD& value);

	/*
	 * Returns number of notifications wrapped in LLSD
	*/
	/*virtual*/ LLSD getValue() const;

protected:

	LLChicletNotificationCounterCtrl(const Params& p);
	friend class LLUICtrlFactory;

private:

	S32 mCounter;
	S32 mInitialWidth;
};

/*
 * Class for displaying avatar's icon in P2P chiclet.
*/
class LLChicletAvatarIconCtrl : public LLAvatarIconCtrl
{
public:

	struct Params :	public LLInitParam::Block<Params, LLAvatarIconCtrl::Params>
	{
		Params()
		{
			draw_tooltip(FALSE);
			mouse_opaque(FALSE);
		};
	};

protected:

	LLChicletAvatarIconCtrl(const Params& p);
	friend class LLUICtrlFactory;
};

/**
 * Class for displaying group's icon in Group chiclet.
 */
class LLChicletGroupIconCtrl : public LLIconCtrl
{
public:

	struct Params :	public LLInitParam::Block<Params, LLIconCtrl::Params>
	{
		Optional<std::string> default_icon;

		Params()
		 : default_icon("default_icon", "default_land_picture.j2c")
		{
		};
	};

	/**
	 * Sets icon, if value is LLUUID::null - default icon will be set.
	 */
	virtual void setValue(const LLSD& value );

protected:

	LLChicletGroupIconCtrl(const Params& p);
	friend class LLUICtrlFactory;

	std::string mDefaultIcon;
};

/*
 * Class for displaying of speaker's voice indicator 
*/
class LLChicletSpeakerCtrl : public LLOutputMonitorCtrl
{
public:

	struct Params : public LLInitParam::Block<Params, LLOutputMonitorCtrl::Params>
	{
		Params(){};
	};
protected:

	LLChicletSpeakerCtrl(const Params&p);
	friend class LLUICtrlFactory;
};

/*
 * Base class for all chiclets.
 */
class LLChiclet : public LLUICtrl
{
public:

	struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		Optional<bool> show_counter;

		Params();
	};

	/*virtual*/ ~LLChiclet();

	/*
	 * Associates chat session id with chiclet.
	*/
	virtual void setSessionId(const LLUUID& session_id) { mSessionId = session_id; }

	/*
	 * Returns associated chat session.
	*/
	virtual const LLUUID& getSessionId() const { return mSessionId; }

	/*
	 * Sets number of unread notifications.
	*/
	virtual void setCounter(S32 counter) = 0;

	/*
	 * Returns number of unread notifications.
	*/
	virtual S32 getCounter() = 0;

	/*
	 * Sets show counter state.
	*/
	virtual void setShowCounter(bool show) { mShowCounter = show; }

	/*
	 * Returns show counter state.
	*/
	virtual bool getShowCounter() {return mShowCounter;};

	/*
	 * Connects chiclet clicked event with callback.
	*/
	/*virtual*/ boost::signals2::connection setLeftButtonClickCallback(
		const commit_callback_t& cb);

	typedef boost::function<void (LLChiclet* ctrl, const LLSD& param)> 
		chiclet_size_changed_callback_t;

	/*
	 * Connects chiclets size changed event with callback.
	*/
	virtual boost::signals2::connection setChicletSizeChangedCallback(
		const chiclet_size_changed_callback_t& cb);

	/*
	 * Sets IM Session id using LLSD
	*/
	/*virtual*/ LLSD getValue() const;

	/*
	 * Returns IM Session id using LLSD
	*/
	/*virtual*/ void setValue(const LLSD& value);

protected:

	friend class LLUICtrlFactory;
	LLChiclet(const Params& p);

	/*
	 * Notifies subscribers about click on chiclet.
	*/
	/*virtual*/ BOOL handleMouseDown(S32 x, S32 y, MASK mask);

	/*
	 * Notifies subscribers about chiclet size changed event.
	*/
	virtual void onChicletSizeChanged();

private:

	LLUUID mSessionId;

	bool mShowCounter;

	typedef boost::signals2::signal<void (LLChiclet* ctrl, const LLSD& param)> 
		chiclet_size_changed_signal_t;

	chiclet_size_changed_signal_t mChicletSizeChangedSignal;
};


/*
* Base class for Instant Message chiclets.
* IMChiclet displays icon, number of unread messages(optional)
* and voice chat status(optional).
*/
class LLIMChiclet : public LLChiclet
{
public:
	enum EType {
		TYPE_UNKNOWN,
		TYPE_IM,
		TYPE_GROUP,
		TYPE_AD_HOC
	};
	struct Params : public LLInitParam::Block<Params, LLChiclet::Params>
	{
		Optional<std::string> new_messages_icon_name;

		Params() : new_messages_icon_name("new_messages_icon_name", "icn_voice-localchat.tga")
		{}
	};

	
	/*virtual*/ ~LLIMChiclet() {};

	/*
	 * Sets IM session name. This name will be displayed in chiclet tooltip.
	*/
	virtual void setIMSessionName(const std::string& name) { setToolTip(name); }

	/*
	 * Sets id of person/group user is chatting with.
	 * Session id should be set before calling this
	*/
	virtual void setOtherParticipantId(const LLUUID& other_participant_id) { mOtherParticipantId = other_participant_id; }

	/*
	 * Gets id of person/group user is chatting with.
	 */
	virtual LLUUID getOtherParticipantId() { return mOtherParticipantId; }

	/*
	* Init Speaker Control with speaker's ID
	*/
	virtual void initSpeakerControl();

	/*
	 * set status (Shows/Hide) for voice control.
	*/
	virtual void setShowSpeaker(bool show);

	/*
	 * Returns voice chat status control visibility.
	*/
	virtual bool getShowSpeaker() {return mShowSpeaker;};

	/*
	* Shows/Hides for voice control for a chiclet.
	*/
	virtual void toggleSpeakerControl();

	/*
	* Shows/hides overlay icon concerning new unread messages.
	*/
	virtual void setShowNewMessagesIcon(bool show);

	/*
	* Returns visibility of overlay icon concerning new unread messages.
	*/
	virtual bool getShowNewMessagesIcon();

	virtual void draw();

	/**
	 * Determine whether given ID refers to a group or an IM chat session.
	 * 
	 * This is used when we need to chose what IM chiclet (P2P/group)
	 * class to instantiate.
	 * 
	 * @param session_id session ID.
	 * @return TYPE_GROUP in case of group chat session,
	 *         TYPE_IM in case of P2P session,
	 *         TYPE_UNKNOWN otherwise.
	 */
	static EType getIMSessionType(const LLUUID& session_id);

	/**
	 * The action taken on mouse down event.
	 * 
	 * Made public so that it can be triggered from outside
	 * (more specifically, from the Active IM window).
	 */
	void onMouseDown();

protected:

	LLIMChiclet(const LLIMChiclet::Params& p);

	/*virtual*/ BOOL handleMouseDown(S32 x, S32 y, MASK mask);

protected:

	bool mShowSpeaker;

	LLIconCtrl* mNewMessagesIcon;
	LLChicletNotificationCounterCtrl* mCounterCtrl;
	LLChicletSpeakerCtrl* mSpeakerCtrl;


	/** the id of another participant, either an avatar id or a group id*/
	LLUUID mOtherParticipantId;

	template<typename Container>
	struct CollectChicletCombiner {
		typedef Container result_type;

		template<typename InputIterator>
		Container operator()(InputIterator first, InputIterator last) const {
			Container c = Container();
			for (InputIterator iter = first; iter != last; iter++) {
				if (*iter != NULL) {
					c.push_back(*iter);
				}
			}
			return c;
		}
	};

public:
	static boost::signals2::signal<LLChiclet* (const LLUUID&),
			CollectChicletCombiner<std::list<LLChiclet*> > >
			sFindChicletsSignal;
};

/**
 * Implements P2P chiclet.
 */
class LLIMP2PChiclet : public LLIMChiclet
{
public:
	struct Params : public LLInitParam::Block<Params, LLIMChiclet::Params>
	{
		Optional<LLChicletAvatarIconCtrl::Params> avatar_icon;

		Optional<LLChicletNotificationCounterCtrl::Params> unread_notifications;

		Optional<LLChicletSpeakerCtrl::Params> speaker;

		Optional<bool>	show_speaker;

		Params();
	};

	/* virtual */ void setOtherParticipantId(const LLUUID& other_participant_id);

	/*
	* Sets number of unread messages. Will update chiclet's width if number text 
	* exceeds size of counter and notify it's parent about size change.
	*/
	/*virtual*/ void setCounter(S32);

	/*
	* Init Speaker Control with speaker's ID
	*/
	/*virtual*/ void initSpeakerControl();

	/*
	* Returns number of unread messages.
	*/
	/*virtual*/ S32 getCounter() { return mCounterCtrl->getCounter(); }

protected:
	LLIMP2PChiclet(const Params& p);
	friend class LLUICtrlFactory;

	/*
	* Creates chiclet popup menu. Will create P2P or Group IM Chat menu 
	* based on other participant's id.
	*/
	virtual void createPopupMenu();

	/*
	* Processes clicks on chiclet popup menu.
	*/
	virtual void onMenuItemClicked(const LLSD& user_data);

	/*
	* Displays popup menu.
	*/
	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);

	/* 
	* Enables/disables menus based on relationship with other participant.
	*/
	virtual void updateMenuItems();

private:

	LLChicletAvatarIconCtrl* mChicletIconCtrl;
	LLMenuGL* mPopupMenu;
};

/**
 * Implements AD-HOC chiclet.
 */
class LLAdHocChiclet : public LLIMChiclet
{
public:
	struct Params : public LLInitParam::Block<Params, LLIMChiclet::Params>
	{
		Optional<LLChicletAvatarIconCtrl::Params> avatar_icon;

		Optional<LLChicletNotificationCounterCtrl::Params> unread_notifications;

		Optional<LLChicletSpeakerCtrl::Params> speaker;

		Optional<bool>	show_speaker;

		Optional<LLColor4>	avatar_icon_color;

		Params();
	};

	/**
	 * Sets session id.
	 * Session ID for group chat is actually Group ID.
	 */
	/*virtual*/ void setSessionId(const LLUUID& session_id);

	/*
	* Sets number of unread messages. Will update chiclet's width if number text 
	* exceeds size of counter and notify it's parent about size change.
	*/
	/*virtual*/ void setCounter(S32);

	/*
	* Keep Speaker Control with actual speaker's ID
	*/
	/*virtual*/ void draw();

	/*
	* Init Speaker Control with speaker's ID
	*/
	/*virtual*/ void initSpeakerControl();

	/*
	* Returns number of unread messages.
	*/
	/*virtual*/ S32 getCounter() { return mCounterCtrl->getCounter(); }

protected:
	LLAdHocChiclet(const Params& p);
	friend class LLUICtrlFactory;

	/*
	* Displays popup menu.
	*/
	virtual BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);

	/*
	* Finds a current speaker and resets the SpeakerControl with speaker's ID
	*/
	/*virtual*/ void switchToCurrentSpeaker();

private:

	LLChicletAvatarIconCtrl* mChicletIconCtrl;
	LLMenuGL* mPopupMenu;
};

/**
 * Implements Group chat chiclet.
 */
class LLIMGroupChiclet : public LLIMChiclet, public LLGroupMgrObserver
{
public:

	struct Params : public LLInitParam::Block<Params, LLIMChiclet::Params>
	{
		Optional<LLChicletGroupIconCtrl::Params> group_icon;

		Optional<LLChicletNotificationCounterCtrl::Params> unread_notifications;

		Optional<LLChicletSpeakerCtrl::Params> speaker;

		Optional<bool>	show_speaker;

		Params();
	};

	/**
	 * Sets session id.
	 * Session ID for group chat is actually Group ID.
	 */
	/*virtual*/ void setSessionId(const LLUUID& session_id);

	/*
	* Keep Speaker Control with actual speaker's ID
	*/
	/*virtual*/ void draw();

	/**
	 * Callback for LLGroupMgrObserver, we get this when group data is available or changed.
	 * Sets group icon.
	 */
	/*virtual*/ void changed(LLGroupChange gc);

	/*
	* Sets number of unread messages. Will update chiclet's width if number text 
	* exceeds size of counter and notify it's parent about size change.
	*/
	/*virtual*/ void setCounter(S32);

	/*
	* Init Speaker Control with speaker's ID
	*/
	/*virtual*/ void initSpeakerControl();

	/*
	* Returns number of unread messages.
	*/
	/*virtual*/ S32 getCounter() { return mCounterCtrl->getCounter(); }

	~LLIMGroupChiclet();

protected:
	LLIMGroupChiclet(const Params& p);
	friend class LLUICtrlFactory;

	/*
	* Finds a current speaker and resets the SpeakerControl with speaker's ID
	*/
	/*virtual*/ void switchToCurrentSpeaker();

	/*
	* Creates chiclet popup menu. Will create P2P or Group IM Chat menu 
	* based on other participant's id.
	*/
	virtual void createPopupMenu();

	/*
	* Processes clicks on chiclet popup menu.
	*/
	virtual void onMenuItemClicked(const LLSD& user_data);

	/*
	* Displays popup menu.
	*/
	/*virtual*/ BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);

private:

	LLChicletGroupIconCtrl* mChicletIconCtrl;
	LLMenuGL* mPopupMenu;
};

/*
 * Implements notification chiclet. Used to display total amount of unread messages 
 * across all IM sessions, total amount of system notifications.
*/
class LLNotificationChiclet : public LLChiclet
{
public:

	struct Params : public LLInitParam::Block<Params, LLChiclet::Params>
	{
		Optional<LLButton::Params> button;

		Optional<LLChicletNotificationCounterCtrl::Params> unread_notifications;

		Params();
	};

	/*virtual*/ void setCounter(S32 counter);

	/*virtual*/S32 getCounter() { return mCounterCtrl->getCounter(); }

	/*virtual*/ void setShowCounter(bool show);

	boost::signals2::connection setClickCallback(const commit_callback_t& cb);

	/*virtual*/ ~ LLNotificationChiclet();

	// methods for updating a number of unread System notifications
	void incUreadSystemNotifications() { setCounter(++mUreadSystemNotifications); }
	void decUreadSystemNotifications() { setCounter(--mUreadSystemNotifications); }
	void setToggleState(BOOL toggled);

protected:
	// connect counter updaters to the corresponding signals
	void connectCounterUpdatersToSignal(std::string notification_type);

	LLNotificationChiclet(const Params& p);
	friend class LLUICtrlFactory;

	static S32 mUreadSystemNotifications;

protected:
	LLButton* mButton;
	LLChicletNotificationCounterCtrl* mCounterCtrl;
};

/*
 * Storage class for all IM chiclets. Provides mechanism to display, 
 * scroll, create, remove chiclets.
*/
class LLChicletPanel : public LLPanel
{
public:

	struct Params :	public LLInitParam::Block<Params, LLPanel::Params>
	{
		Optional<S32> chiclet_padding,
					  scrolling_offset;

		Optional<LLButton::Params> left_scroll_button,
								   right_scroll_button;

		Optional<S32> min_width;

		Params();
	};

	virtual ~LLChicletPanel();

	/*
	 * Creates chiclet and adds it to chiclet list at specified index.
	*/
	template<class T> T* createChiclet(const LLUUID& session_id, S32 index);

	/*
	 * Creates chiclet and adds it to chiclet list at right.
	*/
	template<class T> T* createChiclet(const LLUUID& session_id);

	/*
	 * Returns pointer to chiclet of specified type at specified index.
	*/
	template<class T> T* getChiclet(S32 index);

	/*
	 * Returns pointer to LLChiclet at specified index.
	*/
	LLChiclet* getChiclet(S32 index) { return getChiclet<LLChiclet>(index); }

	/*
	 * Searches a chiclet using IM session id.
	*/
	template<class T> T* findChiclet(const LLUUID& im_session_id);

	/*
	 * Returns number of hosted chiclets.
	*/
	S32 getChicletCount() {return mChicletList.size();};

	/*
	 * Returns index of chiclet in list.
	*/
	S32 getChicletIndex(const LLChiclet* chiclet);

	/*
	 * Removes chiclet by index.
	*/
	void removeChiclet(S32 index);

	/*
	 * Removes chiclet by pointer.
	*/
	void removeChiclet(LLChiclet* chiclet);

	/*
	 * Removes chiclet by IM session id.
	*/
	void removeChiclet(const LLUUID& im_session_id);

	/*
	 * Removes all chiclets.
	*/
	void removeAll();

	/*
	 * Scrolls the panel to the specified chiclet
	 */
	void scrollToChiclet(const LLChiclet* chiclet);

	boost::signals2::connection setChicletClickedCallback(
		const commit_callback_t& cb);

	/*virtual*/ BOOL postBuild();

	/*
	* Handler for the Voice Client's signal. Finds a corresponding chiclet and toggles its SpeakerControl
	*/
	void onCurrentVoiceChannelChanged(const LLUUID& session_id);

	/*
	 * Reshapes controls and rearranges chiclets if needed.
	*/
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE );

	/*virtual*/ void draw();

	S32 getMinWidth() const { return mMinWidth; }

protected:
	LLChicletPanel(const Params&p);
	friend class LLUICtrlFactory;

	S32 calcChickletPanleWidth();

	/*
	 * Adds chiclet to list and rearranges all chiclets.
	*/
	bool addChiclet(LLChiclet*, S32 index);

	/*
	 * Arranges chiclets.
	*/
	void arrange();

	/*
	 * Returns true if chiclets can be scrolled right.
	*/
	bool canScrollRight();

	/*
	* Returns true if chiclets can be scrolled left.
	*/
	bool canScrollLeft();

	/*
	* Shows or hides chiclet scroll buttons if chiclets can or can not be scrolled.
	*/
	void showScrollButtonsIfNeeded();

	/*
	 * Shifts chiclets left or right.
	*/
	void shiftChiclets(S32 offset, S32 start_index = 0);

	/*
	 * Removes gaps between first chiclet and scroll area left side,
	 * last chiclet and scroll area right side.
	*/
	void trimChiclets();

	/*
	 * Scrolls chiclets to right or left.
	*/
	void scroll(S32 offset);

	/*
	 * Verifies that chiclets can be scrolled left, then calls scroll()
	*/
	void scrollLeft();

	/*
	 * Verifies that chiclets can be scrolled right, then calls scroll()
	*/
	void scrollRight();

	/*
	 * Callback for left scroll button clicked
	*/
	void onLeftScrollClick();

	/*
	* Callback for right scroll button clicked
	*/
	void onRightScrollClick();

	/*
	 * Callback for mouse wheel scrolled, calls scrollRight() or scrollLeft()
	*/
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks);

	/*
	 * Notifies subscribers about click on chiclet.
	 * Do not place any code here, instead subscribe on event (see setChicletClickedCallback).
	*/
	void onChicletClick(LLUICtrl*ctrl,const LLSD&param);

	/*
	 * Callback for chiclet size changed event, rearranges chiclets.
	*/
	void onChicletSizeChanged(LLChiclet* ctrl, const LLSD& param);

	typedef std::vector<LLChiclet*> chiclet_list_t;

	/*
	 * Removes chiclet from scroll area and chiclet list.
	*/
	void removeChiclet(chiclet_list_t::iterator it);

	S32 getChicletPadding() { return mChicletPadding; }

	S32 getScrollingOffset() { return mScrollingOffset; }

	bool isAnyIMFloaterDoked();

protected:

	chiclet_list_t mChicletList;
	LLButton* mLeftScrollButton;
	LLButton* mRightScrollButton;
	LLPanel* mScrollArea;

	S32 mChicletPadding;
	S32 mScrollingOffset;
	S32 mMinWidth;
	bool mShowControls;
};

template<class T> 
T* LLChicletPanel::createChiclet(const LLUUID& session_id, S32 index)
{
	typename T::Params params;
	T* chiclet = LLUICtrlFactory::create<T>(params);
	if(!chiclet)
	{
		llwarns << "Could not create chiclet" << llendl;
		return NULL;
	}
	if(!addChiclet(chiclet, index))
	{
		delete chiclet;
		llwarns << "Could not add chiclet to chiclet panel" << llendl;
		return NULL;
	}

	if (!isAnyIMFloaterDoked())
	{
		scrollToChiclet(chiclet);
	}

	chiclet->setSessionId(session_id);

	return chiclet;
}

template<class T>
T* LLChicletPanel::createChiclet(const LLUUID& session_id)
{
	return createChiclet<T>(session_id, mChicletList.size());
}

template<class T>
T* LLChicletPanel::findChiclet(const LLUUID& im_session_id)
{
	if(im_session_id.isNull())
	{
		return NULL;
	}

	chiclet_list_t::const_iterator it = mChicletList.begin();
	for( ; mChicletList.end() != it; ++it)
	{
		LLChiclet* chiclet = *it;

		if(chiclet->getSessionId() == im_session_id)
		{
			T* result = dynamic_cast<T*>(chiclet);
			if(!result && chiclet)
			{
				llwarns << "Found chiclet but of wrong type " << llendl;
			}
			return result;
		}
	}
	return NULL;
}

template<class T> T* LLChicletPanel::getChiclet(S32 index)
{
	if(index < 0 || index >= getChicletCount())
	{
		return NULL;
	}

	LLChiclet* chiclet = mChicletList[index];
	T*result = dynamic_cast<T*>(chiclet);
	if(!result && chiclet)
	{
		llwarns << "Found chiclet but of wrong type " << llendl;
	}
	return result;
}

#endif // LL_LLCHICLET_H
