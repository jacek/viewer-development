/* 
 * @file llinventorypanel.cpp
 * @brief Implementation of the inventory panel and associated stuff.
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

#include "llviewerprecompiledheaders.h"
#include "llinventorypanel.h"

#include <utility> // for std::pair<>

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llavataractions.h"
#include "llfloaterinventory.h"
#include "llfloaterreg.h"
#include "llimfloater.h"
#include "llimview.h"
#include "llinventorybridge.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llsidepanelinventory.h"
#include "llsidetray.h"
#include "llscrollcontainer.h"
#include "llviewerattachmenu.h"
#include "llviewerfoldertype.h"
#include "llvoavatarself.h"

static LLDefaultChildRegistry::Register<LLInventoryPanel> r("inventory_panel");

const std::string LLInventoryPanel::DEFAULT_SORT_ORDER = std::string("InventorySortOrder");
const std::string LLInventoryPanel::RECENTITEMS_SORT_ORDER = std::string("RecentItemsSortOrder");
const std::string LLInventoryPanel::INHERIT_SORT_ORDER = std::string("");
static const LLInventoryFVBridgeBuilder INVENTORY_BRIDGE_BUILDER;


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanelObserver
//
// Bridge to support knowing when the inventory has changed.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInventoryPanelObserver : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip) : mIP(ip) {}
	virtual ~LLInventoryPanelObserver() {}
	virtual void changed(U32 mask) 
	{
		mIP->modelChanged(mask);
	}
protected:
	LLInventoryPanel* mIP;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvPanelComplObserver
//
// Calls specified callback when all specified items become complete.
//
// Usage:
// observer = new LLInvPanelComplObserver(boost::bind(onComplete));
// inventory->addObserver(observer);
// observer->reset(); // (optional)
// observer->watchItem(incomplete_item1_id);
// observer->watchItem(incomplete_item2_id);
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLInvPanelComplObserver : public LLInventoryCompletionObserver
{
public:
	typedef boost::function<void()> callback_t;

	LLInvPanelComplObserver(callback_t cb)
	:	mCallback(cb)
	{
	}

	void reset();

private:
	/*virtual*/ void done();

	/// Called when all the items are complete.
	callback_t	mCallback;
};

void LLInvPanelComplObserver::reset()
{
	mIncomplete.clear();
	mComplete.clear();
}

