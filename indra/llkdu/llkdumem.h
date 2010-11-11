/** 
 * @file llkdumem.h
 * @brief Helper class for kdu memory management
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

#ifndef LL_LLKDUMEM_H
#define LL_LLKDUMEM_H

// Support classes for reading and writing from memory buffers
// for KDU
#include "kdu_image.h"
#include "kdu/kdu_elementary.h"
#include "kdu/kdu_messaging.h"
#include "kdu/kdu_params.h"
#include "kdu/kdu_compressed.h"
#include "kdu/kdu_sample_processing.h"
#include "kdu_image_local.h"
#include "stdtypes.h"

class LLKDUMemSource: public kdu_compressed_source
{
public: // Member functions
	LLKDUMemSource(U8 *input_buffer, U32 size)
	{
		mData = input_buffer;
		mSize = size;
		mCurPos = 0;
	}

    ~LLKDUMemSource()
	{
	}

    int read(kdu_byte *buf, int num_bytes)
	{
		U32 num_out;
		num_out = num_bytes;

		if ((mSize - mCurPos) < (U32)num_bytes)
		{
			num_out = mSize -mCurPos;
		}
		memcpy(buf, mData + mCurPos, num_out);
		mCurPos += num_out;
		return num_out;
	}

	void reset()
	{
		mCurPos = 0;
	}
private: // Data
	U8 *mData;
	U32 mSize;
	U32 mCurPos;
};

class LLKDUMemTarget: public kdu_compressed_target
{
public: // Member functions
	LLKDUMemTarget(U8 *output_buffer, U32 &output_size, const U32 buffer_size)
	{
		mData = output_buffer;
		mSize = buffer_size;
		mCurPos = 0;
		mOutputSize = &output_size;
	}

    ~LLKDUMemTarget()
    {
	}

    bool write(const kdu_byte *buf, int num_bytes)
	{
		U32 num_out;
		num_out = num_bytes;

		if ((mSize - mCurPos) < (U32)num_bytes)
		{
			num_out = mSize - mCurPos;
			memcpy(mData + mCurPos, buf, num_out);
			return false;
		}
		memcpy(mData + mCurPos, buf, num_out);
		mCurPos += num_out;
		*mOutputSize = mCurPos;
		return true;
	}
private: // Data
	U8 *mData;
	U32 mSize;
	U32 mCurPos;
	U32 *mOutputSize;
};


class LLKDUMemIn : public kdu_image_in_base
{
public: // Member functions
    LLKDUMemIn(const U8 *data,
				const U32 size,
				const U16 rows,
				const U16 cols,
				U8 in_num_components,
				siz_params *siz);
    ~LLKDUMemIn();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);

	const U8 *mData;
private: // Data
    int first_comp_idx;
    int num_components;
    int rows, cols;
    int alignment_bytes; // Number of 0's at end of each line.
    int precision[3];
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int num_unread_rows;

	U32 mCurPos;
	U32 mDataSize;
};

class LLKDUMemOut : public kdu_image_out_base
{
public: // Member functions
    LLKDUMemOut(U8 *data, siz_params *siz, U8 in_num_components);
    LLKDUMemOut(siz_params *siz, U8 in_num_components);
    ~LLKDUMemOut();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);

	U8 *mData;
private: // Data
    int first_comp_idx;
    int num_components;
    int rows, cols;
    int precision[3];
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int num_unwritten_rows;

	U32 mCurPos;
	U32 mDataSize;
};

#endif
