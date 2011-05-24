/** 
 * @file llfoldertype.cpp
 * @brief Implementatino of LLFolderType functionality.
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

#include "linden_common.h"

#include "llfoldertype.h"
#include "lldictionary.h"
#include "llmemory.h"
#include "llsingleton.h"

///----------------------------------------------------------------------------
/// Class LLFolderType
///----------------------------------------------------------------------------
struct FolderEntry : public LLDictionaryEntry
{
	FolderEntry(const std::string &type_name, // 8 character limit!
				bool is_protected) // can the viewer change categories of this type?
		:
	LLDictionaryEntry(type_name),
	mIsProtected(is_protected)
	{
		llassert(type_name.length() <= 8);
	}

	const bool mIsProtected;
};

class LLFolderDictionary : public LLSingleton<LLFolderDictionary>,
						   public LLDictionary<LLFolderType::EType, FolderEntry>
{
public:
	LLFolderDictionary();
protected:
	virtual LLFolderType::EType notFound() const
	{
		return LLFolderType::FT_NONE;
	}
};

LLFolderDictionary::LLFolderDictionary()
{
	//       													    TYPE NAME	PROTECTED
	//      													   |-----------|---------|
	addEntry(LLFolderType::FT_TEXTURE, 				new FolderEntry("texture",	TRUE));
	addEntry(LLFolderType::FT_SOUND, 				new FolderEntry("sound",	TRUE));
	addEntry(LLFolderType::FT_CALLINGCARD, 			new FolderEntry("callcard",	TRUE));
	addEntry(LLFolderType::FT_LANDMARK, 			new FolderEntry("landmark",	TRUE));
	addEntry(LLFolderType::FT_CLOTHING, 			new FolderEntry("clothing",	TRUE));
	addEntry(LLFolderType::FT_OBJECT, 				new FolderEntry("object",	TRUE));
	addEntry(LLFolderType::FT_NOTECARD, 			new FolderEntry("notecard",	TRUE));
	addEntry(LLFolderType::FT_ROOT_INVENTORY, 		new FolderEntry("root_inv",	TRUE));
	addEntry(LLFolderType::FT_LSL_TEXT, 			new FolderEntry("lsltext",	TRUE));
	addEntry(LLFolderType::FT_BODYPART, 			new FolderEntry("bodypart",	TRUE));
	addEntry(LLFolderType::FT_TRASH, 				new FolderEntry("trash",	TRUE));
	addEntry(LLFolderType::FT_SNAPSHOT_CATEGORY, 	new FolderEntry("snapshot", TRUE));
	addEntry(LLFolderType::FT_LOST_AND_FOUND, 		new FolderEntry("lstndfnd",	TRUE));
	addEntry(LLFolderType::FT_ANIMATION, 			new FolderEntry("animatn",	TRUE));
	addEntry(LLFolderType::FT_GESTURE, 				new FolderEntry("gesture",	TRUE));
	addEntry(LLFolderType::FT_FAVORITE, 			new FolderEntry("favorite",	TRUE));
	
	for (S32 ensemble_num = S32(LLFolderType::FT_ENSEMBLE_START); ensemble_num <= S32(LLFolderType::FT_ENSEMBLE_END); ensemble_num++)
	{
		addEntry(LLFolderType::EType(ensemble_num), new FolderEntry("ensemble", FALSE)); 
	}

	addEntry(LLFolderType::FT_CURRENT_OUTFIT, 		new FolderEntry("current",	TRUE));
	addEntry(LLFolderType::FT_OUTFIT, 				new FolderEntry("outfit",	FALSE));
	addEntry(LLFolderType::FT_MY_OUTFITS, 			new FolderEntry("my_otfts",	TRUE));

	addEntry(LLFolderType::FT_MESH, 				new FolderEntry("mesh",	TRUE));

	addEntry(LLFolderType::FT_INBOX, 				new FolderEntry("inbox",	TRUE));
		 
	addEntry(LLFolderType::FT_NONE, 				new FolderEntry("-1",		FALSE));
};

// static
LLFolderType::EType LLFolderType::lookup(const std::string& name)
{
	return LLFolderDictionary::getInstance()->lookup(name);
}

// static
const std::string &LLFolderType::lookup(LLFolderType::EType folder_type)
{
	const FolderEntry *entry = LLFolderDictionary::getInstance()->lookup(folder_type);
	if (entry)
	{
		return entry->mName;
	}
	else
	{
		return badLookup();
	}
}

// static
// Only ensembles and plain folders aren't protected.  "Protected" means
// you can't change certain properties such as their type.
bool LLFolderType::lookupIsProtectedType(EType folder_type)
{
	const LLFolderDictionary *dict = LLFolderDictionary::getInstance();
	const FolderEntry *entry = dict->lookup(folder_type);
	if (entry)
	{
		return entry->mIsProtected;
	}
	return true;
}

// static
bool LLFolderType::lookupIsEnsembleType(EType folder_type)
{
	return (folder_type >= FT_ENSEMBLE_START &&
			folder_type <= FT_ENSEMBLE_END);
}

// static
LLAssetType::EType LLFolderType::folderTypeToAssetType(LLFolderType::EType folder_type)
{
	if (LLAssetType::lookup(LLAssetType::EType(folder_type)) == LLAssetType::badLookup())
	{
		llwarns << "Converting to unknown asset type " << folder_type << llendl;
	}
	return (LLAssetType::EType)folder_type;
}

// static
LLFolderType::EType LLFolderType::assetTypeToFolderType(LLAssetType::EType asset_type)
{
	if (LLFolderType::lookup(LLFolderType::EType(asset_type)) == LLFolderType::badLookup())
	{
		llwarns << "Converting to unknown folder type " << asset_type << llendl;
	}
	return (LLFolderType::EType)asset_type;
}

// static
const std::string &LLFolderType::badLookup()
{
	static const std::string sBadLookup = "llfoldertype_bad_lookup";
	return sBadLookup;
}