void LLInvPanelComplObserver::done()
{
	mCallback();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInventoryPanel
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LLInventoryPanel::LLInventoryPanel(const LLInventoryPanel::Params& p) :	
	LLPanel(p),
	mInventoryObserver(NULL),
	mCompletionObserver(NULL),
	mFolderRoot(NULL),
	mScroller(NULL),
	mSortOrderSetting(p.sort_order_setting),
	mInventory(p.inventory),
	mAllowMultiSelect(p.allow_multi_select),
	mShowItemLinkOverlays(p.show_item_link_overlays),
	mViewsInitialized(false),
	mStartFolderString(p.start_folder),	
	mBuildDefaultHierarchy(true),
	mInvFVBridgeBuilder(NULL)
{
	mInvFVBridgeBuilder = &INVENTORY_BRIDGE_BUILDER;

	// contex menu callbacks
	mCommitCallbackRegistrar.add("Inventory.DoToSelected", boost::bind(&LLInventoryPanel::doToSelected, this, _2));
	mCommitCallbackRegistrar.add("Inventory.EmptyTrash", boost::bind(&LLInventoryModel::emptyFolderType, &gInventory, "ConfirmEmptyTrash", LLFolderType::FT_TRASH));
	mCommitCallbackRegistrar.add("Inventory.EmptyLostAndFound", boost::bind(&LLInventoryModel::emptyFolderType, &gInventory, "ConfirmEmptyLostAndFound", LLFolderType::FT_LOST_AND_FOUND));
	mCommitCallbackRegistrar.add("Inventory.DoCreate", boost::bind(&LLInventoryPanel::doCreate, this, _2));
	mCommitCallbackRegistrar.add("Inventory.AttachObject", boost::bind(&LLInventoryPanel::attachObject, this, _2));
	mCommitCallbackRegistrar.add("Inventory.BeginIMSession", boost::bind(&LLInventoryPanel::beginIMSession, this));
	mCommitCallbackRegistrar.add("Inventory.Share",  boost::bind(&LLAvatarActions::shareWithAvatars));
	
	if (mStartFolderString != "")
	{
		mBuildDefaultHierarchy = false;
	}
}

void LLInventoryPanel::initFromParams(const LLInventoryPanel::Params& params)
{
	LLMemType mt(LLMemType::MTYPE_INVENTORY_POST_BUILD);

	mCommitCallbackRegistrar.pushScope(); // registered as a widget; need to push callback scope ourselves
	
	// Create root folder
	{
		LLRect folder_rect(0,
						   0,
						   getRect().getWidth(),
						   0);
		LLFolderView::Params p;
		p.name = getName();
		p.title = getLabel();
		p.rect = folder_rect;
		p.parent_panel = this;
		p.tool_tip = p.name;
		p.use_label_suffix = params.use_label_suffix;
		mFolderRoot = LLUICtrlFactory::create<LLFolderView>(p);
		mFolderRoot->setAllowMultiSelect(mAllowMultiSelect);
	}

	mCommitCallbackRegistrar.popScope();
	
	mFolderRoot->setCallbackRegistrar(&mCommitCallbackRegistrar);
	
	// Scroller
	{
		LLRect scroller_view_rect = getRect();
		scroller_view_rect.translate(-scroller_view_rect.mLeft, -scroller_view_rect.mBottom);
		LLScrollContainer::Params p;
		p.name("Inventory Scroller");
		p.rect(scroller_view_rect);
		p.follows.flags(FOLLOWS_ALL);
		p.reserve_scroll_corner(true);
		p.tab_stop(true);
		mScroller = LLUICtrlFactory::create<LLScrollContainer>(p);
		addChild(mScroller);
		mScroller->addChild(mFolderRoot);
		mFolderRoot->setScrollContainer(mScroller);
		mFolderRoot->addChild(mFolderRoot->mStatusTextBox);
	}

	// Set up the callbacks from the inventory we're viewing, and then build everything.
	mInventoryObserver = new LLInventoryPanelObserver(this);
	mInventory->addObserver(mInventoryObserver);

	mCompletionObserver = new LLInvPanelComplObserver(boost::bind(&LLInventoryPanel::onItemsCompletion, this));
	mInventory->addObserver(mCompletionObserver);

	// Build view of inventory if we need default full hierarchy and inventory ready,
	// otherwise wait for idle callback.
	if (mBuildDefaultHierarchy && mInventory->isInventoryUsable() && !mViewsInitialized)
	{
		initializeViews();
	}
	gIdleCallbacks.addFunction(onIdle, (void*)this);

	if (mSortOrderSetting != INHERIT_SORT_ORDER)
	{
		setSortOrder(gSavedSettings.getU32(mSortOrderSetting));
	}
	else
	{
		setSortOrder(gSavedSettings.getU32(DEFAULT_SORT_ORDER));
	}
	mFolderRoot->setSortOrder(getFilter()->getSortOrder());

	// Initialize base class params.
	LLPanel::initFromParams(params);
}

LLInventoryPanel::~LLInventoryPanel()
{
	if (mFolderRoot)
	{
		U32 sort_order = mFolderRoot->getSortOrder();
		if (mSortOrderSetting != INHERIT_SORT_ORDER)
		{
			gSavedSettings.setU32(mSortOrderSetting, sort_order);
		}
	}

	gIdleCallbacks.deleteFunction(onIdle, this);

	// LLView destructor will take care of the sub-views.
	mInventory->removeObserver(mInventoryObserver);
	mInventory->removeObserver(mCompletionObserver);
	delete mInventoryObserver;
	delete mCompletionObserver;

	mScroller = NULL;
}

void LLInventoryPanel::draw()
{
	// Select the desired item (in case it wasn't loaded when the selection was requested)
	mFolderRoot->updateSelection();
	LLPanel::draw();
}

LLInventoryFilter* LLInventoryPanel::getFilter()
{
	if (mFolderRoot) 
	{
		return mFolderRoot->getFilter();
	}
	return NULL;
}

void LLInventoryPanel::setFilterTypes(U64 types, LLInventoryFilter::EFilterType filter_type)
{
	if (filter_type == LLInventoryFilter::FILTERTYPE_OBJECT)
		getFilter()->setFilterObjectTypes(types);
	if (filter_type == LLInventoryFilter::FILTERTYPE_CATEGORY)
		getFilter()->setFilterCategoryTypes(types);
}

void LLInventoryPanel::setFilterPermMask(PermissionMask filter_perm_mask)
{
	getFilter()->setFilterPermissions(filter_perm_mask);
}

void LLInventoryPanel::setFilterWearableTypes(U64 types)
{
	getFilter()->setFilterWearableTypes(types);
}

void LLInventoryPanel::setFilterSubString(const std::string& string)
{
	getFilter()->setFilterSubString(string);
}

void LLInventoryPanel::setSortOrder(U32 order)
{
	getFilter()->setSortOrder(order);
	if (getFilter()->isModified())
	{
		mFolderRoot->setSortOrder(order);
		// try to keep selection onscreen, even if it wasn't to start with
		mFolderRoot->scrollToShowSelection();
	}
}

void LLInventoryPanel::setSinceLogoff(BOOL sl)
{
	getFilter()->setDateRangeLastLogoff(sl);
}

void LLInventoryPanel::setHoursAgo(U32 hours)
{
	getFilter()->setHoursAgo(hours);
}

void LLInventoryPanel::setFilterLinks(U64 filter_links)
{
	getFilter()->setFilterLinks(filter_links);
}

void LLInventoryPanel::setShowFolderState(LLInventoryFilter::EFolderShow show)
{
	getFilter()->setShowFolderState(show);
}

LLInventoryFilter::EFolderShow LLInventoryPanel::getShowFolderState()
{
	return getFilter()->getShowFolderState();
}

void LLInventoryPanel::modelChanged(U32 mask)
{
	static LLFastTimer::DeclareTimer FTM_REFRESH("Inventory Refresh");
	LLFastTimer t2(FTM_REFRESH);

	bool handled = false;

	if (!mViewsInitialized) return;
	
	const LLInventoryModel* model = getModel();
	if (!model) return;

	const LLInventoryModel::changed_items_t& changed_items = model->getChangedIDs();
	if (changed_items.empty()) return;

	for (LLInventoryModel::changed_items_t::const_iterator items_iter = changed_items.begin();
		 items_iter != changed_items.end();
		 ++items_iter)
	{
		const LLUUID& item_id = (*items_iter);
		const LLInventoryObject* model_item = model->getObject(item_id);
		LLFolderViewItem* view_item = mFolderRoot->getItemByID(item_id);

		// LLFolderViewFolder is derived from LLFolderViewItem so dynamic_cast from item
		// to folder is the fast way to get a folder without searching through folders tree.
		LLFolderViewFolder* view_folder = dynamic_cast<LLFolderViewFolder*>(view_item);

		//////////////////////////////
		// LABEL Operation
		// Empty out the display name for relabel.
		if (mask & LLInventoryObserver::LABEL)
		{
			handled = true;
			if (view_item)
			{
				// Request refresh on this item (also flags for filtering)
				LLInvFVBridge* bridge = (LLInvFVBridge*)view_item->getListener();
				if(bridge)
				{	// Clear the display name first, so it gets properly re-built during refresh()
					bridge->clearDisplayName();

					view_item->refresh();
				}
			}
		}

		//////////////////////////////
		// REBUILD Operation
		// Destroy and regenerate the UI.
		if (mask & LLInventoryObserver::REBUILD)
		{
			handled = true;
			if (model_item && view_item)
			{
				view_item->destroyView();
			}
			buildNewViews(item_id);
		}

		//////////////////////////////
		// INTERNAL Operation
		// This could be anything.  For now, just refresh the item.
		if (mask & LLInventoryObserver::INTERNAL)
		{
			if (view_item)
			{
				view_item->refresh();
			}
		}

		//////////////////////////////
		// SORT Operation
		// Sort the folder.
		if (mask & LLInventoryObserver::SORT)
		{
			if (view_folder)
			{
				view_folder->requestSort();
			}
		}	

		// We don't typically care which of these masks the item is actually flagged with, since the masks
		// may not be accurate (e.g. in the main inventory panel, I move an item from My Inventory into
		// Landmarks; this is a STRUCTURE change for that panel but is an ADD change for the Landmarks
		// panel).  What's relevant is that the item and UI are probably out of sync and thus need to be
		// resynchronized.
		if (mask & (LLInventoryObserver::STRUCTURE |
					LLInventoryObserver::ADD |
					LLInventoryObserver::REMOVE))
		{
			handled = true;

			//////////////////////////////
			// ADD Operation
			// Item exists in memory but a UI element hasn't been created for it.
			if (model_item && !view_item)
			{
				// Add the UI element for this item.
				buildNewViews(item_id);
				// Select any newly created object that has the auto rename at top of folder root set.
				if(mFolderRoot->getRoot()->needsAutoRename())
				{
					setSelection(item_id, FALSE);
				}
			}

			//////////////////////////////
			// STRUCTURE Operation
			// This item already exists in both memory and UI.  It was probably reparented.
			if (model_item && view_item)
			{
				// Don't process the item if it's hanging from the root, since its
				// model_item's parent will be NULL.
				if (view_item->getRoot() != view_item->getParent())
				{
					LLFolderViewFolder* new_parent = (LLFolderViewFolder*)mFolderRoot->getItemByID(model_item->getParentUUID());
					// Item has been moved.
					if (view_item->getParentFolder() != new_parent)
					{
						if (new_parent != NULL)
						{
							// Item is to be moved and we found its new parent in the panel's directory, so move the item's UI.
							view_item->getParentFolder()->extractItem(view_item);
							view_item->addToFolder(new_parent, mFolderRoot);
						}
						else 
						{
							// Item is to be moved outside the panel's directory (e.g. moved to trash for a panel that 
							// doesn't include trash).  Just remove the item's UI.
							view_item->destroyView();
						}
					}
				}
			}
			
			//////////////////////////////
			// REMOVE Operation
			// This item has been removed from memory, but its associated UI element still exists.
			if (!model_item && view_item)
			{
				// Remove the item's UI.
				view_item->destroyView();
			}
		}
	}
}

// static
void LLInventoryPanel::onIdle(void *userdata)
{
	if (!gInventory.isInventoryUsable())
		return;

	LLInventoryPanel *self = (LLInventoryPanel*)userdata;
	// Inventory just initialized, do complete build
	if (!self->mViewsInitialized)
	{
		self->initializeViews();
	}
	if (self->mViewsInitialized)
	{
		gIdleCallbacks.deleteFunction(onIdle, (void*)self);
	}
}

void LLInventoryPanel::initializeViews()
{
	if (!gInventory.isInventoryUsable()) return;

	// Determine the root folder in case specified, and
	// build the views starting with that folder.
	const LLFolderType::EType preferred_type = LLViewerFolderType::lookupTypeFromNewCategoryName(mStartFolderString);

	if ("LIBRARY" == mStartFolderString)
	{
		mStartFolderID = gInventory.getLibraryRootFolderID();
	}
	else
	{
		mStartFolderID = (preferred_type != LLFolderType::FT_NONE ? gInventory.findCategoryUUIDForType(preferred_type) : LLUUID::null);
	}
	rebuildViewsFor(mStartFolderID);

	mViewsInitialized = true;
	
	openStartFolderOrMyInventory();
	
	// Special case for new user login
	if (gAgent.isFirstLogin())
	{
		// Auto open the user's library
		LLFolderViewFolder* lib_folder = mFolderRoot->getFolderByID(gInventory.getLibraryRootFolderID());
		if (lib_folder)
		{
			lib_folder->setOpen(TRUE);
		}
		
		// Auto close the user's my inventory folder
		LLFolderViewFolder* my_inv_folder = mFolderRoot->getFolderByID(gInventory.getRootFolderID());
		if (my_inv_folder)
		{
			my_inv_folder->setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_DOWN);
		}
	}
}

