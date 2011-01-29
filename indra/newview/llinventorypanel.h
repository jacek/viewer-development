/** 
 * @file llinventorypanel.h
 * @brief LLInventoryPanel
 * class definition
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLINVENTORYPANEL_H
#define LL_LLINVENTORYPANEL_H

#include "llassetstorage.h"
#include "lldarray.h"
#include "llfloater.h"
#include "llinventory.h"
#include "llinventoryfilter.h"
#include "llfolderview.h"
#include "llinventorymodel.h"
#include "lluictrlfactory.h"
#include <set>

class LLFolderViewItem;
class LLInventoryFilter;
class LLInventoryModel;
class LLInvFVBridge;
class LLInventoryFVBridgeBuilder;
class LLMenuBarGL;
class LLCheckBoxCtrl;
class LLSpinCtrl;
class LLScrollContainer;
class LLTextBox;
class LLIconCtrl;
class LLSaveFolderState;
class LLFilterEditor;
class LLTabContainer;
class LLInvPanelComplObserver;

class LLInventoryPanel : public LLPanel
{
	//--------------------------------------------------------------------
	// Data
	//--------------------------------------------------------------------
public:
	struct Filter : public LLInitParam::Block<Filter>
	{
		Optional<U32>			sort_order;
		Optional<U32>			types;
		Optional<std::string>	search_string;

		Filter()
		:	sort_order("sort_order"),
			types("types", 0xffffffff),
			search_string("search_string")
		{}
	};

	struct Params 
	:	public LLInitParam::Block<Params, LLPanel::Params>
	{
		Optional<std::string>				sort_order_setting;
		Optional<LLInventoryModel*>			inventory;
		Optional<bool>						allow_multi_select;
		Optional<bool>						show_item_link_overlays;
		Optional<Filter>					filter;
		Optional<std::string>               start_folder;
		Optional<bool>						use_label_suffix;

		Params()
		:	sort_order_setting("sort_order_setting"),
			inventory("", &gInventory),
			allow_multi_select("allow_multi_select", true),
			show_item_link_overlays("show_item_link_overlays", false),
			filter("filter"),
			start_folder("start_folder"),
			use_label_suffix("use_label_suffix", true)
		{}
	};

	//--------------------------------------------------------------------
	// Initialization
	//--------------------------------------------------------------------
protected:
	LLInventoryPanel(const Params&);
	void initFromParams(const Params&);
	friend class LLUICtrlFactory;
public:
	virtual ~LLInventoryPanel();

public:
	LLInventoryModel* getModel() { return mInventory; }

	// LLView methods
	void draw();
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);
	// LLUICtrl methods
	 /*virtual*/ void onFocusLost();
	 /*virtual*/ void onFocusReceived();

	// Call this method to set the selection.
	void openAllFolders();
	void setSelection(const LLUUID& obj_id, BOOL take_keyboard_focus);
	void setSelectCallback(const LLFolderView::signal_t::slot_type& cb);
	void clearSelection();
	LLInventoryFilter* getFilter();
	void setFilterTypes(U64 filter, LLInventoryFilter::EFilterType = LLInventoryFilter::FILTERTYPE_OBJECT);
	U32 getFilterObjectTypes() const { return mFolderRoot->getFilterObjectTypes(); }
	void setFilterPermMask(PermissionMask filter_perm_mask);
	U32 getFilterPermMask() const { return mFolderRoot->getFilterPermissions(); }
	void setFilterWearableTypes(U64 filter);
	void setFilterSubString(const std::string& string);
	const std::string getFilterSubString() { return mFolderRoot->getFilterSubString(); }
	void setSinceLogoff(BOOL sl);
	void setHoursAgo(U32 hours);
	BOOL getSinceLogoff();
	void setFilterLinks(U64 filter_links);

	void setShowFolderState(LLInventoryFilter::EFolderShow show);
	LLInventoryFilter::EFolderShow getShowFolderState();
	void setAllowMultiSelect(BOOL allow) { mFolderRoot->setAllowMultiSelect(allow); }
	// This method is called when something has changed about the inventory.
	void modelChanged(U32 mask);
	LLFolderView* getRootFolder() { return mFolderRoot; }
	LLScrollContainer* getScrollableContainer() { return mScroller; }
	
	void onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action);
	
	// Callbacks
	void doToSelected(const LLSD& userdata);
	void doCreate(const LLSD& userdata);
	bool beginIMSession();
	bool attachObject(const LLSD& userdata);
	
	// DEBUG ONLY:
	static void dumpSelectionInformation(void* user_data);

	void openSelected();
	void unSelectAll()	{ mFolderRoot->setSelection(NULL, FALSE, FALSE); }
	
	static void onIdle(void* user_data);

	// Find whichever inventory panel is active / on top.
	// "Auto_open" determines if we open an inventory panel if none are open.
	static LLInventoryPanel *getActiveInventoryPanel(BOOL auto_open = TRUE);

protected:
	void openStartFolderOrMyInventory(); // open the first level of inventory
	void onItemsCompletion();			// called when selected items are complete

	LLInventoryModel*			mInventory;
	LLInventoryObserver*		mInventoryObserver;
	LLInvPanelComplObserver*	mCompletionObserver;
	BOOL 						mAllowMultiSelect;
	BOOL 						mShowItemLinkOverlays; // Shows link graphic over inventory item icons

	LLFolderView*				mFolderRoot;
	LLScrollContainer*			mScroller;

	/**
	 * Pointer to LLInventoryFVBridgeBuilder.
	 *
	 * It is set in LLInventoryPanel's constructor and can be overridden in derived classes with 
	 * another implementation.
	 * Take into account it will not be deleted by LLInventoryPanel itself.
	 */
	const LLInventoryFVBridgeBuilder* mInvFVBridgeBuilder;


	//--------------------------------------------------------------------
	// Sorting
	//--------------------------------------------------------------------
public:
	static const std::string DEFAULT_SORT_ORDER;
	static const std::string RECENTITEMS_SORT_ORDER;
	static const std::string INHERIT_SORT_ORDER;
	
	void setSortOrder(U32 order);
	U32 getSortOrder() const { return mFolderRoot->getSortOrder(); }
private:
	std::string					mSortOrderSetting;

	//--------------------------------------------------------------------
	// Hidden folders
	//--------------------------------------------------------------------
public:
	void addHideFolderType(LLFolderType::EType folder_type);
protected:
	BOOL getIsHiddenFolderType(LLFolderType::EType folder_type) const;
private:
	std::vector<LLFolderType::EType> mHiddenFolderTypes;

	//--------------------------------------------------------------------
	// Initialization routines for building up the UI ("views")
	//--------------------------------------------------------------------
public:
	BOOL 				getIsViewsInitialized() const { return mViewsInitialized; }
	const LLUUID&		getStartFolderID() const { return mStartFolderID; }
	const std::string&  getStartFolderString() { return mStartFolderString; }
protected:
	// Builds the UI.  Call this once the inventory is usable.
	void 				initializeViews();
	void rebuildViewsFor(const LLUUID& id); // Given the id and the parent, build all of the folder views.
	virtual void buildNewViews(const LLUUID& id);
private:
	BOOL				mBuildDefaultHierarchy; // default inventory hierarchy should be created in postBuild()
	BOOL				mViewsInitialized; // Views have been generated
	// UUID of category from which hierarchy should be built.  Set with the 
	// "start_folder" xml property.  Default is LLUUID::null that means total Inventory hierarchy. 
	std::string         mStartFolderString;
	LLUUID				mStartFolderID;
};

#endif // LL_LLINVENTORYPANEL_H
