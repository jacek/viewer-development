/** 
 * @file llsdparam.cpp
 * @brief parameter block abstraction for creating complex objects and 
 * parsing construction parameters from xml and LLSD
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "linden_common.h"

// Project includes
#include "llsdparam.h"

//
// LLParamSDParser
//
LLParamSDParser::LLParamSDParser()
{
	using boost::bind;

	registerParserFuncs<S32>(readS32, bind(&LLParamSDParser::writeTypedValue<S32>, this, _1, _2));
	registerParserFuncs<U32>(readU32, bind(&LLParamSDParser::writeU32Param, this, _1, _2));
	registerParserFuncs<F32>(readF32, bind(&LLParamSDParser::writeTypedValue<F32>, this, _1, _2));
	registerParserFuncs<F64>(readF64, bind(&LLParamSDParser::writeTypedValue<F64>, this, _1, _2));
	registerParserFuncs<bool>(readBool, bind(&LLParamSDParser::writeTypedValue<F32>, this, _1, _2));
	registerParserFuncs<std::string>(readString, bind(&LLParamSDParser::writeTypedValue<std::string>, this, _1, _2));
	registerParserFuncs<LLUUID>(readUUID, bind(&LLParamSDParser::writeTypedValue<LLUUID>, this, _1, _2));
	registerParserFuncs<LLDate>(readDate, bind(&LLParamSDParser::writeTypedValue<LLDate>, this, _1, _2));
	registerParserFuncs<LLURI>(readURI, bind(&LLParamSDParser::writeTypedValue<LLURI>, this, _1, _2));
	registerParserFuncs<LLSD>(readSD, bind(&LLParamSDParser::writeTypedValue<LLSD>, this, _1, _2));
}

// special case handling of U32 due to ambiguous LLSD::assign overload
bool LLParamSDParser::writeU32Param(const void* val_ptr, const parser_t::name_stack_t& name_stack)
{
	if (!mWriteSD) return false;
	
	LLSD* sd_to_write = getSDWriteNode(name_stack);
	if (!sd_to_write) return false;

	sd_to_write->assign((S32)*((const U32*)val_ptr));
	return true;
}

void LLParamSDParser::readSD(const LLSD& sd, LLInitParam::BaseBlock& block, bool silent)
{
	mCurReadSD = NULL;
	mNameStack.clear();
	setParseSilently(silent);

	readSDValues(sd, block);
}

void LLParamSDParser::writeSD(LLSD& sd, const LLInitParam::BaseBlock& block)
{
	mWriteSD = &sd;
	block.serializeBlock(*this);
}

void LLParamSDParser::readSDValues(const LLSD& sd, LLInitParam::BaseBlock& block)
{
	if (sd.isMap())
	{
		for (LLSD::map_const_iterator it = sd.beginMap();
			it != sd.endMap();
			++it)
		{
			mNameStack.push_back(make_pair(it->first, newParseGeneration()));
			readSDValues(it->second, block);
			mNameStack.pop_back();
		}
	}
	else if (sd.isArray())
	{
		for (LLSD::array_const_iterator it = sd.beginArray();
			it != sd.endArray();
			++it)
		{
			mNameStack.back().second = newParseGeneration();
			readSDValues(*it, block);
		}
	}
	else
	{
		mCurReadSD = &sd;
		block.submitValue(mNameStack, *this);
	}
}

/*virtual*/ std::string LLParamSDParser::getCurrentElementName()
{
	std::string full_name = "sd";
	for (name_stack_t::iterator it = mNameStack.begin();	
		it != mNameStack.end();
		++it)
	{
		full_name += llformat("[%s]", it->first.c_str());
	}

	return full_name;
}

LLSD* LLParamSDParser::getSDWriteNode(const parser_t::name_stack_t& name_stack)
{
	//TODO: implement nested LLSD writing
	return mWriteSD;
}

bool LLParamSDParser::readS32(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((S32*)val_ptr) = self.mCurReadSD->asInteger();
    return true;
}

bool LLParamSDParser::readU32(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((U32*)val_ptr) = self.mCurReadSD->asInteger();
    return true;
}

bool LLParamSDParser::readF32(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((F32*)val_ptr) = self.mCurReadSD->asReal();
    return true;
}

bool LLParamSDParser::readF64(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((F64*)val_ptr) = self.mCurReadSD->asReal();
    return true;
}

bool LLParamSDParser::readBool(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

    *((bool*)val_ptr) = self.mCurReadSD->asBoolean();
    return true;
}

bool LLParamSDParser::readString(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((std::string*)val_ptr) = self.mCurReadSD->asString();
    return true;
}

bool LLParamSDParser::readUUID(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLUUID*)val_ptr) = self.mCurReadSD->asUUID();
    return true;
}

bool LLParamSDParser::readDate(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLDate*)val_ptr) = self.mCurReadSD->asDate();
    return true;
}

bool LLParamSDParser::readURI(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLURI*)val_ptr) = self.mCurReadSD->asURI();
    return true;
}

bool LLParamSDParser::readSD(Parser& parser, void* val_ptr)
{
	LLParamSDParser& self = static_cast<LLParamSDParser&>(parser);

	*((LLSD*)val_ptr) = *self.mCurReadSD;
    return true;
}