void LLInventoryPanel::rebuildViewsFor(const LLUUID& id)
{
	// Destroy the old view for this ID so we can rebuild it.
	LLFolderViewItem* old_view = mFolderRoot->getItemByID(id);
	if (old_view && id.notNull())
	{
		old_view->destroyView();
	}

	buildNewViews(id);
}

void LLInventoryPanel::buildNewViews(const LLUUID& id)
{
	LLMemType mt(LLMemType::MTYPE_INVENTORY_BUILD_NEW_VIEWS);
	LLFolderViewItem* itemp = NULL;
	LLInventoryObject* objectp = gInventory.getObject(id);
	if (objectp)
	{
		const LLUUID &parent_id = objectp->getParentUUID();
		LLFolderViewFolder* parent_folder = (LLFolderViewFolder*)mFolderRoot->getItemByID(parent_id);
		if (id == mStartFolderID)
		{
			parent_folder = mFolderRoot;
		}
		else if ((mStartFolderID != LLUUID::null) && (!gInventory.isObjectDescendentOf(id, mStartFolderID)))
		{
			// This item exists outside the inventory's hierarchy, so don't add it.
			return;
		}
		
		if (objectp->getType() <= LLAssetType::AT_NONE ||
			objectp->getType() >= LLAssetType::AT_COUNT)
		{
			llwarns << "LLInventoryPanel::buildNewViews called with invalid objectp->mType : "
					<< ((S32) objectp->getType()) << " name " << objectp->getName() << " UUID " << objectp->getUUID() 
					<< llendl;
			return;
		}
		
		if ((objectp->getType() == LLAssetType::AT_CATEGORY) &&
			(objectp->getActualType() != LLAssetType::AT_LINK_FOLDER))
		{
			LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(objectp->getType(),
																			objectp->getType(),
																			LLInventoryType::IT_CATEGORY,
																			this,
																			mFolderRoot,
																			objectp->getUUID());
			if (new_listener)
			{
				LLFolderViewFolder::Params params;
				params.name = new_listener->getDisplayName();
				params.icon = new_listener->getIcon();
				params.icon_open = new_listener->getOpenIcon();
				if (mShowItemLinkOverlays) // if false, then links show up just like normal items
				{
					params.icon_overlay = LLUI::getUIImage("Inv_Link");
				}
				params.root = mFolderRoot;
				params.listener = new_listener;
				params.tool_tip = params.name;
				LLFolderViewFolder* folderp = LLUICtrlFactory::create<LLFolderViewFolder>(params);
				folderp->setItemSortOrder(mFolderRoot->getSortOrder());
				itemp = folderp;

				// Hide the root folder, so we can show the contents of a folder flat
				// but still have the parent folder present for listener-related operations.
				if (id == mStartFolderID)
				{
					folderp->setHidden(TRUE);
				}
				const LLViewerInventoryCategory *cat = dynamic_cast<LLViewerInventoryCategory *>(objectp);
				if (cat && getIsHiddenFolderType(cat->getPreferredType()))
				{
					folderp->setHidden(TRUE);
				}
			}
		}
		else 
		{
			// Build new view for item.
			LLInventoryItem* item = (LLInventoryItem*)objectp;
			LLInvFVBridge* new_listener = mInvFVBridgeBuilder->createBridge(item->getType(),
																			item->getActualType(),
																			item->getInventoryType(),
																			this,
																			mFolderRoot,
																			item->getUUID(),
																			item->getFlags());

			if (new_listener)
			{
				LLFolderViewItem::Params params;
				params.name = new_listener->getDisplayName();
				params.icon = new_listener->getIcon();
				params.icon_open = new_listener->getOpenIcon();
				if (mShowItemLinkOverlays) // if false, then links show up just like normal items
				{
					params.icon_overlay = LLUI::getUIImage("Inv_Link");
				}
				params.creation_date = new_listener->getCreationDate();
				params.root = mFolderRoot;
				params.listener = new_listener;
				params.rect = LLRect (0, 0, 0, 0);
				params.tool_tip = params.name;
				itemp = LLUICtrlFactory::create<LLFolderViewItem> (params);
			}
		}

		if (itemp)
		{
			itemp->addToFolder(parent_folder, mFolderRoot);

			// Don't add children of hidden folders unless this is the panel's root folder.
			if (itemp->getHidden() && (id != mStartFolderID))
			{
				return;
			}
		}
	}

	// If this is a folder, add the children of the folder and recursively add any 
	// child folders.
	if ((id == mStartFolderID) ||
		(objectp && objectp->getType() == LLAssetType::AT_CATEGORY))
	{
		LLViewerInventoryCategory::cat_array_t* categories;
		LLViewerInventoryItem::item_array_t* items;
		mInventory->lockDirectDescendentArrays(id, categories, items);
		
		if(categories)
		{
			for (LLViewerInventoryCategory::cat_array_t::const_iterator cat_iter = categories->begin();
				 cat_iter != categories->end();
				 ++cat_iter)
			{
				const LLViewerInventoryCategory* cat = (*cat_iter);
				buildNewViews(cat->getUUID());
			}
		}
		
		if(items)
		{
			for (LLViewerInventoryItem::item_array_t::const_iterator item_iter = items->begin();
				 item_iter != items->end();
				 ++item_iter)
			{
				const LLViewerInventoryItem* item = (*item_iter);
				buildNewViews(item->getUUID());
			}
		}
		mInventory->unlockDirectDescendentArrays(id);
	}
}

