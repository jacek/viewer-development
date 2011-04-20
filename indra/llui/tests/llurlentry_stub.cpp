/**
 * @file llurlentry_stub.cpp
 * @author Martin Reddy
 * @brief Stub implementations for LLUrlEntry unit test dependencies
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llstring.h"
#include "llfile.h"
#include "llavatarnamecache.h"
#include "llcachename.h"
#include "lluuid.h"
#include "message.h"

#include <string>

// Stub for LLAvatarNameCache
bool LLAvatarNameCache::get(const LLUUID& agent_id, LLAvatarName *av_name)
{
	return false;
}

void LLAvatarNameCache::get(const LLUUID& agent_id, callback_slot_t slot)
{
	return;
}

bool LLAvatarNameCache::useDisplayNames()
{
	return false;
}

//
// Stub implementation for LLCacheName
//
BOOL LLCacheName::getFullName(const LLUUID& id, std::string& fullname)
{
	fullname = "Lynx Linden";
	return TRUE;
}

BOOL LLCacheName::getGroupName(const LLUUID& id, std::string& group)
{
	group = "My Group";
	return TRUE;
}

boost::signals2::connection LLCacheName::get(const LLUUID& id, bool is_group, const LLCacheNameCallback& callback)
{
	return boost::signals2::connection();
}

boost::signals2::connection LLCacheName::getGroup(const LLUUID& id, const LLCacheNameCallback& callback)
{
	return boost::signals2::connection();
}

LLCacheName* gCacheName = NULL;

//
// Stub implementation for LLTrans
//
class LLTrans
{
public:
	static std::string getString(const std::string &xml_desc, const LLStringUtil::format_map_t& args);
};

std::string LLTrans::getString(const std::string &xml_desc, const LLStringUtil::format_map_t& args)
{
	return std::string();
}

//
// Stub implementation for LLStyle::Params::Params
//

LLStyle::Params::Params()
{
}

//
// Stub implementations for various LLInitParam classes
//

namespace LLInitParam
{
	BaseBlock::BaseBlock() {}
	BaseBlock::~BaseBlock() {}
	Param::Param(BaseBlock* enclosing_block)
	:	mIsProvided(false)
	{
		const U8* my_addr = reinterpret_cast<const U8*>(this);
		const U8* block_addr = reinterpret_cast<const U8*>(enclosing_block);
		mEnclosingBlockOffset = (U16)(my_addr - block_addr);
	}
	void BaseBlock::paramChanged(const Param& last_param, bool user_provided) {}

	void BaseBlock::addParam(BlockDescriptor& block_data, const ParamDescriptorPtr in_param, const char* char_name){}
	void BaseBlock::addSynonym(Param& param, const std::string& synonym) {}
	param_handle_t BaseBlock::getHandleFromParam(const Param* param) const {return 0;}
	
	void BaseBlock::init(BlockDescriptor& descriptor, BlockDescriptor& base_descriptor, size_t block_size)
	{
		descriptor.mCurrentBlockPtr = this;
	}
	bool BaseBlock::deserializeBlock(Parser& p, Parser::name_stack_range_t name_stack, S32 generation){ return true; }
	void BaseBlock::serializeBlock(Parser& parser, Parser::name_stack_t name_stack, const LLInitParam::BaseBlock* diff_block) const {}
	bool BaseBlock::inspectBlock(Parser& parser, Parser::name_stack_t name_stack, S32 min_value, S32 max_value) const { return true; }
	bool BaseBlock::merge(BlockDescriptor& block_data, const BaseBlock& other, bool overwrite) { return true; }
	bool BaseBlock::validateBlock(bool emit_errors) const { return true; }

	ParamValue<LLUIColor, TypeValues<LLUIColor> >::ParamValue(const LLUIColor& color)
	:	super_t(color)
	{}

	void ParamValue<LLUIColor, TypeValues<LLUIColor> >::updateValueFromBlock() 
	{}
	
	void ParamValue<LLUIColor, TypeValues<LLUIColor> >::updateBlockFromValue()
	{}

	bool ParamCompare<const LLFontGL*, false>::equals(const LLFontGL* a, const LLFontGL* b)
	{
		return false;
	}

	ParamValue<const LLFontGL*, TypeValues<const LLFontGL*> >::ParamValue(const LLFontGL* fontp)
	:	super_t(fontp)
	{}

	void ParamValue<const LLFontGL*, TypeValues<const LLFontGL*> >::updateValueFromBlock()
	{}
	
	void ParamValue<const LLFontGL*, TypeValues<const LLFontGL*> >::updateBlockFromValue()
	{}

	void TypeValues<LLFontGL::HAlign>::declareValues()
	{}

	void TypeValues<LLFontGL::VAlign>::declareValues()
	{}

	void TypeValues<LLFontGL::ShadowType>::declareValues()
	{}

	void ParamValue<LLUIImage*, TypeValues<LLUIImage*> >::updateValueFromBlock()
	{}
	
	void ParamValue<LLUIImage*, TypeValues<LLUIImage*> >::updateBlockFromValue()
	{}

	
	bool ParamCompare<LLUIImage*, false>::equals(
		LLUIImage* const &a,
		LLUIImage* const &b)
	{
		return false;
	}

	bool ParamCompare<LLUIColor, false>::equals(const LLUIColor &a, const LLUIColor &b)
	{
		return false;
	}

}

//static
LLFontGL* LLFontGL::getFontDefault()
{
	return NULL; 
}

char const* const _PREHASH_AgentData = (char *)"AgentData";
char const* const _PREHASH_AgentID = (char *)"AgentID";

LLHost LLHost::invalid(INVALID_PORT,INVALID_HOST_IP_ADDRESS);

LLMessageSystem* gMessageSystem = NULL;

//
// Stub implementation for LLMessageSystem
//
void LLMessageSystem::newMessage(const char *name) { }
void LLMessageSystem::nextBlockFast(const char *blockname) { }
void LLMessageSystem::nextBlock(const char *blockname) { }
void LLMessageSystem::addUUIDFast( const char *varname, const LLUUID& uuid) { }
void LLMessageSystem::addUUID( const char *varname, const LLUUID& uuid) { }
S32 LLMessageSystem::sendReliable(const LLHost &host) { return 0; }
