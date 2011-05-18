/** 
 * @file llhudnametag.cpp
 * @brief Name tags for avatars
 * @author James Cook
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

#include "llviewerprecompiledheaders.h"

#include "llhudnametag.h"

#include "llrender.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "lldrawable.h"
#include "llfontgl.h"
#include "llglheaders.h"
#include "llhudrender.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewerobject.h"
#include "llvovolume.h"
#include "llviewerwindow.h"
#include "llstatusbar.h"
#include "llmenugl.h"
#include "pipeline.h"
#include <boost/tokenizer.hpp>


const F32 SPRING_STRENGTH = 0.7f;
const F32 RESTORATION_SPRING_TIME_CONSTANT = 0.1f;
const F32 HORIZONTAL_PADDING = 16.f;
const F32 VERTICAL_PADDING = 12.f;
const F32 LINE_PADDING = 3.f;			// aka "leading"
const F32 BUFFER_SIZE = 2.f;
const F32 MIN_EDGE_OVERLAP = 3.f;
const F32 HUD_TEXT_MAX_WIDTH = 190.f;
const F32 HUD_TEXT_MAX_WIDTH_NO_BUBBLE = 1000.f;
const F32 RESIZE_TIME = 0.f;
const S32 NUM_OVERLAP_ITERATIONS = 10;
const F32 NEIGHBOR_FORCE_FRACTION = 1.f;
const F32 POSITION_DAMPING_TC = 0.2f;
const F32 MAX_STABLE_CAMERA_VELOCITY = 0.1f;
const F32 LOD_0_SCREEN_COVERAGE = 0.15f;
const F32 LOD_1_SCREEN_COVERAGE = 0.30f;
const F32 LOD_2_SCREEN_COVERAGE = 0.40f;

std::set<LLPointer<LLHUDNameTag> > LLHUDNameTag::sTextObjects;
std::vector<LLPointer<LLHUDNameTag> > LLHUDNameTag::sVisibleTextObjects;
BOOL LLHUDNameTag::sDisplayText = TRUE ;

bool llhudnametag_further_away::operator()(const LLPointer<LLHUDNameTag>& lhs, const LLPointer<LLHUDNameTag>& rhs) const
{
	return lhs->getDistance() > rhs->getDistance();
}


LLHUDNameTag::LLHUDNameTag(const U8 type)
:	LLHUDObject(type),
	mDoFade(TRUE),
	mFadeDistance(8.f),
	mFadeRange(4.f),
	mLastDistance(0.f),
	mZCompare(TRUE),
	mVisibleOffScreen(FALSE),
	mOffscreen(FALSE),
	mColor(1.f, 1.f, 1.f, 1.f),
//	mScale(),
	mWidth(0.f),
	mHeight(0.f),
	mFontp(LLFontGL::getFontSansSerifSmall()),
	mBoldFontp(LLFontGL::getFontSansSerifBold()),
	mSoftScreenRect(),
	mPositionAgent(),
	mPositionOffset(),
	mMass(10.f),
	mMaxLines(10),
	mOffsetY(0),
	mRadius(0.1f),
	mTextSegments(),
	mLabelSegments(),
	mTextAlignment(ALIGN_TEXT_CENTER),
	mVertAlignment(ALIGN_VERT_CENTER),
	mLOD(0),
	mHidden(FALSE)
{
	LLPointer<LLHUDNameTag> ptr(this);
	sTextObjects.insert(ptr);
}

LLHUDNameTag::~LLHUDNameTag()
{
}


BOOL LLHUDNameTag::lineSegmentIntersect(const LLVector3& start, const LLVector3& end, LLVector3& intersection, BOOL debug_render)
{
	if (!mVisible || mHidden)
	{
		return FALSE;
	}

	// don't pick text that isn't bound to a viewerobject
	if (!mSourceObject || mSourceObject->mDrawable.isNull())
	{
		return FALSE;
	}
	
	F32 alpha_factor = 1.f;
	LLColor4 text_color = mColor;
	if (mDoFade)
	{
		if (mLastDistance > mFadeDistance)
		{
			alpha_factor = llmax(0.f, 1.f - (mLastDistance - mFadeDistance)/mFadeRange);
			text_color.mV[3] = text_color.mV[3]*alpha_factor;
		}
	}
	if (text_color.mV[3] < 0.01f)
	{
		return FALSE;
	}

	mOffsetY = lltrunc(mHeight * ((mVertAlignment == ALIGN_VERT_CENTER) ? 0.5f : 1.f));

	LLVector3 position = mPositionAgent;

	if (mSourceObject)
	{ //get intersection of eye through mPositionAgent to plane of source object
		//using this position keeps the camera from focusing on some seemingly random 
		//point several meters in front of the nametag
		const LLVector3& p = mSourceObject->getPositionAgent();
		const LLVector3& n = LLViewerCamera::getInstance()->getAtAxis();
		const LLVector3& eye = LLViewerCamera::getInstance()->getOrigin();

		LLVector3 ray = position-eye;
		ray.normalize();

		LLVector3 delta = p-position;
		F32 dist = delta*n;
		F32 dt =  dist/(ray*n);
		position += ray*dt;
	}

	// scale screen size of borders down
	//RN: for now, text on hud objects is never occluded

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;
	
	LLViewerCamera::getInstance()->getPixelVectors(position, y_pixel_vec, x_pixel_vec);

	LLVector3 width_vec = mWidth * x_pixel_vec;
	LLVector3 height_vec = mHeight * y_pixel_vec;
	
	LLCoordGL screen_pos;
	LLViewerCamera::getInstance()->projectPosAgentToScreen(position, screen_pos, FALSE);

	LLVector2 screen_offset;
	screen_offset = updateScreenPos(mPositionOffset);
	
	LLVector3 render_position = position  
			+ (x_pixel_vec * screen_offset.mV[VX])
			+ (y_pixel_vec * screen_offset.mV[VY]);


	//if (mUseBubble)
	{
		LLVector3 bg_pos = render_position
			+ (F32)mOffsetY * y_pixel_vec
			- (width_vec / 2.f)
			- (height_vec);
		//LLUI::translate(bg_pos.mV[VX], bg_pos.mV[VY], bg_pos.mV[VZ]);

		LLVector3 v[] = 
		{
			bg_pos,
			bg_pos + width_vec,
			bg_pos + width_vec + height_vec,
			bg_pos + height_vec,
		};

		if (debug_render)
		{
			gGL.begin(LLRender::LINE_STRIP);
			gGL.vertex3fv(v[0].mV);
			gGL.vertex3fv(v[1].mV);
			gGL.vertex3fv(v[2].mV);
			gGL.vertex3fv(v[3].mV);
			gGL.vertex3fv(v[0].mV);
			gGL.vertex3fv(v[2].mV);
			gGL.end();
		}

		LLVector3 dir = end-start;
		F32 a, b, t;

		if (LLTriangleRayIntersect(v[0], v[1], v[2], start, dir, a, b, t, FALSE) ||
			LLTriangleRayIntersect(v[2], v[3], v[0], start, dir, a, b, t, FALSE) )
		{
			if (t <= 1.f)
			{
				intersection = start + dir*t;
				return TRUE;
			}
		}
	}

	return FALSE;
}

void LLHUDNameTag::render()
{
	if (sDisplayText)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		renderText(FALSE);
	}
}

void LLHUDNameTag::renderForSelect()
{
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
	renderText(TRUE);
}

void LLHUDNameTag::renderText(BOOL for_select)
{
	if (!mVisible || mHidden)
	{
		return;
	}

	// don't pick text that isn't bound to a viewerobject
	if (for_select && 
		(!mSourceObject || mSourceObject->mDrawable.isNull()))
	{
		return;
	}
	
	if (for_select)
	{
		gGL.getTexUnit(0)->disable();
	}
	else
	{
		gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	}

	LLGLState gls_blend(GL_BLEND, for_select ? FALSE : TRUE);
	LLGLState gls_alpha(GL_ALPHA_TEST, for_select ? FALSE : TRUE);
	
	LLColor4 shadow_color(0.f, 0.f, 0.f, 1.f);
	F32 alpha_factor = 1.f;
	LLColor4 text_color = mColor;
	if (mDoFade)
	{
		if (mLastDistance > mFadeDistance)
		{
			alpha_factor = llmax(0.f, 1.f - (mLastDistance - mFadeDistance)/mFadeRange);
			text_color.mV[3] = text_color.mV[3]*alpha_factor;
		}
	}
	if (text_color.mV[3] < 0.01f)
	{
		return;
	}
	shadow_color.mV[3] = text_color.mV[3];

	mOffsetY = lltrunc(mHeight * ((mVertAlignment == ALIGN_VERT_CENTER) ? 0.5f : 1.f));

	// *TODO: cache this image
	LLUIImagePtr imagep = LLUI::getUIImage("Rounded_Rect");

	// *TODO: make this a per-text setting
	LLColor4 bg_color = LLUIColorTable::instance().getColor("NameTagBackground");
	bg_color.setAlpha(gSavedSettings.getF32("ChatBubbleOpacity") * alpha_factor);

	// maybe a no-op?
	//const S32 border_height = 16;
	//const S32 border_width = 16;
	const S32 border_height = 8;
	const S32 border_width = 8;

	// *TODO move this into helper function
	F32 border_scale = 1.f;

	if (border_height * 2 > mHeight)
	{
		border_scale = (F32)mHeight / ((F32)border_height * 2.f);
	}
	if (border_width * 2 > mWidth)
	{
		border_scale = llmin(border_scale, (F32)mWidth / ((F32)border_width * 2.f));
	}

	// scale screen size of borders down
	//RN: for now, text on hud objects is never occluded

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;
	
	LLViewerCamera::getInstance()->getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);

	LLVector2 border_scale_vec((F32)border_width / (F32)imagep->getTextureWidth(), (F32)border_height / (F32)imagep->getTextureHeight());
	LLVector3 width_vec = mWidth * x_pixel_vec;
	LLVector3 height_vec = mHeight * y_pixel_vec;
	LLVector3 scaled_border_width = (F32)llfloor(border_scale * (F32)border_width) * x_pixel_vec;
	LLVector3 scaled_border_height = (F32)llfloor(border_scale * (F32)border_height) * y_pixel_vec;

	mRadius = (width_vec + height_vec).magVec() * 0.5f;

	LLCoordGL screen_pos;
	LLViewerCamera::getInstance()->projectPosAgentToScreen(mPositionAgent, screen_pos, FALSE);

	LLVector2 screen_offset;
//	if (!mUseBubble)
//	{
//		screen_offset = mPositionOffset;
//	}
//	else
//	{
		screen_offset = updateScreenPos(mPositionOffset);
//	}

	LLVector3 render_position = mPositionAgent  
			+ (x_pixel_vec * screen_offset.mV[VX])
			+ (y_pixel_vec * screen_offset.mV[VY]);

//	if (mUseBubble)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		LLUI::pushMatrix();
		{
			LLVector3 bg_pos = render_position
				+ (F32)mOffsetY * y_pixel_vec
				- (width_vec / 2.f)
				- (height_vec);
			LLUI::translate(bg_pos.mV[VX], bg_pos.mV[VY], bg_pos.mV[VZ]);

			if (for_select)
			{
				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				S32 name = mSourceObject->mGLName;
				LLColor4U coloru((U8)(name >> 16), (U8)(name >> 8), (U8)name);
				gGL.color4ubv(coloru.mV);
				gl_segmented_rect_3d_tex(border_scale_vec, scaled_border_width, scaled_border_height, width_vec, height_vec);
				LLUI::popMatrix();
				return;
			}
			else
			{
				gGL.getTexUnit(0)->bind(imagep->getImage());
				
				gGL.color4fv(bg_color.mV);
				gl_segmented_rect_3d_tex(border_scale_vec, scaled_border_width, scaled_border_height, width_vec, height_vec);
		
				if ( mLabelSegments.size())
				{
					LLUI::pushMatrix();
					{
						gGL.color4f(text_color.mV[VX], text_color.mV[VY], text_color.mV[VZ], gSavedSettings.getF32("ChatBubbleOpacity") * alpha_factor);
						LLVector3 label_height = (mFontp->getLineHeight() * mLabelSegments.size() + (VERTICAL_PADDING / 3.f)) * y_pixel_vec;
						LLVector3 label_offset = height_vec - label_height;
						LLUI::translate(label_offset.mV[VX], label_offset.mV[VY], label_offset.mV[VZ]);
						gl_segmented_rect_3d_tex_top(border_scale_vec, scaled_border_width, scaled_border_height, width_vec, label_height);
					}
					LLUI::popMatrix();
				}
			}

			BOOL outside_width = llabs(mPositionOffset.mV[VX]) > mWidth * 0.5f;
			BOOL outside_height = llabs(mPositionOffset.mV[VY] + (mVertAlignment == ALIGN_VERT_TOP ? mHeight * 0.5f : 0.f)) > mHeight * (mVertAlignment == ALIGN_VERT_TOP ? mHeight * 0.75f : 0.5f);

			// draw line segments pointing to parent object
			if (!mOffscreen && (outside_width || outside_height))
			{
				LLUI::pushMatrix();
				{
					gGL.color4fv(bg_color.mV);
					LLVector3 target_pos = -1.f * (mPositionOffset.mV[VX] * x_pixel_vec + mPositionOffset.mV[VY] * y_pixel_vec);
					target_pos += (width_vec / 2.f);
					target_pos += mVertAlignment == ALIGN_VERT_CENTER ? (height_vec * 0.5f) : LLVector3::zero;
					target_pos -= 3.f * x_pixel_vec;
					target_pos -= 6.f * y_pixel_vec;
					LLUI::translate(target_pos.mV[VX], target_pos.mV[VY], target_pos.mV[VZ]);
					gl_segmented_rect_3d_tex(border_scale_vec, 3.f * x_pixel_vec, 3.f * y_pixel_vec, 6.f * x_pixel_vec, 6.f * y_pixel_vec);	
				}
				LLUI::popMatrix();

				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				LLGLDepthTest gls_depth(mZCompare ? GL_TRUE : GL_FALSE, GL_FALSE);
				
				LLVector3 box_center_offset;
				box_center_offset = (width_vec * 0.5f) + (height_vec * 0.5f);
				LLUI::translate(box_center_offset.mV[VX], box_center_offset.mV[VY], box_center_offset.mV[VZ]);
				gGL.color4fv(bg_color.mV);
				LLUI::setLineWidth(2.0);
				gGL.begin(LLRender::LINES);
				{
					if (outside_width)
					{
						LLVector3 vert;
						// draw line in x then y
						if (mPositionOffset.mV[VX] < 0.f)
						{
							// start at right edge
							vert = width_vec * 0.5f;
							gGL.vertex3fv(vert.mV);
						}
						else
						{
							// start at left edge
							vert = width_vec * -0.5f;
							gGL.vertex3fv(vert.mV);
						}
						vert = -mPositionOffset.mV[VX] * x_pixel_vec;
						gGL.vertex3fv(vert.mV);
						gGL.vertex3fv(vert.mV);
						vert -= mPositionOffset.mV[VY] * y_pixel_vec;
						vert -= ((mVertAlignment == ALIGN_VERT_TOP) ? (height_vec * 0.5f) : LLVector3::zero);
						gGL.vertex3fv(vert.mV);
					}
					else
					{
						LLVector3 vert;
						// draw line in y then x
						if (mPositionOffset.mV[VY] < 0.f)
						{
							// start at top edge
							vert = (height_vec * 0.5f) - (mPositionOffset.mV[VX] * x_pixel_vec);
							gGL.vertex3fv(vert.mV);
						}
						else
						{
							// start at bottom edge
							vert = (height_vec * -0.5f)  - (mPositionOffset.mV[VX] * x_pixel_vec);
							gGL.vertex3fv(vert.mV);
						}
						vert = -mPositionOffset.mV[VY] * y_pixel_vec - mPositionOffset.mV[VX] * x_pixel_vec;
						vert -= ((mVertAlignment == ALIGN_VERT_TOP) ? (height_vec * 0.5f) : LLVector3::zero);
						gGL.vertex3fv(vert.mV);
					}
				}
				gGL.end();
				LLUI::setLineWidth(1.0);

			}
		}
		LLUI::popMatrix();
	}

	F32 y_offset = (F32)mOffsetY;
		
	// Render label
	{
		gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);

		for(std::vector<LLHUDTextSegment>::iterator segment_iter = mLabelSegments.begin();
			segment_iter != mLabelSegments.end(); ++segment_iter )
		{
			// Label segments use default font
			const LLFontGL* fontp = (segment_iter->mStyle == LLFontGL::BOLD) ? mBoldFontp : mFontp;
			y_offset -= fontp->getLineHeight();

			F32 x_offset;
			if (mTextAlignment == ALIGN_TEXT_CENTER)
			{
				x_offset = -0.5f*segment_iter->getWidth(fontp);
			}
			else // ALIGN_LEFT
			{
				x_offset = -0.5f * mWidth + (HORIZONTAL_PADDING / 2.f);
			}

			LLColor4 label_color(0.f, 0.f, 0.f, 1.f);
			label_color.mV[VALPHA] = alpha_factor;
			hud_render_text(segment_iter->getText(), render_position, *fontp, segment_iter->mStyle, LLFontGL::NO_SHADOW, x_offset, y_offset, label_color, FALSE);
		}
	}

	// Render text
	{
		// -1 mMaxLines means unlimited lines.
		S32 start_segment;
		S32 max_lines = getMaxLines();

		if (max_lines < 0) 
		{
			start_segment = 0;
		}
		else 
		{
			start_segment = llmax((S32)0, (S32)mTextSegments.size() - max_lines);
		}

		for (std::vector<LLHUDTextSegment>::iterator segment_iter = mTextSegments.begin() + start_segment;
			 segment_iter != mTextSegments.end(); ++segment_iter )
		{
			const LLFontGL* fontp = segment_iter->mFont;
			y_offset -= fontp->getLineHeight();
			y_offset -= LINE_PADDING;

			U8 style = segment_iter->mStyle;
			LLFontGL::ShadowType shadow = LLFontGL::DROP_SHADOW;
	
			F32 x_offset;
			if (mTextAlignment== ALIGN_TEXT_CENTER)
			{
				x_offset = -0.5f*segment_iter->getWidth(fontp);
			}
			else // ALIGN_LEFT
			{
				x_offset = -0.5f * mWidth + (HORIZONTAL_PADDING / 2.f);

				// *HACK
				x_offset += 1;
			}

			text_color = segment_iter->mColor;
			text_color.mV[VALPHA] *= alpha_factor;

			hud_render_text(segment_iter->getText(), render_position, *fontp, style, shadow, x_offset, y_offset, text_color, FALSE);
		}
	}
	/// Reset the default color to white.  The renderer expects this to be the default. 
	gGL.color4f(1.0f, 1.0f, 1.0f, 1.0f);
	if (for_select)
	{
		gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
	}
}

void LLHUDNameTag::setString(const std::string &text_utf8)
{
	mTextSegments.clear();
	addLine(text_utf8, mColor);
}

void LLHUDNameTag::clearString()
{
	mTextSegments.clear();
}


void LLHUDNameTag::addLine(const std::string &text_utf8,
						const LLColor4& color,
						const LLFontGL::StyleFlags style,
						const LLFontGL* font)
{
	LLWString wline = utf8str_to_wstring(text_utf8);
	if (!wline.empty())
	{
		// use default font for segment if custom font not specified
		if (!font)
		{
			font = mFontp;
		}
		typedef boost::tokenizer<boost::char_separator<llwchar>, LLWString::const_iterator, LLWString > tokenizer;
		LLWString seps(utf8str_to_wstring("\r\n"));
		boost::char_separator<llwchar> sep(seps.c_str());

		tokenizer tokens(wline, sep);
		tokenizer::iterator iter = tokens.begin();

		while (iter != tokens.end())
		{
			U32 line_length = 0;
			do	
			{
				F32 max_pixels = HUD_TEXT_MAX_WIDTH;
				S32 segment_length = font->maxDrawableChars(iter->substr(line_length).c_str(), max_pixels, wline.length(), LLFontGL::WORD_BOUNDARY_IF_POSSIBLE);
				LLHUDTextSegment segment(iter->substr(line_length, segment_length), style, color, font);
				mTextSegments.push_back(segment);
				line_length += segment_length;
			}
			while (line_length != iter->size());
			++iter;
		}
	}
}

void LLHUDNameTag::setLabel(const std::string &label_utf8)
{
	mLabelSegments.clear();
	addLabel(label_utf8);
}

void LLHUDNameTag::addLabel(const std::string& label_utf8)
{
	LLWString wstr = utf8string_to_wstring(label_utf8);
	if (!wstr.empty())
	{
		LLWString seps(utf8str_to_wstring("\r\n"));
		LLWString empty;

		typedef boost::tokenizer<boost::char_separator<llwchar>, LLWString::const_iterator, LLWString > tokenizer;
		boost::char_separator<llwchar> sep(seps.c_str(), empty.c_str(), boost::keep_empty_tokens);

		tokenizer tokens(wstr, sep);
		tokenizer::iterator iter = tokens.begin();

		while (iter != tokens.end())
		{
			U32 line_length = 0;
			do	
			{
				S32 segment_length = mFontp->maxDrawableChars(iter->substr(line_length).c_str(), 
					HUD_TEXT_MAX_WIDTH, wstr.length(), LLFontGL::WORD_BOUNDARY_IF_POSSIBLE);
				LLHUDTextSegment segment(iter->substr(line_length, segment_length), LLFontGL::NORMAL, mColor, mFontp);
				mLabelSegments.push_back(segment);
				line_length += segment_length;
			}
			while (line_length != iter->size());
			++iter;
		}
	}
}

void LLHUDNameTag::setZCompare(const BOOL zcompare)
{
	mZCompare = zcompare;
}

void LLHUDNameTag::setFont(const LLFontGL* font)
{
	mFontp = font;
}


void LLHUDNameTag::setColor(const LLColor4 &color)
{
	mColor = color;
	for (std::vector<LLHUDTextSegment>::iterator segment_iter = mTextSegments.begin();
		 segment_iter != mTextSegments.end(); ++segment_iter )
	{
		segment_iter->mColor = color;
	}
}

void LLHUDNameTag::setAlpha(F32 alpha)
{
	mColor.mV[VALPHA] = alpha;
	for (std::vector<LLHUDTextSegment>::iterator segment_iter = mTextSegments.begin();
		 segment_iter != mTextSegments.end(); ++segment_iter )
	{
		segment_iter->mColor.mV[VALPHA] = alpha;
	}
}


void LLHUDNameTag::setDoFade(const BOOL do_fade)
{
	mDoFade = do_fade;
}

void LLHUDNameTag::updateVisibility()
{
	if (mSourceObject)
	{
		mSourceObject->updateText();
	}
	
	mPositionAgent = gAgent.getPosAgentFromGlobal(mPositionGlobal);

	if (!mSourceObject)
	{
		//llwarns << "LLHUDNameTag::updateScreenPos -- mSourceObject is NULL!" << llendl;
		mVisible = TRUE;
		sVisibleTextObjects.push_back(LLPointer<LLHUDNameTag> (this));
		return;
	}

	// Not visible if parent object is dead
	if (mSourceObject->isDead())
	{
		mVisible = FALSE;
		return;
	}

	// push text towards camera by radius of object, but not past camera
	LLVector3 vec_from_camera = mPositionAgent - LLViewerCamera::getInstance()->getOrigin();
	LLVector3 dir_from_camera = vec_from_camera;
	dir_from_camera.normVec();

	if (dir_from_camera * LLViewerCamera::getInstance()->getAtAxis() <= 0.f)
	{ //text is behind camera, don't render
		mVisible = FALSE;
		return;
	}
		
	if (vec_from_camera * LLViewerCamera::getInstance()->getAtAxis() <= LLViewerCamera::getInstance()->getNear() + 0.1f + mSourceObject->getVObjRadius())
	{
		mPositionAgent = LLViewerCamera::getInstance()->getOrigin() + vec_from_camera * ((LLViewerCamera::getInstance()->getNear() + 0.1f) / (vec_from_camera * LLViewerCamera::getInstance()->getAtAxis()));
	}
	else
	{
		mPositionAgent -= dir_from_camera * mSourceObject->getVObjRadius();
	}

	mLastDistance = (mPositionAgent - LLViewerCamera::getInstance()->getOrigin()).magVec();

	if (mLOD >= 3 || !mTextSegments.size() || (mDoFade && (mLastDistance > mFadeDistance + mFadeRange)))
	{
		mVisible = FALSE;
		return;
	}

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;

	LLViewerCamera::getInstance()->getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);

	LLVector3 render_position = mPositionAgent + 			
			(x_pixel_vec * mPositionOffset.mV[VX]) +
			(y_pixel_vec * mPositionOffset.mV[VY]);

	mOffscreen = FALSE;
	if (!LLViewerCamera::getInstance()->sphereInFrustum(render_position, mRadius))
	{
		if (!mVisibleOffScreen)
		{
			mVisible = FALSE;
			return;
		}
		else
		{
			mOffscreen = TRUE;
		}
	}

	mVisible = TRUE;
	sVisibleTextObjects.push_back(LLPointer<LLHUDNameTag> (this));
}

LLVector2 LLHUDNameTag::updateScreenPos(LLVector2 &offset)
{
	LLCoordGL screen_pos;
	LLVector2 screen_pos_vec;
	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;
	LLViewerCamera::getInstance()->getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);
	LLVector3 world_pos = mPositionAgent + (offset.mV[VX] * x_pixel_vec) + (offset.mV[VY] * y_pixel_vec);
	if (!LLViewerCamera::getInstance()->projectPosAgentToScreen(world_pos, screen_pos, FALSE) && mVisibleOffScreen)
	{
		// bubble off-screen, so find a spot for it along screen edge
		LLViewerCamera::getInstance()->projectPosAgentToScreenEdge(world_pos, screen_pos);
	}

	screen_pos_vec.setVec((F32)screen_pos.mX, (F32)screen_pos.mY);

	LLRect world_rect = gViewerWindow->getWorldViewRectScaled();
	S32 bottom = world_rect.mBottom + STATUS_BAR_HEIGHT;

	LLVector2 screen_center;
	screen_center.mV[VX] = llclamp((F32)screen_pos_vec.mV[VX], (F32)world_rect.mLeft + mWidth * 0.5f, (F32)world_rect.mRight - mWidth * 0.5f);

	if(mVertAlignment == ALIGN_VERT_TOP)
	{
		screen_center.mV[VY] = llclamp((F32)screen_pos_vec.mV[VY], 
			(F32)bottom, 
			(F32)world_rect.mTop - mHeight - (F32)MENU_BAR_HEIGHT);
		mSoftScreenRect.setLeftTopAndSize(screen_center.mV[VX] - (mWidth + BUFFER_SIZE) * 0.5f, 
			screen_center.mV[VY] + (mHeight + BUFFER_SIZE), mWidth + BUFFER_SIZE, mHeight + BUFFER_SIZE);
	}
	else
	{
		screen_center.mV[VY] = llclamp((F32)screen_pos_vec.mV[VY], 
			(F32)bottom + mHeight * 0.5f, 
			(F32)world_rect.mTop - mHeight * 0.5f - (F32)MENU_BAR_HEIGHT);
		mSoftScreenRect.setCenterAndSize(screen_center.mV[VX], screen_center.mV[VY], mWidth + BUFFER_SIZE, mHeight + BUFFER_SIZE);
	}

	return offset + (screen_center - LLVector2((F32)screen_pos.mX, (F32)screen_pos.mY));
}

void LLHUDNameTag::updateSize()
{
	F32 height = 0.f;
	F32 width = 0.f;

	S32 max_lines = getMaxLines();
	//S32 lines = (max_lines < 0) ? (S32)mTextSegments.size() : llmin((S32)mTextSegments.size(), max_lines);
	//F32 height = (F32)mFontp->getLineHeight() * (lines + mLabelSegments.size());

	S32 start_segment;
	if (max_lines < 0) start_segment = 0;
	else start_segment = llmax((S32)0, (S32)mTextSegments.size() - max_lines);

	std::vector<LLHUDTextSegment>::iterator iter = mTextSegments.begin() + start_segment;
	while (iter != mTextSegments.end())
	{
		const LLFontGL* fontp = iter->mFont;
		height += fontp->getLineHeight();
		height += LINE_PADDING;
		width = llmax(width, llmin(iter->getWidth(fontp), HUD_TEXT_MAX_WIDTH));
		++iter;
	}

	// Don't want line spacing under the last line
	if (height > 0.f)
	{
		height -= LINE_PADDING;
	}

	iter = mLabelSegments.begin();
	while (iter != mLabelSegments.end())
	{
		height += mFontp->getLineHeight();
		width = llmax(width, llmin(iter->getWidth(mFontp), HUD_TEXT_MAX_WIDTH));
		++iter;
	}
	
	if (width == 0.f)
	{
		return;
	}

	width += HORIZONTAL_PADDING;
	height += VERTICAL_PADDING;

	// *TODO: Could do a timer-based resize here
	//mWidth = llmax(width, lerp(mWidth, (F32)width, u));
	//mHeight = llmax(height, lerp(mHeight, (F32)height, u));
	mWidth = width;
	mHeight = height;
}

void LLHUDNameTag::updateAll()
{
	// iterate over all text objects, calculate their restoration forces,
	// and add them to the visible set if they are on screen and close enough
	sVisibleTextObjects.clear();
	
	TextObjectIterator text_it;
	for (text_it = sTextObjects.begin(); text_it != sTextObjects.end(); ++text_it)
	{
		LLHUDNameTag* textp = (*text_it);
		textp->mTargetPositionOffset.clearVec();
		textp->updateSize();
		textp->updateVisibility();
	}
	
	// sort back to front for rendering purposes
	std::sort(sVisibleTextObjects.begin(), sVisibleTextObjects.end(), llhudnametag_further_away());

	// iterate from front to back, and set LOD based on current screen coverage
	F32 screen_area = (F32)(gViewerWindow->getWindowWidthScaled() * gViewerWindow->getWindowHeightScaled());
	F32 current_screen_area = 0.f;
	std::vector<LLPointer<LLHUDNameTag> >::reverse_iterator r_it;
	for (r_it = sVisibleTextObjects.rbegin(); r_it != sVisibleTextObjects.rend(); ++r_it)
	{
		LLHUDNameTag* textp = (*r_it);
//		if (textp->mUseBubble)
//		{
			if (current_screen_area / screen_area > LOD_2_SCREEN_COVERAGE)
			{
				textp->setLOD(3);
			}
			else if (current_screen_area / screen_area > LOD_1_SCREEN_COVERAGE)
			{
				textp->setLOD(2);
			}
			else if (current_screen_area / screen_area > LOD_0_SCREEN_COVERAGE)
			{
				textp->setLOD(1);
			}
			else
			{
				textp->setLOD(0);
			}
			textp->updateSize();
			// find on-screen position and initialize collision rectangle
			textp->mTargetPositionOffset = textp->updateScreenPos(LLVector2::zero);
			current_screen_area += (F32)(textp->mSoftScreenRect.getWidth() * textp->mSoftScreenRect.getHeight());
//		}
	}

	LLStat* camera_vel_stat = LLViewerCamera::getInstance()->getVelocityStat();
	F32 camera_vel = camera_vel_stat->getCurrent();
	if (camera_vel > MAX_STABLE_CAMERA_VELOCITY)
	{
		return;
	}

	VisibleTextObjectIterator src_it;

	for (S32 i = 0; i < NUM_OVERLAP_ITERATIONS; i++)
	{
		for (src_it = sVisibleTextObjects.begin(); src_it != sVisibleTextObjects.end(); ++src_it)
		{
			LLHUDNameTag* src_textp = (*src_it);

//			if (!src_textp->mUseBubble)
//			{
//				continue;
//			}
			VisibleTextObjectIterator dst_it = src_it;
			++dst_it;
			for (; dst_it != sVisibleTextObjects.end(); ++dst_it)
			{
				LLHUDNameTag* dst_textp = (*dst_it);

//				if (!dst_textp->mUseBubble)
//				{
//					continue;
//				}
				if (src_textp->mSoftScreenRect.overlaps(dst_textp->mSoftScreenRect))
				{
					LLRectf intersect_rect = src_textp->mSoftScreenRect;
					intersect_rect.intersectWith(dst_textp->mSoftScreenRect);
					intersect_rect.stretch(-BUFFER_SIZE * 0.5f);
					
					F32 src_center_x = src_textp->mSoftScreenRect.getCenterX();
					F32 src_center_y = src_textp->mSoftScreenRect.getCenterY();
					F32 dst_center_x = dst_textp->mSoftScreenRect.getCenterX();
					F32 dst_center_y = dst_textp->mSoftScreenRect.getCenterY();
					F32 intersect_center_x = intersect_rect.getCenterX();
					F32 intersect_center_y = intersect_rect.getCenterY();
					LLVector2 force = lerp(LLVector2(dst_center_x - intersect_center_x, dst_center_y - intersect_center_y), 
										LLVector2(intersect_center_x - src_center_x, intersect_center_y - src_center_y),
										0.5f);
					force.setVec(dst_center_x - src_center_x, dst_center_y - src_center_y);
					force.normVec();

					LLVector2 src_force = -1.f * force;
					LLVector2 dst_force = force;

					LLVector2 force_strength;
					F32 src_mult = dst_textp->mMass / (dst_textp->mMass + src_textp->mMass); 
					F32 dst_mult = 1.f - src_mult;
					F32 src_aspect_ratio = src_textp->mSoftScreenRect.getWidth() / src_textp->mSoftScreenRect.getHeight();
					F32 dst_aspect_ratio = dst_textp->mSoftScreenRect.getWidth() / dst_textp->mSoftScreenRect.getHeight();
					src_force.mV[VY] *= src_aspect_ratio;
					src_force.normVec();
					dst_force.mV[VY] *= dst_aspect_ratio;
					dst_force.normVec();

					src_force.mV[VX] *= llmin(intersect_rect.getWidth() * src_mult, intersect_rect.getHeight() * SPRING_STRENGTH);
					src_force.mV[VY] *= llmin(intersect_rect.getHeight() * src_mult, intersect_rect.getWidth() * SPRING_STRENGTH);
					dst_force.mV[VX] *=  llmin(intersect_rect.getWidth() * dst_mult, intersect_rect.getHeight() * SPRING_STRENGTH);
					dst_force.mV[VY] *=  llmin(intersect_rect.getHeight() * dst_mult, intersect_rect.getWidth() * SPRING_STRENGTH);
					
					src_textp->mTargetPositionOffset += src_force;
					dst_textp->mTargetPositionOffset += dst_force;
					src_textp->mTargetPositionOffset = src_textp->updateScreenPos(src_textp->mTargetPositionOffset);
					dst_textp->mTargetPositionOffset = dst_textp->updateScreenPos(dst_textp->mTargetPositionOffset);
				}
			}
		}
	}

	VisibleTextObjectIterator this_object_it;
	for (this_object_it = sVisibleTextObjects.begin(); this_object_it != sVisibleTextObjects.end(); ++this_object_it)
	{
//		if (!(*this_object_it)->mUseBubble)
//		{
//			continue;
//		}
		(*this_object_it)->mPositionOffset = lerp((*this_object_it)->mPositionOffset, (*this_object_it)->mTargetPositionOffset, LLCriticalDamp::getInterpolant(POSITION_DAMPING_TC));
	}
}

void LLHUDNameTag::setLOD(S32 lod)
{
	mLOD = lod;
	//RN: uncomment this to visualize LOD levels
	//std::string label = llformat("%d", lod);
	//setLabel(label);
}

S32 LLHUDNameTag::getMaxLines()
{
	switch(mLOD)
	{
	case 0:
		return mMaxLines;
	case 1:
		return mMaxLines > 0 ? mMaxLines / 2 : 5;
	case 2:
		return mMaxLines > 0 ? mMaxLines / 3 : 2;
	default:
		// label only
		return 0;
	}
}

void LLHUDNameTag::markDead()
{
	sTextObjects.erase(LLPointer<LLHUDNameTag>(this));
	LLHUDObject::markDead();
}

void LLHUDNameTag::shiftAll(const LLVector3& offset)
{
	TextObjectIterator text_it;
	for (text_it = sTextObjects.begin(); text_it != sTextObjects.end(); ++text_it)
	{
		LLHUDNameTag *textp = text_it->get();
		textp->shift(offset);
	}
}

void LLHUDNameTag::shift(const LLVector3& offset)
{
	mPositionAgent += offset;
}

//static 
void LLHUDNameTag::addPickable(std::set<LLViewerObject*> &pick_list)
{
	//this might put an object on the pick list a second time, overriding it's mGLName, which is ok
	// *FIX: we should probably cull against pick frustum
	VisibleTextObjectIterator text_it;
	for (text_it = sVisibleTextObjects.begin(); text_it != sVisibleTextObjects.end(); ++text_it)
	{
//		if (!(*text_it)->mUseBubble)
//		{
//			continue;
//		}
		pick_list.insert((*text_it)->mSourceObject);
	}
}

//static
// called when UI scale changes, to flush font width caches
void LLHUDNameTag::reshape()
{
	TextObjectIterator text_it;
	for (text_it = sTextObjects.begin(); text_it != sTextObjects.end(); ++text_it)
	{
		LLHUDNameTag* textp = (*text_it);
		std::vector<LLHUDTextSegment>::iterator segment_iter; 
		for (segment_iter = textp->mTextSegments.begin();
			 segment_iter != textp->mTextSegments.end(); ++segment_iter )
		{
			segment_iter->clearFontWidthMap();
		}
		for(segment_iter = textp->mLabelSegments.begin();
			segment_iter != textp->mLabelSegments.end(); ++segment_iter )
		{
			segment_iter->clearFontWidthMap();
		}		
	}
}

//============================================================================

F32 LLHUDNameTag::LLHUDTextSegment::getWidth(const LLFontGL* font)
{
	std::map<const LLFontGL*, F32>::iterator iter = mFontWidthMap.find(font);
	if (iter != mFontWidthMap.end())
	{
		return iter->second;
	}
	else
	{
		F32 width = font->getWidthF32(mText.c_str());
		mFontWidthMap[font] = width;
		return width;
	}
}