// bit of a hack to make sure the inventory is open.
void LLInventoryPanel::openStartFolderOrMyInventory()
{
	if (mStartFolderString != "")
	{
		mFolderRoot->openFolder(mStartFolderString);
	}
	else
	{
		// Find My Inventory folder and open it up by name
		for (LLView *child = mFolderRoot->getFirstChild(); child; child = mFolderRoot->findNextSibling(child))
		{
			LLFolderViewFolder *fchild = dynamic_cast<LLFolderViewFolder*>(child);
			if (fchild && fchild->getListener() &&
				(fchild->getListener()->getUUID() == gInventory.getRootFolderID()))
			{
				const std::string& child_name = child->getName();
				mFolderRoot->openFolder(child_name);
				break;
			}
		}
	}
}

void LLInventoryPanel::onItemsCompletion()
{
	if (mFolderRoot) mFolderRoot->updateMenu();
}

void LLInventoryPanel::openSelected()
{
	LLFolderViewItem* folder_item = mFolderRoot->getCurSelectedItem();
	if(!folder_item) return;
	LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getListener();
	if(!bridge) return;
	bridge->openItem();
}

BOOL LLInventoryPanel::handleHover(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLView::handleHover(x, y, mask);
	if(handled)
	{
		ECursorType cursor = getWindow()->getCursor();
		if (LLInventoryModelBackgroundFetch::instance().backgroundFetchActive() && cursor == UI_CURSOR_ARROW)
		{
			// replace arrow cursor with arrow and hourglass cursor
			getWindow()->setCursor(UI_CURSOR_WORKING);
		}
	}
	else
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
	}
	return TRUE;
}

BOOL LLInventoryPanel::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
{
	BOOL handled = LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);

	// If folder view is empty the (x, y) point won't be in its rect
	// so the handler must be called explicitly.
	// but only if was not handled before. See EXT-6746.
	if (!handled && !mFolderRoot->hasVisibleChildren())
	{
		handled = mFolderRoot->handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
	}

	if (handled)
	{
		mFolderRoot->setDragAndDropThisFrame();
	}

	return handled;
}

void LLInventoryPanel::onFocusLost()
{
	// inventory no longer handles cut/copy/paste/delete
	if (LLEditMenuHandler::gEditMenuHandler == mFolderRoot)
	{
		LLEditMenuHandler::gEditMenuHandler = NULL;
	}

	LLPanel::onFocusLost();
}

void LLInventoryPanel::onFocusReceived()
{
	// inventory now handles cut/copy/paste/delete
	LLEditMenuHandler::gEditMenuHandler = mFolderRoot;

	LLPanel::onFocusReceived();
}

void LLInventoryPanel::openAllFolders()
{
	mFolderRoot->setOpenArrangeRecursively(TRUE, LLFolderViewFolder::RECURSE_DOWN);
	mFolderRoot->arrangeAll();
}

void LLInventoryPanel::setSelection(const LLUUID& obj_id, BOOL take_keyboard_focus)
{
	// Don't select objects in COF (e.g. to prevent refocus when items are worn).
	const LLInventoryObject *obj = gInventory.getObject(obj_id);
	if (obj && obj->getParentUUID() == LLAppearanceMgr::instance().getCOF())
	{
		return;
	}
	mFolderRoot->setSelectionByID(obj_id, take_keyboard_focus);
}

void LLInventoryPanel::setSelectCallback(const LLFolderView::signal_t::slot_type& cb) 
{ 
	if (mFolderRoot) 
	{
		mFolderRoot->setSelectCallback(cb);
	}
}

void LLInventoryPanel::clearSelection()
{
	mFolderRoot->clearSelection();
}

void LLInventoryPanel::onSelectionChange(const std::deque<LLFolderViewItem*>& items, BOOL user_action)
{
	// Schedule updating the folder view context menu when all selected items become complete (STORM-373).
	mCompletionObserver->reset();
	for (std::deque<LLFolderViewItem*>::const_iterator it = items.begin(); it != items.end(); ++it)
	{
		LLUUID id = (*it)->getListener()->getUUID();
		LLViewerInventoryItem* inv_item = mInventory->getItem(id);

		if (inv_item && !inv_item->isFinished())
		{
			mCompletionObserver->watchItem(id);
		}
	}

	LLFolderView* fv = getRootFolder();
	if (fv->needsAutoRename()) // auto-selecting a new user-created asset and preparing to rename
	{
		fv->setNeedsAutoRename(FALSE);
		if (items.size()) // new asset is visible and selected
		{
			fv->startRenamingSelectedItem();
		}
	}
}

void LLInventoryPanel::doToSelected(const LLSD& userdata)
{
	mFolderRoot->doToSelected(&gInventory, userdata);
}

void LLInventoryPanel::doCreate(const LLSD& userdata)
{
	menu_create_inventory_item(mFolderRoot, LLFolderBridge::sSelf.get(), userdata);
}

bool LLInventoryPanel::beginIMSession()
{
	std::set<LLUUID> selected_items = mFolderRoot->getSelectionList();

	std::string name;
	static int session_num = 1;

	LLDynamicArray<LLUUID> members;
	EInstantMessage type = IM_SESSION_CONFERENCE_START;

	std::set<LLUUID>::const_iterator iter;
	for (iter = selected_items.begin(); iter != selected_items.end(); iter++)
	{

		LLUUID item = *iter;
		LLFolderViewItem* folder_item = mFolderRoot->getItemByID(item);
			
		if(folder_item) 
		{
			LLFolderViewEventListener* fve_listener = folder_item->getListener();
			if (fve_listener && (fve_listener->getInventoryType() == LLInventoryType::IT_CATEGORY))
			{

				LLFolderBridge* bridge = (LLFolderBridge*)folder_item->getListener();
				if(!bridge) return true;
				LLViewerInventoryCategory* cat = bridge->getCategory();
				if(!cat) return true;
				name = cat->getName();
				LLUniqueBuddyCollector is_buddy;
				LLInventoryModel::cat_array_t cat_array;
				LLInventoryModel::item_array_t item_array;
				gInventory.collectDescendentsIf(bridge->getUUID(),
												cat_array,
												item_array,
												LLInventoryModel::EXCLUDE_TRASH,
												is_buddy);
				S32 count = item_array.count();
				if(count > 0)
				{
					//*TODO by what to replace that?
					//LLFloaterReg::showInstance("communicate");

					// create the session
					LLAvatarTracker& at = LLAvatarTracker::instance();
					LLUUID id;
					for(S32 i = 0; i < count; ++i)
					{
						id = item_array.get(i)->getCreatorUUID();
						if(at.isBuddyOnline(id))
						{
							members.put(id);
						}
					}
				}
			}
			else
			{
				LLFolderViewItem* folder_item = mFolderRoot->getItemByID(item);
				if(!folder_item) return true;
				LLInvFVBridge* listenerp = (LLInvFVBridge*)folder_item->getListener();

				if (listenerp->getInventoryType() == LLInventoryType::IT_CALLINGCARD)
				{
					LLInventoryItem* inv_item = gInventory.getItem(listenerp->getUUID());

					if (inv_item)
					{
						LLAvatarTracker& at = LLAvatarTracker::instance();
						LLUUID id = inv_item->getCreatorUUID();

						if(at.isBuddyOnline(id))
						{
							members.put(id);
						}
					}
				} //if IT_CALLINGCARD
			} //if !IT_CATEGORY
		}
	} //for selected_items	

	// the session_id is randomly generated UUID which will be replaced later
	// with a server side generated number

	if (name.empty())
	{
		name = llformat("Session %d", session_num++);
	}

	LLUUID session_id = gIMMgr->addSession(name, type, members[0], members);
	if (session_id != LLUUID::null)
	{
		LLIMFloater::show(session_id);
	}
		
	return true;
}

bool LLInventoryPanel::attachObject(const LLSD& userdata)
{
	// Copy selected item UUIDs to a vector.
	std::set<LLUUID> selected_items = mFolderRoot->getSelectionList();
	uuid_vec_t items;
	for (std::set<LLUUID>::const_iterator set_iter = selected_items.begin(); 
		 set_iter != selected_items.end(); 
		 ++set_iter)
	{
		items.push_back(*set_iter);
	}

	// Attach selected items.
	LLViewerAttachMenu::attachObjects(items, userdata.asString());

	gFocusMgr.setKeyboardFocus(NULL);

	return true;
}

BOOL LLInventoryPanel::getSinceLogoff()
{
	return getFilter()->isSinceLogoff();
}

// DEBUG ONLY
// static 
void LLInventoryPanel::dumpSelectionInformation(void* user_data)
{
	LLInventoryPanel* iv = (LLInventoryPanel*)user_data;
	iv->mFolderRoot->dumpSelectionInformation();
}

BOOL is_inventorysp_active()
{
	if (!LLSideTray::getInstance()->isPanelActive("sidepanel_inventory")) return FALSE;
	LLSidepanelInventory *inventorySP = dynamic_cast<LLSidepanelInventory *>(LLSideTray::getInstance()->getPanel("sidepanel_inventory"));
	if (!inventorySP) return FALSE; 
	return inventorySP->isMainInventoryPanelActive();
}

// static
LLInventoryPanel* LLInventoryPanel::getActiveInventoryPanel(BOOL auto_open)
{
	S32 z_min = S32_MAX;
	LLInventoryPanel* res = NULL;
	LLFloater* active_inv_floaterp = NULL;

	// A. If the inventory side panel is open, use that preferably.
	if (is_inventorysp_active())
	{
		LLSidepanelInventory *inventorySP = dynamic_cast<LLSidepanelInventory *>(LLSideTray::getInstance()->getPanel("sidepanel_inventory"));
		if (inventorySP)
		{
			return inventorySP->getActivePanel();
		}
	}
	// or if it is in floater undocked from sidetray get it and remember z order of floater to later compare it
	// with other inventory floaters order.
	else if (!LLSideTray::getInstance()->isTabAttached("sidebar_inventory"))
	{
		LLSidepanelInventory *inventorySP =
			dynamic_cast<LLSidepanelInventory *>(LLSideTray::getInstance()->getPanel("sidepanel_inventory"));
		LLFloater* inv_floater = LLFloaterReg::findInstance("side_bar_tab", LLSD("sidebar_inventory"));
		if (inventorySP && inv_floater)
		{
			res = inventorySP->getActivePanel();
			z_min = gFloaterView->getZOrder(inv_floater);
			active_inv_floaterp = inv_floater;
		}
		else
		{
			llwarns << "Inventory tab is detached from sidetray, but  either panel or floater were not found!" << llendl;
		}
	}
	
	// B. Iterate through the inventory floaters and return whichever is on top.
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("inventory");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin(); iter != inst_list.end(); ++iter)
	{
		LLFloaterInventory* iv = dynamic_cast<LLFloaterInventory*>(*iter);
		if (iv && iv->getVisible())
		{
			S32 z_order = gFloaterView->getZOrder(iv);
			if (z_order < z_min)
			{
				res = iv->getPanel();
				z_min = z_order;
				active_inv_floaterp = iv;
			}
		}
	}

	if (res)
	{
		// Make sure the floater is not minimized (STORM-438).
		if (active_inv_floaterp && active_inv_floaterp->isMinimized())
			active_inv_floaterp->setMinimized(FALSE);

		return res;
	}
		
	// C. If no panels are open and we don't want to force open a panel, then just abort out.
	if (!auto_open) return NULL;
	
	// D. Open the inventory side panel and use that.
    LLSD key;
	LLSidepanelInventory *sidepanel_inventory =
		dynamic_cast<LLSidepanelInventory *>(LLSideTray::getInstance()->showPanel("sidepanel_inventory", key));
	if (sidepanel_inventory)
	{
		return sidepanel_inventory->getActivePanel();
	}

	return NULL;
}

void LLInventoryPanel::addHideFolderType(LLFolderType::EType folder_type)
{
	if (!getIsHiddenFolderType(folder_type))
	{
		mHiddenFolderTypes.push_back(folder_type);
	}
}

BOOL LLInventoryPanel::getIsHiddenFolderType(LLFolderType::EType folder_type) const
{
	return (std::find(mHiddenFolderTypes.begin(), mHiddenFolderTypes.end(), folder_type) != mHiddenFolderTypes.end());
}


/************************************************************************/
/* Recent Inventory Panel related class                                 */
/************************************************************************/
class LLInventoryRecentItemsPanel;
static LLDefaultChildRegistry::Register<LLInventoryRecentItemsPanel> t_recent_inventory_panel("recent_inventory_panel");

static const LLRecentInventoryBridgeBuilder RECENT_ITEMS_BUILDER;
class LLInventoryRecentItemsPanel : public LLInventoryPanel
{
public:
	struct Params :	public LLInitParam::Block<Params, LLInventoryPanel::Params>
	{};

protected:
	LLInventoryRecentItemsPanel (const Params&);
	friend class LLUICtrlFactory;
};

LLInventoryRecentItemsPanel::LLInventoryRecentItemsPanel( const Params& params)
: LLInventoryPanel(params)
{
	// replace bridge builder to have necessary View bridges.
	mInvFVBridgeBuilder = &RECENT_ITEMS_BUILDER;
}

