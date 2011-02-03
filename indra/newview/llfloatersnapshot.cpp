/** 
 * @file llfloatersnapshot.cpp
 * @brief Snapshot preview window, allowing saving, e-mailing, etc.
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

#include "llviewerprecompiledheaders.h"

#include "llfloatersnapshot.h"

#include "llfloaterreg.h"

// Viewer includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llcallbacklist.h"
#include "llcriticaldamp.h"
#include "llfloaterperms.h"
#include "llui.h"
#include "llfocusmgr.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "lleconomy.h"
#include "lllandmarkactions.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llviewercontrol.h"
#include "lluictrlfactory.h"
#include "llviewerstats.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "llviewermenufile.h"	// upload_new_resource()
#include "llfloaterpostcard.h"
#include "llcheckboxctrl.h"
#include "llradiogroup.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "llwebsharing.h"
#include "llworld.h"
#include "llagentui.h"

// Linden library includes
#include "llfontgl.h"
#include "llsys.h"
#include "llrender.h"
#include "v3dmath.h"
#include "llmath.h"
#include "lldir.h"
#include "llsdserialize.h"
#include "llgl.h"
#include "llglheaders.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "lllocalcliprect.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"		// LLLocale
#include "llvfile.h"
#include "llvfs.h"
#include "llwindow.h"

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------
S32 LLFloaterSnapshot::sUIWinHeightLong = 530 ;
S32 LLFloaterSnapshot::sUIWinHeightShort = LLFloaterSnapshot::sUIWinHeightLong - 240 ;
S32 LLFloaterSnapshot::sUIWinWidth = 215 ;

LLSnapshotFloaterView* gSnapshotFloaterView = NULL;

const F32 AUTO_SNAPSHOT_TIME_DELAY = 1.f;

F32 SHINE_TIME = 0.5f;
F32 SHINE_WIDTH = 0.6f;
F32 SHINE_OPACITY = 0.3f;
F32 FALL_TIME = 0.6f;
S32 BORDER_WIDTH = 6;

const S32 MAX_POSTCARD_DATASIZE = 1024 * 1024; // one megabyte
const S32 MAX_TEXTURE_SIZE = 512 ; //max upload texture size 512 * 512

static LLDefaultChildRegistry::Register<LLSnapshotFloaterView> r("snapshot_floater_view");

///----------------------------------------------------------------------------
/// Class LLSnapshotLivePreview 
///----------------------------------------------------------------------------
class LLSnapshotLivePreview : public LLView
{
public:
	enum ESnapshotType
	{
		SNAPSHOT_POSTCARD,
		SNAPSHOT_TEXTURE,
		SNAPSHOT_LOCAL,
		SNAPSHOT_WEB
	};


	struct Params : public LLInitParam::Block<Params, LLView::Params>
	{
		Params()
		{
			name = "snapshot_live_preview";
			mouse_opaque = false;
		}
	};


	LLSnapshotLivePreview(const LLSnapshotLivePreview::Params& p);
	~LLSnapshotLivePreview();

	/*virtual*/ void draw();
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent);
	
	void setSize(S32 w, S32 h);
	void getSize(S32& w, S32& h) const;
	S32 getDataSize() const { return mDataSize; }
	void setMaxImageSize(S32 size) ;
	S32  getMaxImageSize() {return mMaxImageSize ;}
	
	ESnapshotType getSnapshotType() const { return mSnapshotType; }
	LLFloaterSnapshot::ESnapshotFormat getSnapshotFormat() const { return mSnapshotFormat; }
	BOOL getSnapshotUpToDate() const { return mSnapshotUpToDate; }
	BOOL isSnapshotActive() { return mSnapshotActive; }
	LLViewerTexture* getThumbnailImage() const { return mThumbnailImage ; }
	S32  getThumbnailWidth() const { return mThumbnailWidth ; }
	S32  getThumbnailHeight() const { return mThumbnailHeight ; }
	BOOL getThumbnailLock() const { return mThumbnailUpdateLock ; }
	BOOL getThumbnailUpToDate() const { return mThumbnailUpToDate ;}
	LLViewerTexture* getCurrentImage();
	F32 getImageAspect();
	F32 getAspect() ;
	LLRect getImageRect();
	BOOL isImageScaled();
	
	void setSnapshotType(ESnapshotType type) { mSnapshotType = type; }
	void setSnapshotFormat(LLFloaterSnapshot::ESnapshotFormat type) { mSnapshotFormat = type; }
	void setSnapshotQuality(S32 quality);
	void setSnapshotBufferType(LLViewerWindow::ESnapshotType type) { mSnapshotBufferType = type; }
	void updateSnapshot(BOOL new_snapshot, BOOL new_thumbnail = FALSE, F32 delay = 0.f);
	void saveWeb();
	LLFloaterPostcard* savePostcard();
	void saveTexture();
	BOOL saveLocal();

	BOOL setThumbnailImageSize() ;
	void generateThumbnailImage(BOOL force_update = FALSE) ;
	void resetThumbnailImage() { mThumbnailImage = NULL ; }
	void drawPreviewRect(S32 offset_x, S32 offset_y) ;

	// Returns TRUE when snapshot generated, FALSE otherwise.
	static BOOL onIdle( void* snapshot_preview );

	// callback for region name resolve
	void regionNameCallback(LLImageJPEG* snapshot, LLSD& metadata, const std::string& name, S32 x, S32 y, S32 z);

private:
	LLColor4					mColor;
	LLPointer<LLViewerTexture>	mViewerImage[2]; //used to represent the scene when the frame is frozen.
	LLRect						mImageRect[2];
	S32							mWidth[2];
	S32							mHeight[2];
	BOOL						mImageScaled[2];
	S32                         mMaxImageSize ;
	
	//thumbnail image
	LLPointer<LLViewerTexture>	mThumbnailImage ;
	S32                         mThumbnailWidth ;
	S32                         mThumbnailHeight ;
	LLRect                      mPreviewRect ;
	BOOL                        mThumbnailUpdateLock ;
	BOOL                        mThumbnailUpToDate ;

	S32							mCurImageIndex;
	LLPointer<LLImageRaw>		mPreviewImage;
	LLPointer<LLImageRaw>		mPreviewImageEncoded;
	LLPointer<LLImageFormatted>	mFormattedImage;
	LLFrameTimer				mSnapshotDelayTimer;
	S32							mShineCountdown;
	LLFrameTimer				mShineAnimTimer;
	F32							mFlashAlpha;
	BOOL						mNeedsFlash;
	LLVector3d					mPosTakenGlobal;
	S32							mSnapshotQuality;
	S32							mDataSize;
	ESnapshotType				mSnapshotType;
	LLFloaterSnapshot::ESnapshotFormat	mSnapshotFormat;
	BOOL						mSnapshotUpToDate;
	LLFrameTimer				mFallAnimTimer;
	LLVector3					mCameraPos;
	LLQuaternion				mCameraRot;
	BOOL						mSnapshotActive;
	LLViewerWindow::ESnapshotType mSnapshotBufferType;

public:
	static std::set<LLSnapshotLivePreview*> sList;
	BOOL                        mKeepAspectRatio ;
};

std::set<LLSnapshotLivePreview*> LLSnapshotLivePreview::sList;

LLSnapshotLivePreview::LLSnapshotLivePreview (const LLSnapshotLivePreview::Params& p) 
:	LLView(p),
	mColor(1.f, 0.f, 0.f, 0.5f), 
	mCurImageIndex(0),
	mPreviewImage(NULL),
	mThumbnailImage(NULL) ,
	mThumbnailWidth(0),
	mThumbnailHeight(0),
	mPreviewImageEncoded(NULL),
	mFormattedImage(NULL),
	mShineCountdown(0),
	mFlashAlpha(0.f),
	mNeedsFlash(TRUE),
	mSnapshotQuality(gSavedSettings.getS32("SnapshotQuality")),
	mDataSize(0),
	mSnapshotType(SNAPSHOT_POSTCARD),
	mSnapshotFormat(LLFloaterSnapshot::ESnapshotFormat(gSavedSettings.getS32("SnapshotFormat"))),
	mSnapshotUpToDate(FALSE),
	mCameraPos(LLViewerCamera::getInstance()->getOrigin()),
	mCameraRot(LLViewerCamera::getInstance()->getQuaternion()),
	mSnapshotActive(FALSE),
	mSnapshotBufferType(LLViewerWindow::SNAPSHOT_TYPE_COLOR)
{
	setSnapshotQuality(gSavedSettings.getS32("SnapshotQuality"));
	mSnapshotDelayTimer.setTimerExpirySec(0.0f);
	mSnapshotDelayTimer.start();
// 	gIdleCallbacks.addFunction( &LLSnapshotLivePreview::onIdle, (void*)this );
	sList.insert(this);
	setFollowsAll();
	mWidth[0] = gViewerWindow->getWindowWidthRaw();
	mWidth[1] = gViewerWindow->getWindowWidthRaw();
	mHeight[0] = gViewerWindow->getWindowHeightRaw();
	mHeight[1] = gViewerWindow->getWindowHeightRaw();
	mImageScaled[0] = FALSE;
	mImageScaled[1] = FALSE;

	mMaxImageSize = MAX_SNAPSHOT_IMAGE_SIZE ;
	mKeepAspectRatio = gSavedSettings.getBOOL("KeepAspectForSnapshot") ;
	mThumbnailUpdateLock = FALSE ;
	mThumbnailUpToDate   = FALSE ;
}

LLSnapshotLivePreview::~LLSnapshotLivePreview()
{
	// delete images
	mPreviewImage = NULL;
	mPreviewImageEncoded = NULL;
	mFormattedImage = NULL;

// 	gIdleCallbacks.deleteFunction( &LLSnapshotLivePreview::onIdle, (void*)this );
	sList.erase(this);
}

void LLSnapshotLivePreview::setMaxImageSize(S32 size) 
{
	if(size < MAX_SNAPSHOT_IMAGE_SIZE)
	{
		mMaxImageSize = size;
	}
	else
	{
		mMaxImageSize = MAX_SNAPSHOT_IMAGE_SIZE ;
	}
}

LLViewerTexture* LLSnapshotLivePreview::getCurrentImage()
{
	return mViewerImage[mCurImageIndex];
}

F32 LLSnapshotLivePreview::getAspect()
{
	F32 image_aspect_ratio = ((F32)mWidth[mCurImageIndex]) / ((F32)mHeight[mCurImageIndex]);
	F32 window_aspect_ratio = ((F32)getRect().getWidth()) / ((F32)getRect().getHeight());

	if (!mKeepAspectRatio)//gSavedSettings.getBOOL("KeepAspectForSnapshot"))
	{
		return image_aspect_ratio;
	}
	else
	{
		return window_aspect_ratio;
	}
}

F32 LLSnapshotLivePreview::getImageAspect()
{
	if (!mViewerImage[mCurImageIndex])
	{
		return 0.f;
	}

	return getAspect() ;	
}

LLRect LLSnapshotLivePreview::getImageRect()
{
	return mImageRect[mCurImageIndex];
}

BOOL LLSnapshotLivePreview::isImageScaled()
{
	return mImageScaled[mCurImageIndex];
}

void LLSnapshotLivePreview::updateSnapshot(BOOL new_snapshot, BOOL new_thumbnail, F32 delay) 
{ 
	if (mSnapshotUpToDate)
	{
		S32 old_image_index = mCurImageIndex;
		mCurImageIndex = (mCurImageIndex + 1) % 2; 
		mWidth[mCurImageIndex] = mWidth[old_image_index];
		mHeight[mCurImageIndex] = mHeight[old_image_index];
		mFallAnimTimer.start();		
	}
	mSnapshotUpToDate = FALSE; 		

	LLRect& rect = mImageRect[mCurImageIndex];
	rect.set(0, getRect().getHeight(), getRect().getWidth(), 0);

	F32 image_aspect_ratio = ((F32)mWidth[mCurImageIndex]) / ((F32)mHeight[mCurImageIndex]);
	F32 window_aspect_ratio = ((F32)getRect().getWidth()) / ((F32)getRect().getHeight());

	if (mKeepAspectRatio)//gSavedSettings.getBOOL("KeepAspectForSnapshot"))
	{
		if (image_aspect_ratio > window_aspect_ratio)
		{
			// trim off top and bottom
			S32 new_height = llround((F32)getRect().getWidth() / image_aspect_ratio); 
			rect.mBottom += (getRect().getHeight() - new_height) / 2;
			rect.mTop -= (getRect().getHeight() - new_height) / 2;
		}
		else if (image_aspect_ratio < window_aspect_ratio)
		{
			// trim off left and right
			S32 new_width = llround((F32)getRect().getHeight() * image_aspect_ratio); 
			rect.mLeft += (getRect().getWidth() - new_width) / 2;
			rect.mRight -= (getRect().getWidth() - new_width) / 2;
		}
	}

	mShineAnimTimer.stop();
	if (new_snapshot)
	{
		mSnapshotDelayTimer.start();
		mSnapshotDelayTimer.setTimerExpirySec(delay);
	}
	if(new_thumbnail)
	{
		mThumbnailUpToDate = FALSE ;
	}
	setThumbnailImageSize();
}

void LLSnapshotLivePreview::setSnapshotQuality(S32 quality)
{
	llclamp(quality, 0, 100);
	if (quality != mSnapshotQuality)
	{
		mSnapshotQuality = quality;
		gSavedSettings.setS32("SnapshotQuality", quality);
		mSnapshotUpToDate = FALSE;
	}
}

void LLSnapshotLivePreview::drawPreviewRect(S32 offset_x, S32 offset_y)
{
	F32 line_width ; 
	glGetFloatv(GL_LINE_WIDTH, &line_width) ;
	glLineWidth(2.0f * line_width) ;
	LLColor4 color(0.0f, 0.0f, 0.0f, 1.0f) ;
	gl_rect_2d( mPreviewRect.mLeft + offset_x, mPreviewRect.mTop + offset_y,
				mPreviewRect.mRight + offset_x, mPreviewRect.mBottom + offset_y, color, FALSE ) ;
	glLineWidth(line_width) ;

	//draw four alpha rectangles to cover areas outside of the snapshot image
	if(!mKeepAspectRatio)
	{
		LLColor4 alpha_color(0.5f, 0.5f, 0.5f, 0.8f) ;
		S32 dwl = 0, dwr = 0 ;
		if(mThumbnailWidth > mPreviewRect.getWidth())
		{
			dwl = (mThumbnailWidth - mPreviewRect.getWidth()) >> 1 ;
			dwr = mThumbnailWidth - mPreviewRect.getWidth() - dwl ;

			gl_rect_2d(mPreviewRect.mLeft + offset_x - dwl, mPreviewRect.mTop + offset_y,
				mPreviewRect.mLeft + offset_x, mPreviewRect.mBottom + offset_y, alpha_color, TRUE ) ;
			gl_rect_2d( mPreviewRect.mRight + offset_x, mPreviewRect.mTop + offset_y,
				mPreviewRect.mRight + offset_x + dwr, mPreviewRect.mBottom + offset_y, alpha_color, TRUE ) ;
		}

		if(mThumbnailHeight > mPreviewRect.getHeight())
		{
			S32 dh = (mThumbnailHeight - mPreviewRect.getHeight()) >> 1 ;
			gl_rect_2d(mPreviewRect.mLeft + offset_x - dwl, mPreviewRect.mBottom + offset_y ,
				mPreviewRect.mRight + offset_x + dwr, mPreviewRect.mBottom + offset_y - dh, alpha_color, TRUE ) ;

			dh = mThumbnailHeight - mPreviewRect.getHeight() - dh ;
			gl_rect_2d( mPreviewRect.mLeft + offset_x - dwl, mPreviewRect.mTop + offset_y + dh,
				mPreviewRect.mRight + offset_x + dwr, mPreviewRect.mTop + offset_y, alpha_color, TRUE ) ;
		}
	}
}

//called when the frame is frozen.
void LLSnapshotLivePreview::draw()
{
	if (mViewerImage[mCurImageIndex].notNull() &&
	    mPreviewImageEncoded.notNull() &&
	    mSnapshotUpToDate)
	{
		LLColor4 bg_color(0.f, 0.f, 0.3f, 0.4f);
		gl_rect_2d(getRect(), bg_color);
		LLRect &rect = mImageRect[mCurImageIndex];
		LLRect shadow_rect = mImageRect[mCurImageIndex];
		shadow_rect.stretch(BORDER_WIDTH);
		gl_drop_shadow(shadow_rect.mLeft, shadow_rect.mTop, shadow_rect.mRight, shadow_rect.mBottom, LLColor4(0.f, 0.f, 0.f, mNeedsFlash ? 0.f :0.5f), 10);

		LLColor4 image_color(1.f, 1.f, 1.f, 1.f);
		gGL.color4fv(image_color.mV);
		gGL.getTexUnit(0)->bind(mViewerImage[mCurImageIndex]);
		// calculate UV scale
		F32 uv_width = mImageScaled[mCurImageIndex] ? 1.f : llmin((F32)mWidth[mCurImageIndex] / (F32)mViewerImage[mCurImageIndex]->getWidth(), 1.f);
		F32 uv_height = mImageScaled[mCurImageIndex] ? 1.f : llmin((F32)mHeight[mCurImageIndex] / (F32)mViewerImage[mCurImageIndex]->getHeight(), 1.f);
		glPushMatrix();
		{
			glTranslatef((F32)rect.mLeft, (F32)rect.mBottom, 0.f);
			gGL.begin(LLRender::QUADS);
			{
				gGL.texCoord2f(uv_width, uv_height);
				gGL.vertex2i(rect.getWidth(), rect.getHeight() );

				gGL.texCoord2f(0.f, uv_height);
				gGL.vertex2i(0, rect.getHeight() );

				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2i(0, 0);

				gGL.texCoord2f(uv_width, 0.f);
				gGL.vertex2i(rect.getWidth(), 0);
			}
			gGL.end();
		}
		glPopMatrix();

		gGL.color4f(1.f, 1.f, 1.f, mFlashAlpha);
		gl_rect_2d(getRect());
		if (mNeedsFlash)
		{
			if (mFlashAlpha < 1.f)
			{
				mFlashAlpha = lerp(mFlashAlpha, 1.f, LLCriticalDamp::getInterpolant(0.02f));
			}
			else
			{
				mNeedsFlash = FALSE;
			}
		}
		else
		{
			mFlashAlpha = lerp(mFlashAlpha, 0.f, LLCriticalDamp::getInterpolant(0.15f));
		}

		if (mShineCountdown > 0)
		{
			mShineCountdown--;
			if (mShineCountdown == 0)
			{
				mShineAnimTimer.start();
			}
		}
		else if (mShineAnimTimer.getStarted())
		{
			F32 shine_interp = llmin(1.f, mShineAnimTimer.getElapsedTimeF32() / SHINE_TIME);
			
			// draw "shine" effect
			LLLocalClipRect clip(getLocalRect());
			{
				// draw diagonal stripe with gradient that passes over screen
				S32 x1 = gViewerWindow->getWindowWidthScaled() * llround((clamp_rescale(shine_interp, 0.f, 1.f, -1.f - SHINE_WIDTH, 1.f)));
				S32 x2 = x1 + llround(gViewerWindow->getWindowWidthScaled() * SHINE_WIDTH);
				S32 x3 = x2 + llround(gViewerWindow->getWindowWidthScaled() * SHINE_WIDTH);
				S32 y1 = 0;
				S32 y2 = gViewerWindow->getWindowHeightScaled();

				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				gGL.begin(LLRender::QUADS);
				{
					gGL.color4f(1.f, 1.f, 1.f, 0.f);
					gGL.vertex2i(x1, y1);
					gGL.vertex2i(x1 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.color4f(1.f, 1.f, 1.f, SHINE_OPACITY);
					gGL.vertex2i(x2 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.vertex2i(x2, y1);

					gGL.color4f(1.f, 1.f, 1.f, SHINE_OPACITY);
					gGL.vertex2i(x2, y1);
					gGL.vertex2i(x2 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.color4f(1.f, 1.f, 1.f, 0.f);
					gGL.vertex2i(x3 + gViewerWindow->getWindowWidthScaled(), y2);
					gGL.vertex2i(x3, y1);
				}
				gGL.end();
			}

			// if we're at the end of the animation, stop
			if (shine_interp >= 1.f)
			{
				mShineAnimTimer.stop();
			}
		}
	}

	// draw framing rectangle
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		LLRect outline_rect = mImageRect[mCurImageIndex];
		gGL.begin(LLRender::QUADS);
		{
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mTop);

			gGL.vertex2i(outline_rect.mLeft, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);

			gGL.vertex2i(outline_rect.mLeft, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);

			gGL.vertex2i(outline_rect.mRight, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH, outline_rect.mBottom - BORDER_WIDTH);
		}
		gGL.end();
	}

	// draw old image dropping away
	if (mFallAnimTimer.getStarted())
	{
		S32 old_image_index = (mCurImageIndex + 1) % 2;
		if (mViewerImage[old_image_index].notNull() && mFallAnimTimer.getElapsedTimeF32() < FALL_TIME)
		{
			F32 fall_interp = mFallAnimTimer.getElapsedTimeF32() / FALL_TIME;
			F32 alpha = clamp_rescale(fall_interp, 0.f, 1.f, 0.8f, 0.4f);
			LLColor4 image_color(1.f, 1.f, 1.f, alpha);
			gGL.color4fv(image_color.mV);
			gGL.getTexUnit(0)->bind(mViewerImage[old_image_index]);
			// calculate UV scale
			// *FIX get this to work with old image
			BOOL rescale = !mImageScaled[old_image_index] && mViewerImage[mCurImageIndex].notNull();
			F32 uv_width = rescale ? llmin((F32)mWidth[old_image_index] / (F32)mViewerImage[mCurImageIndex]->getWidth(), 1.f) : 1.f;
			F32 uv_height = rescale ? llmin((F32)mHeight[old_image_index] / (F32)mViewerImage[mCurImageIndex]->getHeight(), 1.f) : 1.f;
			glPushMatrix();
			{
				LLRect& rect = mImageRect[old_image_index];
				glTranslatef((F32)rect.mLeft, (F32)rect.mBottom - llround(getRect().getHeight() * 2.f * (fall_interp * fall_interp)), 0.f);
				glRotatef(-45.f * fall_interp, 0.f, 0.f, 1.f);
				gGL.begin(LLRender::QUADS);
				{
					gGL.texCoord2f(uv_width, uv_height);
					gGL.vertex2i(rect.getWidth(), rect.getHeight() );

					gGL.texCoord2f(0.f, uv_height);
					gGL.vertex2i(0, rect.getHeight() );

					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex2i(0, 0);

					gGL.texCoord2f(uv_width, 0.f);
					gGL.vertex2i(rect.getWidth(), 0);
				}
				gGL.end();
			}
			glPopMatrix();
		}
	}
}

/*virtual*/ 
void LLSnapshotLivePreview::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLRect old_rect = getRect();
	LLView::reshape(width, height, called_from_parent);
	if (old_rect.getWidth() != width || old_rect.getHeight() != height)
	{
		updateSnapshot(FALSE, TRUE);
	}
}

BOOL LLSnapshotLivePreview::setThumbnailImageSize()
{
	if(mWidth[mCurImageIndex] < 10 || mHeight[mCurImageIndex] < 10)
	{
		return FALSE ;
	}
	S32 window_width = gViewerWindow->getWindowWidthRaw() ;
	S32 window_height = gViewerWindow->getWindowHeightRaw() ;

	F32 window_aspect_ratio = ((F32)window_width) / ((F32)window_height);

	// UI size for thumbnail
	S32 max_width = LLFloaterSnapshot::getUIWinWidth() - 20;
	S32 max_height = 90;

	if (window_aspect_ratio > (F32)max_width / max_height)
	{
		// image too wide, shrink to width
		mThumbnailWidth = max_width;
		mThumbnailHeight = llround((F32)max_width / window_aspect_ratio);
	}
	else
	{
		// image too tall, shrink to height
		mThumbnailHeight = max_height;
		mThumbnailWidth = llround((F32)max_height * window_aspect_ratio);
	}
	
	if(mThumbnailWidth > window_width || mThumbnailHeight > window_height)
	{
		return FALSE ;//if the window is too small, ignore thumbnail updating.
	}

	S32 left = 0 , top = mThumbnailHeight, right = mThumbnailWidth, bottom = 0 ;
	if(!mKeepAspectRatio)
	{
		F32 ratio_x = (F32)mWidth[mCurImageIndex] / window_width ;
		F32 ratio_y = (F32)mHeight[mCurImageIndex] / window_height ;

		//if(mWidth[mCurImageIndex] > window_width ||
		//	mHeight[mCurImageIndex] > window_height )
		{
			if(ratio_x > ratio_y)
			{
				top = (S32)(top * ratio_y / ratio_x) ;
			}
			else
			{
				right = (S32)(right * ratio_x / ratio_y) ;
			}			
		}
		//else
		//{
		//	right = (S32)(right * ratio_x) ;
		//	top = (S32)(top * ratio_y) ;
		//}
		left = (S32)((mThumbnailWidth - right) * 0.5f) ;
		bottom = (S32)((mThumbnailHeight - top) * 0.5f) ;
		top += bottom ;
		right += left ;
	}
	mPreviewRect.set(left - 1, top + 1, right + 1, bottom - 1) ;

	return TRUE ;
}

void LLSnapshotLivePreview::generateThumbnailImage(BOOL force_update)
{	
	if(mThumbnailUpdateLock) //in the process of updating
	{
		return ;
	}
	if(mThumbnailUpToDate && !force_update)//already updated
	{
		return ;
	}
	if(mWidth[mCurImageIndex] < 10 || mHeight[mCurImageIndex] < 10)
	{
		return ;
	}

	////lock updating
	mThumbnailUpdateLock = TRUE ;

	if(!setThumbnailImageSize())
	{
		mThumbnailUpdateLock = FALSE ;
		mThumbnailUpToDate = TRUE ;
		return ;
	}

	if(mThumbnailImage)
	{
		resetThumbnailImage() ;
	}		

	LLPointer<LLImageRaw> raw = NULL ;
	S32 w , h ;
	w = get_lower_power_two(mThumbnailWidth, 512) * 2 ;
	h = get_lower_power_two(mThumbnailHeight, 512) * 2 ;

	{
		raw = new LLImageRaw ;
		if(!gViewerWindow->thumbnailSnapshot(raw,
								w, h,
								gSavedSettings.getBOOL("RenderUIInSnapshot"),
								FALSE,
								mSnapshotBufferType) )								
		{
			raw = NULL ;
		}
	}

	if(raw)
	{
		mThumbnailImage = LLViewerTextureManager::getLocalTexture(raw.get(), FALSE); 		
		mThumbnailUpToDate = TRUE ;
	}

	//unlock updating
	mThumbnailUpdateLock = FALSE ;		
}


// Called often. Checks whether it's time to grab a new snapshot and if so, does it.
// Returns TRUE if new snapshot generated, FALSE otherwise.
//static 
BOOL LLSnapshotLivePreview::onIdle( void* snapshot_preview )
{
	LLSnapshotLivePreview* previewp = (LLSnapshotLivePreview*)snapshot_preview;	

	LLVector3 new_camera_pos = LLViewerCamera::getInstance()->getOrigin();
	LLQuaternion new_camera_rot = LLViewerCamera::getInstance()->getQuaternion();
	if (gSavedSettings.getBOOL("FreezeTime") && 
		(new_camera_pos != previewp->mCameraPos || dot(new_camera_rot, previewp->mCameraRot) < 0.995f))
	{
		previewp->mCameraPos = new_camera_pos;
		previewp->mCameraRot = new_camera_rot;
		// request a new snapshot whenever the camera moves, with a time delay
		BOOL autosnap = gSavedSettings.getBOOL("AutoSnapshot");
		previewp->updateSnapshot(
			autosnap, // whether a new snapshot is needed or merely invalidate the existing one
			FALSE, // or if 1st arg is false, whether to produce a new thumbnail image.
			autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f); // shutter delay if 1st arg is true.
	}

	// see if it's time yet to snap the shot and bomb out otherwise.
	previewp->mSnapshotActive = 
		(previewp->mSnapshotDelayTimer.getStarted() &&	previewp->mSnapshotDelayTimer.hasExpired())
		&& !LLToolCamera::getInstance()->hasMouseCapture(); // don't take snapshots while ALT-zoom active
	if ( ! previewp->mSnapshotActive)
	{
		return FALSE;
	}

	// time to produce a snapshot

	if (!previewp->mPreviewImage)
	{
		previewp->mPreviewImage = new LLImageRaw;
	}

	if (!previewp->mPreviewImageEncoded)
	{
		previewp->mPreviewImageEncoded = new LLImageRaw;
	}

	previewp->setVisible(FALSE);
	previewp->setEnabled(FALSE);
	
	previewp->getWindow()->incBusyCount();
	previewp->mImageScaled[previewp->mCurImageIndex] = FALSE;

	// grab the raw image and encode it into desired format
	if(gViewerWindow->rawSnapshot(
							previewp->mPreviewImage,
							previewp->mWidth[previewp->mCurImageIndex],
							previewp->mHeight[previewp->mCurImageIndex],
							previewp->mKeepAspectRatio,//gSavedSettings.getBOOL("KeepAspectForSnapshot"),
							previewp->getSnapshotType() == LLSnapshotLivePreview::SNAPSHOT_TEXTURE,
							gSavedSettings.getBOOL("RenderUIInSnapshot"),
							FALSE,
							previewp->mSnapshotBufferType,
							previewp->getMaxImageSize()))
	{
		previewp->mPreviewImageEncoded->resize(
			previewp->mPreviewImage->getWidth(), 
			previewp->mPreviewImage->getHeight(), 
			previewp->mPreviewImage->getComponents());

		if(previewp->getSnapshotType() == SNAPSHOT_TEXTURE)
		{
			LLPointer<LLImageJ2C> formatted = new LLImageJ2C;
			LLPointer<LLImageRaw> scaled = new LLImageRaw(
				previewp->mPreviewImage->getData(),
				previewp->mPreviewImage->getWidth(),
				previewp->mPreviewImage->getHeight(),
				previewp->mPreviewImage->getComponents());
		
			scaled->biasedScaleToPowerOfTwo(512);
			previewp->mImageScaled[previewp->mCurImageIndex] = TRUE;
			if (formatted->encode(scaled, 0.f))
			{
				previewp->mDataSize = formatted->getDataSize();
				formatted->decode(previewp->mPreviewImageEncoded, 0);
			}
		}
		else
		{
			// delete any existing image
			previewp->mFormattedImage = NULL;
			// now create the new one of the appropriate format.
			// note: postcards and web hardcoded to use jpeg always.
			LLFloaterSnapshot::ESnapshotFormat format;

			if (previewp->getSnapshotType() == SNAPSHOT_POSTCARD ||
				previewp->getSnapshotType() == SNAPSHOT_WEB)
			{
				format = LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG;
			}
			else
			{
				format = previewp->getSnapshotFormat();
			}

			switch(format)
			{
			case LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG:
				previewp->mFormattedImage = new LLImagePNG(); 
				break;
			case LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG:
				previewp->mFormattedImage = new LLImageJPEG(previewp->mSnapshotQuality); 
				break;
			case LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP:
				previewp->mFormattedImage = new LLImageBMP(); 
				break;
			}
			if (previewp->mFormattedImage->encode(previewp->mPreviewImage, 0))
			{
				previewp->mDataSize = previewp->mFormattedImage->getDataSize();
				// special case BMP to copy instead of decode otherwise decode will crash.
				if(format == LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP)
				{
					previewp->mPreviewImageEncoded->copy(previewp->mPreviewImage);
				}
				else
				{
					previewp->mFormattedImage->decode(previewp->mPreviewImageEncoded, 0);
				}
			}
		}

		LLPointer<LLImageRaw> scaled = new LLImageRaw(
			previewp->mPreviewImageEncoded->getData(),
			previewp->mPreviewImageEncoded->getWidth(),
			previewp->mPreviewImageEncoded->getHeight(),
			previewp->mPreviewImageEncoded->getComponents());
		
		if(!scaled->isBufferInvalid())
		{
			// leave original image dimensions, just scale up texture buffer
			if (previewp->mPreviewImageEncoded->getWidth() > 1024 || previewp->mPreviewImageEncoded->getHeight() > 1024)
			{
				// go ahead and shrink image to appropriate power of 2 for display
				scaled->biasedScaleToPowerOfTwo(1024);
				previewp->mImageScaled[previewp->mCurImageIndex] = TRUE;
			}
			else
			{
				// expand image but keep original image data intact
				scaled->expandToPowerOfTwo(1024, FALSE);
			}

			previewp->mViewerImage[previewp->mCurImageIndex] = LLViewerTextureManager::getLocalTexture(scaled.get(), FALSE);
			LLPointer<LLViewerTexture> curr_preview_image = previewp->mViewerImage[previewp->mCurImageIndex];
			gGL.getTexUnit(0)->bind(curr_preview_image);
			if (previewp->getSnapshotType() != SNAPSHOT_TEXTURE)
			{
				curr_preview_image->setFilteringOption(LLTexUnit::TFO_POINT);
			}
			else
			{
				curr_preview_image->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
			}
			curr_preview_image->setAddressMode(LLTexUnit::TAM_CLAMP);

			previewp->mSnapshotUpToDate = TRUE;
			previewp->generateThumbnailImage(TRUE) ;

			previewp->mPosTakenGlobal = gAgentCamera.getCameraPositionGlobal();
			previewp->mShineCountdown = 4; // wait a few frames to avoid animation glitch due to readback this frame
		}
	}
	previewp->getWindow()->decBusyCount();
	// only show fullscreen preview when in freeze frame mode
	previewp->setVisible(gSavedSettings.getBOOL("UseFreezeFrame"));
	previewp->mSnapshotDelayTimer.stop();
	previewp->mSnapshotActive = FALSE;

	if(!previewp->getThumbnailUpToDate())
	{
		previewp->generateThumbnailImage() ;
	}

	return TRUE;
}

void LLSnapshotLivePreview::setSize(S32 w, S32 h)
{
	mWidth[mCurImageIndex] = w;
	mHeight[mCurImageIndex] = h;
}

void LLSnapshotLivePreview::getSize(S32& w, S32& h) const
{
	w = mWidth[mCurImageIndex];
	h = mHeight[mCurImageIndex];
}

LLFloaterPostcard* LLSnapshotLivePreview::savePostcard()
{
	if(mViewerImage[mCurImageIndex].isNull())
	{
		//this should never happen!!
		llwarns << "The snapshot image has not been generated!" << llendl ;
		return NULL ;
	}

	// calculate and pass in image scale in case image data only use portion
	// of viewerimage buffer
	LLVector2 image_scale(1.f, 1.f);
	if (!isImageScaled())
	{
		image_scale.setVec(llmin(1.f, (F32)mWidth[mCurImageIndex] / (F32)getCurrentImage()->getWidth()), llmin(1.f, (F32)mHeight[mCurImageIndex] / (F32)getCurrentImage()->getHeight()));
	}

	LLImageJPEG* jpg = dynamic_cast<LLImageJPEG*>(mFormattedImage.get());
	if(!jpg)
	{
		llwarns << "Formatted image not a JPEG" << llendl;
		return NULL;
	}
	LLFloaterPostcard* floater = LLFloaterPostcard::showFromSnapshot(jpg, mViewerImage[mCurImageIndex], image_scale, mPosTakenGlobal);
	// relinquish lifetime of jpeg image to postcard floater
	mFormattedImage = NULL;
	mDataSize = 0;
	updateSnapshot(FALSE, FALSE);

	return floater;
}

void LLSnapshotLivePreview::saveTexture()
{
	// gen a new uuid for this asset
	LLTransactionID tid;
	tid.generate();
	LLAssetID new_asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
		
	LLPointer<LLImageJ2C> formatted = new LLImageJ2C;
	LLPointer<LLImageRaw> scaled = new LLImageRaw(mPreviewImage->getData(),
												  mPreviewImage->getWidth(),
												  mPreviewImage->getHeight(),
												  mPreviewImage->getComponents());
	
	scaled->biasedScaleToPowerOfTwo(512);
			
	if (formatted->encode(scaled, 0.0f))
	{
		LLVFile::writeFile(formatted->getData(), formatted->getDataSize(), gVFS, new_asset_id, LLAssetType::AT_TEXTURE);
		std::string pos_string;
		LLAgentUI::buildLocationString(pos_string, LLAgentUI::LOCATION_FORMAT_FULL);
		std::string who_took_it;
		LLAgentUI::buildFullname(who_took_it);
		LLAssetStorage::LLStoreAssetCallback callback = NULL;
		S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
		void *userdata = NULL;
		upload_new_resource(tid,	// tid
				    LLAssetType::AT_TEXTURE,
				    "Snapshot : " + pos_string,
				    "Taken by " + who_took_it + " at " + pos_string,
				    0,
				    LLFolderType::FT_SNAPSHOT_CATEGORY,
				    LLInventoryType::IT_SNAPSHOT,
				    PERM_ALL,  // Note: Snapshots to inventory is a special case of content upload
				    LLFloaterPerms::getGroupPerms(), // that is more permissive than other uploads
				    LLFloaterPerms::getEveryonePerms(),
				    "Snapshot : " + pos_string,
				    callback, expected_upload_cost, userdata);
		gViewerWindow->playSnapshotAnimAndSound();
	}
	else
	{
		LLNotificationsUtil::add("ErrorEncodingSnapshot");
		llwarns << "Error encoding snapshot" << llendl;
	}

	LLViewerStats::getInstance()->incStat(LLViewerStats::ST_SNAPSHOT_COUNT );
	
	mDataSize = 0;
}

BOOL LLSnapshotLivePreview::saveLocal()
{
	BOOL success = gViewerWindow->saveImageNumbered(mFormattedImage);

	// Relinquish image memory. Save button will be disabled as a side-effect.
	mFormattedImage = NULL;
	mDataSize = 0;
	updateSnapshot(FALSE, FALSE);

	if(success)
	{
		gViewerWindow->playSnapshotAnimAndSound();
	}
	return success;
}

void LLSnapshotLivePreview::saveWeb()
{
	// *FIX: Will break if the window closes because of CloseSnapshotOnKeep!
	// Needs to pass on ownership of the image.
	LLImageJPEG* jpg = dynamic_cast<LLImageJPEG*>(mFormattedImage.get());
	if(!jpg)
	{
		llwarns << "Formatted image not a JPEG" << llendl;
		return;
	}

	LLSD metadata;
	metadata["description"] = getChild<LLLineEditor>("description")->getText();

	LLLandmarkActions::getRegionNameAndCoordsFromPosGlobal(gAgentCamera.getCameraPositionGlobal(),
		boost::bind(&LLSnapshotLivePreview::regionNameCallback, this, jpg, metadata, _1, _2, _3, _4));

	gViewerWindow->playSnapshotAnimAndSound();
}

void LLSnapshotLivePreview::regionNameCallback(LLImageJPEG* snapshot, LLSD& metadata, const std::string& name, S32 x, S32 y, S32 z)
{
	metadata["slurl"] = LLSLURL(name, LLVector3d(x, y, z)).getSLURLString();

	LLWebSharing::instance().shareSnapshot(snapshot, metadata);
}

///----------------------------------------------------------------------------
/// Class LLFloaterSnapshot::Impl
///----------------------------------------------------------------------------

class LLFloaterSnapshot::Impl
{
public:
	Impl()
	:	mAvatarPauseHandles(),
		mLastToolset(NULL),
		mAspectRatioCheckOff(false)
	{
	}
	~Impl()
	{
		//unpause avatars
		mAvatarPauseHandles.clear();

	}
	static void onClickDiscard(void* data);
	static void onClickKeep(void* data);
	static void onCommitSave(LLUICtrl* ctrl, void* data);
	static void onClickNewSnapshot(void* data);
	static void onClickAutoSnap(LLUICtrl *ctrl, void* data);
	//static void onClickAdvanceSnap(LLUICtrl *ctrl, void* data);
	static void onClickLess(void* data) ;
	static void onClickMore(void* data) ;
	static void onClickUICheck(LLUICtrl *ctrl, void* data);
	static void onClickHUDCheck(LLUICtrl *ctrl, void* data);
	static void onClickKeepOpenCheck(LLUICtrl *ctrl, void* data);
	static void onClickKeepAspectCheck(LLUICtrl *ctrl, void* data);
	static void onCommitQuality(LLUICtrl* ctrl, void* data);
	static void onCommitResolution(LLUICtrl* ctrl, void* data) { updateResolution(ctrl, data); }
	static void updateResolution(LLUICtrl* ctrl, void* data, BOOL do_update = TRUE);
	static void onCommitFreezeFrame(LLUICtrl* ctrl, void* data);
	static void onCommitLayerTypes(LLUICtrl* ctrl, void*data);
	static void onCommitSnapshotType(LLUICtrl* ctrl, void* data);
	static void onCommitSnapshotFormat(LLUICtrl* ctrl, void* data);
	static void onCommitCustomResolution(LLUICtrl *ctrl, void* data);
	static void resetSnapshotSizeOnUI(LLFloaterSnapshot *view, S32 width, S32 height) ;
	static BOOL checkImageSize(LLSnapshotLivePreview* previewp, S32& width, S32& height, BOOL isWidthChanged, S32 max_value);

	static LLSnapshotLivePreview* getPreviewView(LLFloaterSnapshot *floater);
	static void setResolution(LLFloaterSnapshot* floater, const std::string& comboname);
	static void updateControls(LLFloaterSnapshot* floater);
	static void updateLayout(LLFloaterSnapshot* floater);
	static void updateResolutionTextEntry(LLFloaterSnapshot* floater);

private:
	static LLSnapshotLivePreview::ESnapshotType getTypeIndex(LLFloaterSnapshot* floater);
	static LLSD getTypeName(LLSnapshotLivePreview::ESnapshotType index);
	static ESnapshotFormat getFormatIndex(LLFloaterSnapshot* floater);
	static LLViewerWindow::ESnapshotType getLayerType(LLFloaterSnapshot* floater);
	static void comboSetCustom(LLFloaterSnapshot *floater, const std::string& comboname);
	static void checkAutoSnapshot(LLSnapshotLivePreview* floater, BOOL update_thumbnail = FALSE);
	static void checkAspectRatio(LLFloaterSnapshot *view, S32 index) ;

public:
	std::vector<LLAnimPauseRequest> mAvatarPauseHandles;

	LLToolset*	mLastToolset;
	LLHandle<LLView> mPreviewHandle;
	bool mAspectRatioCheckOff ;
};

// static
LLSnapshotLivePreview* LLFloaterSnapshot::Impl::getPreviewView(LLFloaterSnapshot *floater)
{
	LLSnapshotLivePreview* previewp = (LLSnapshotLivePreview*)floater->impl.mPreviewHandle.get();
	return previewp;
}

// static
LLSnapshotLivePreview::ESnapshotType LLFloaterSnapshot::Impl::getTypeIndex(LLFloaterSnapshot* floater)
{
	LLSnapshotLivePreview::ESnapshotType index = LLSnapshotLivePreview::SNAPSHOT_POSTCARD;
	LLSD value = floater->getChild<LLUICtrl>("snapshot_type_radio")->getValue();

	const std::string id = value.asString();
	if (id == "postcard")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_POSTCARD;
	}
	else if (id == "texture")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_TEXTURE;
	}
	else if (id == "local")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_LOCAL;
	}
	else if (id == "share_to_web")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_WEB;
	}

	return index;
}

// static
LLSD LLFloaterSnapshot::Impl::getTypeName(LLSnapshotLivePreview::ESnapshotType index)
{
	std::string id;
	switch (index)
	{
		case LLSnapshotLivePreview::SNAPSHOT_WEB:
			id = "share_to_web";
			break;
		case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
			id = "postcard";
			break;
		case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
			id = "texture";
			break;
		case LLSnapshotLivePreview::SNAPSHOT_LOCAL:
		default:
			id = "local";
			break;
	}
	return LLSD(id);
}

// static
LLFloaterSnapshot::ESnapshotFormat LLFloaterSnapshot::Impl::getFormatIndex(LLFloaterSnapshot* floater)
{
	ESnapshotFormat index = SNAPSHOT_FORMAT_PNG;
	if(floater->hasChild("local_format_combo"))
	{
		LLComboBox* local_format_combo = floater->findChild<LLComboBox>("local_format_combo");
		const std::string id  = local_format_combo->getSelectedItemLabel();
		if (id == "PNG")
			index = SNAPSHOT_FORMAT_PNG;
		else if (id == "JPEG")
			index = SNAPSHOT_FORMAT_JPEG;
		else if (id == "BMP")
			index = SNAPSHOT_FORMAT_BMP;
	}
		return index;
}



// static
LLViewerWindow::ESnapshotType LLFloaterSnapshot::Impl::getLayerType(LLFloaterSnapshot* floater)
{
	LLViewerWindow::ESnapshotType type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;
	LLSD value = floater->getChild<LLUICtrl>("layer_types")->getValue();
	const std::string id = value.asString();
	if (id == "colors")
		type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;
	else if (id == "depth")
		type = LLViewerWindow::SNAPSHOT_TYPE_DEPTH;
	return type;
}

// static
void LLFloaterSnapshot::Impl::setResolution(LLFloaterSnapshot* floater, const std::string& comboname)
{
	LLComboBox* combo = floater->getChild<LLComboBox>(comboname);
		combo->setVisible(TRUE);
	updateResolution(combo, floater, FALSE); // to sync spinners with combo
}

//static 
void LLFloaterSnapshot::Impl::updateLayout(LLFloaterSnapshot* floaterp)
{
	LLSnapshotLivePreview* previewp = getPreviewView(floaterp);

	S32 delta_height = gSavedSettings.getBOOL("AdvanceSnapshot") ? 0 : floaterp->getUIWinHeightShort() - floaterp->getUIWinHeightLong() ;

	if(!gSavedSettings.getBOOL("AdvanceSnapshot")) //set to original window resolution
	{
		previewp->mKeepAspectRatio = TRUE;

		floaterp->getChild<LLComboBox>("postcard_size_combo")->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotPostcardLastResolution", 0);

		floaterp->getChild<LLComboBox>("texture_size_combo")->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotTextureLastResolution", 0);

		floaterp->getChild<LLComboBox>("local_size_combo")->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotLocalLastResolution", 0);

		LLSnapshotLivePreview* previewp = getPreviewView(floaterp);
		previewp->setSize(gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw());
	}

	bool use_freeze_frame = floaterp->getChild<LLUICtrl>("freeze_frame_check")->getValue().asBoolean();

	if (use_freeze_frame)
	{
		// stop all mouse events at fullscreen preview layer
		floaterp->getParent()->setMouseOpaque(TRUE);
		
		// shrink to smaller layout
		floaterp->reshape(floaterp->getRect().getWidth(), floaterp->getUIWinHeightLong() + delta_height);

		// can see and interact with fullscreen preview now
		if (previewp)
		{
			previewp->setVisible(TRUE);
			previewp->setEnabled(TRUE);
		}

		//RN: freeze all avatars
		LLCharacter* avatarp;
		for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
			iter != LLCharacter::sInstances.end(); ++iter)
		{
			avatarp = *iter;
			floaterp->impl.mAvatarPauseHandles.push_back(avatarp->requestPause());
		}

		// freeze everything else
		gSavedSettings.setBOOL("FreezeTime", TRUE);

		if (LLToolMgr::getInstance()->getCurrentToolset() != gCameraToolset)
		{
			floaterp->impl.mLastToolset = LLToolMgr::getInstance()->getCurrentToolset();
			LLToolMgr::getInstance()->setCurrentToolset(gCameraToolset);
		}
	}
	else // turning off freeze frame mode
	{
		floaterp->getParent()->setMouseOpaque(FALSE);
		floaterp->reshape(floaterp->getRect().getWidth(), floaterp->getUIWinHeightLong() + delta_height);
		if (previewp)
		{
			previewp->setVisible(FALSE);
			previewp->setEnabled(FALSE);
		}

		//RN: thaw all avatars
		floaterp->impl.mAvatarPauseHandles.clear();

		// thaw everything else
		gSavedSettings.setBOOL("FreezeTime", FALSE);

		// restore last tool (e.g. pie menu, etc)
		if (floaterp->impl.mLastToolset)
		{
			LLToolMgr::getInstance()->setCurrentToolset(floaterp->impl.mLastToolset);
		}
	}
}

// This is the main function that keeps all the GUI controls in sync with the saved settings.
// It should be called anytime a setting is changed that could affect the controls.
// No other methods should be changing any of the controls directly except for helpers called by this method.
// The basic pattern for programmatically changing the GUI settings is to first set the
// appropriate saved settings and then call this method to sync the GUI with them.
// static
void LLFloaterSnapshot::Impl::updateControls(LLFloaterSnapshot* floater)
{
	LLRadioGroup* snapshot_type_radio = floater->getChild<LLRadioGroup>("snapshot_type_radio");
	LLSnapshotLivePreview::ESnapshotType shot_type = (LLSnapshotLivePreview::ESnapshotType)gSavedSettings.getS32("LastSnapshotType");
	snapshot_type_radio->setSelectedByValue(getTypeName(shot_type), true);

	ESnapshotFormat shot_format = (ESnapshotFormat)gSavedSettings.getS32("SnapshotFormat");
	LLViewerWindow::ESnapshotType layer_type = getLayerType(floater);

	floater->getChildView("share_to_web")->setVisible( gSavedSettings.getBOOL("SnapshotSharingEnabled"));

	floater->getChildView("postcard_size_combo")->setVisible( FALSE);
	floater->getChildView("texture_size_combo")->setVisible( FALSE);
	floater->getChildView("local_size_combo")->setVisible( FALSE);

	floater->getChild<LLComboBox>("postcard_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotPostcardLastResolution"));
	floater->getChild<LLComboBox>("texture_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotTextureLastResolution"));
	floater->getChild<LLComboBox>("local_size_combo")->selectNthItem(gSavedSettings.getS32("SnapshotLocalLastResolution"));
	floater->getChild<LLComboBox>("local_format_combo")->selectNthItem(gSavedSettings.getS32("SnapshotFormat"));

	// *TODO: Separate settings for Web images from postcards
	floater->getChildView("send_btn")->setVisible(	shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD ||
													shot_type == LLSnapshotLivePreview::SNAPSHOT_WEB);
	floater->getChildView("upload_btn")->setVisible(shot_type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE);
	floater->getChildView("save_btn")->setVisible(	shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL);
	floater->getChildView("keep_aspect_check")->setEnabled(shot_type != LLSnapshotLivePreview::SNAPSHOT_TEXTURE && !floater->impl.mAspectRatioCheckOff);
	floater->getChildView("layer_types")->setEnabled(shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL);

	BOOL is_advance = gSavedSettings.getBOOL("AdvanceSnapshot");
	BOOL is_local = shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL;
	BOOL show_slider = (shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD ||
						shot_type == LLSnapshotLivePreview::SNAPSHOT_WEB ||
					   (is_local && shot_format == LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG));

	floater->getChildView("more_btn")->setVisible( !is_advance); // the only item hidden in advanced mode
	floater->getChildView("less_btn")->setVisible(				is_advance);
	floater->getChildView("type_label2")->setVisible(				is_advance);
	floater->getChildView("format_label")->setVisible(			is_advance && is_local);
	floater->getChildView("local_format_combo")->setVisible(		is_advance && is_local);
	floater->getChildView("layer_types")->setVisible(				is_advance);
	floater->getChildView("layer_type_label")->setVisible(		is_advance);
	floater->getChildView("snapshot_width")->setVisible(			is_advance);
	floater->getChildView("snapshot_height")->setVisible(			is_advance);
	floater->getChildView("keep_aspect_check")->setVisible(		is_advance);
	floater->getChildView("ui_check")->setVisible(				is_advance);
	floater->getChildView("hud_check")->setVisible(				is_advance);
	floater->getChildView("keep_open_check")->setVisible(			is_advance);
	floater->getChildView("freeze_frame_check")->setVisible(		is_advance);
	floater->getChildView("auto_snapshot_check")->setVisible(		is_advance);
	floater->getChildView("image_quality_slider")->setVisible(	is_advance && show_slider);

	if (gSavedSettings.getBOOL("RenderUIInSnapshot") || gSavedSettings.getBOOL("RenderHUDInSnapshot"))
	{ //clamp snapshot resolution to window size when showing UI or HUD in snapshot

		LLSpinCtrl* width_ctrl = floater->getChild<LLSpinCtrl>("snapshot_width");
		LLSpinCtrl* height_ctrl = floater->getChild<LLSpinCtrl>("snapshot_height");

		S32 width = gViewerWindow->getWindowWidthRaw();
		S32 height = gViewerWindow->getWindowHeightRaw();

		width_ctrl->setMaxValue(width);
		
		height_ctrl->setMaxValue(height);

		if (width_ctrl->getValue().asInteger() > width)
		{
			width_ctrl->forceSetValue(width);
		}
		if (height_ctrl->getValue().asInteger() > height)
		{
			height_ctrl->forceSetValue(height);
		}
	}
	else
	{ 
		LLSpinCtrl* width = floater->getChild<LLSpinCtrl>("snapshot_width");
		width->setMaxValue(6016);
		LLSpinCtrl* height = floater->getChild<LLSpinCtrl>("snapshot_height");
		height->setMaxValue(6016);
	}
		
	LLSnapshotLivePreview* previewp = getPreviewView(floater);
	BOOL got_bytes = previewp && previewp->getDataSize() > 0;
	BOOL got_snap = previewp && previewp->getSnapshotUpToDate();

	// *TODO: Separate maximum size for Web images from postcards
	floater->getChildView("send_btn")->setEnabled((shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD ||
											shot_type == LLSnapshotLivePreview::SNAPSHOT_WEB) &&
											got_snap && previewp->getDataSize() <= MAX_POSTCARD_DATASIZE);
	floater->getChildView("upload_btn")->setEnabled(shot_type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE  && got_snap);
	floater->getChildView("save_btn")->setEnabled(shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL    && got_snap);

	LLLocale locale(LLLocale::USER_LOCALE);
	std::string bytes_string;
	if (got_snap)
	{
		LLResMgr::getInstance()->getIntegerString(bytes_string, (previewp->getDataSize()) >> 10 );
	}
	S32 upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
	floater->getChild<LLUICtrl>("texture")->setLabelArg("[AMOUNT]", llformat("%d",upload_cost));
	floater->getChild<LLUICtrl>("upload_btn")->setLabelArg("[AMOUNT]", llformat("%d",upload_cost));
	floater->getChild<LLUICtrl>("file_size_label")->setTextArg("[SIZE]", got_snap ? bytes_string : floater->getString("unknown"));
	floater->getChild<LLUICtrl>("file_size_label")->setColor(
		shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD 
		&& got_bytes
		&& previewp->getDataSize() > MAX_POSTCARD_DATASIZE ? LLUIColor(LLColor4::red) : LLUIColorTable::instance().getColor( "LabelTextColor" ));

	switch(shot_type)
	{
	  // *TODO: Separate settings for Web images from postcards
	  case LLSnapshotLivePreview::SNAPSHOT_WEB:
	  case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
		layer_type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;
		floater->getChild<LLUICtrl>("layer_types")->setValue("colors");
		if(is_advance)
		{			
			setResolution(floater, "postcard_size_combo");
		}
		break;
	  case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
		layer_type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;
		floater->getChild<LLUICtrl>("layer_types")->setValue("colors");
		if(is_advance)
		{
			setResolution(floater, "texture_size_combo");			
		}
		break;
	  case  LLSnapshotLivePreview::SNAPSHOT_LOCAL:
		if(is_advance)
		{
			setResolution(floater, "local_size_combo");
		}
		break;
	  default:
		break;
	}

	updateResolutionTextEntry(floater);

	if (previewp)
	{
		previewp->setSnapshotType(shot_type);
		previewp->setSnapshotFormat(shot_format);
		previewp->setSnapshotBufferType(layer_type);
	}
}

// static
void LLFloaterSnapshot::Impl::updateResolutionTextEntry(LLFloaterSnapshot* floater)
{
	LLSpinCtrl* width_spinner = floater->getChild<LLSpinCtrl>("snapshot_width");
	LLSpinCtrl* height_spinner = floater->getChild<LLSpinCtrl>("snapshot_height");

	if(getTypeIndex(floater) == LLSnapshotLivePreview::SNAPSHOT_TEXTURE)
	{
		width_spinner->setAllowEdit(FALSE);
		height_spinner->setAllowEdit(FALSE);
	}
	else
	{
		width_spinner->setAllowEdit(TRUE);
		height_spinner->setAllowEdit(TRUE);
	}
}

// static
void LLFloaterSnapshot::Impl::checkAutoSnapshot(LLSnapshotLivePreview* previewp, BOOL update_thumbnail)
{
	if (previewp)
	{
		BOOL autosnap = gSavedSettings.getBOOL("AutoSnapshot");
		previewp->updateSnapshot(autosnap, update_thumbnail, autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickDiscard(void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	
	if (view)
	{
		view->closeFloater();
	}
}


// static
void LLFloaterSnapshot::Impl::onCommitSave(LLUICtrl* ctrl, void* data)
{
	if (ctrl->getValue().asString() == "save as")
	{
		gViewerWindow->resetSnapshotLoc();
	}
	onClickKeep(data);
}

// static
void LLFloaterSnapshot::Impl::onClickKeep(void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	LLSnapshotLivePreview* previewp = getPreviewView(view);

	if (previewp)
	{
		switch (previewp->getSnapshotType())
		{
		  case LLSnapshotLivePreview::SNAPSHOT_WEB:
			previewp->saveWeb();
			break;

		  case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
			{
				LLFloaterPostcard* floater = previewp->savePostcard();
				// if still in snapshot mode, put postcard floater in snapshot floaterview
				// and link it to snapshot floater
				if (floater && !gSavedSettings.getBOOL("CloseSnapshotOnKeep"))
				{
					gFloaterView->removeChild(floater);
					gSnapshotFloaterView->addChild(floater);
					view->addDependentFloater(floater, FALSE);
				}
			}
			break;

		  case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
			previewp->saveTexture();
			break;

		  case LLSnapshotLivePreview::SNAPSHOT_LOCAL:
			previewp->saveLocal();
			break;

		  default:
			break;
		}

		if (gSavedSettings.getBOOL("CloseSnapshotOnKeep"))
		{
			view->closeFloater();
		}
		else
		{
			checkAutoSnapshot(previewp);
		}

		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickNewSnapshot(void* data)
{
	LLSnapshotLivePreview* previewp = getPreviewView((LLFloaterSnapshot *)data);
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (previewp && view)
	{
		previewp->updateSnapshot(TRUE);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickAutoSnap(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "AutoSnapshot", check->get() );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		checkAutoSnapshot(getPreviewView(view));
		updateControls(view);
	}
}

void LLFloaterSnapshot::Impl::onClickMore(void* data)
{
	gSavedSettings.setBOOL( "AdvanceSnapshot", TRUE );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		view->translate( 0, view->getUIWinHeightShort() - view->getUIWinHeightLong() );
		view->reshape(view->getRect().getWidth(), view->getUIWinHeightLong());
		updateControls(view) ;
		updateLayout(view) ;
		if(getPreviewView(view))
		{
			getPreviewView(view)->setThumbnailImageSize() ;
		}
	}
}
void LLFloaterSnapshot::Impl::onClickLess(void* data)
{
	gSavedSettings.setBOOL( "AdvanceSnapshot", FALSE );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		view->translate( 0, view->getUIWinHeightLong() - view->getUIWinHeightShort() );
		view->reshape(view->getRect().getWidth(), view->getUIWinHeightShort());
		updateControls(view) ;
		updateLayout(view) ;
		if(getPreviewView(view))
		{
			getPreviewView(view)->setThumbnailImageSize() ;
		}
	}
}

// static
void LLFloaterSnapshot::Impl::onClickUICheck(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "RenderUIInSnapshot", check->get() );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		checkAutoSnapshot(getPreviewView(view), TRUE);
		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickHUDCheck(LLUICtrl *ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "RenderHUDInSnapshot", check->get() );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		checkAutoSnapshot(getPreviewView(view), TRUE);
		updateControls(view);
	}
}

// static
void LLFloaterSnapshot::Impl::onClickKeepOpenCheck(LLUICtrl* ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;

	gSavedSettings.setBOOL( "CloseSnapshotOnKeep", !check->get() );
}

// static
void LLFloaterSnapshot::Impl::onClickKeepAspectCheck(LLUICtrl* ctrl, void* data)
{
	LLCheckBoxCtrl *check = (LLCheckBoxCtrl *)ctrl;
	gSavedSettings.setBOOL( "KeepAspectForSnapshot", check->get() );
	
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		LLSnapshotLivePreview* previewp = getPreviewView(view) ;
		if(previewp)
		{
			previewp->mKeepAspectRatio = gSavedSettings.getBOOL("KeepAspectForSnapshot") ;

			S32 w, h ;
			previewp->getSize(w, h) ;
			if(checkImageSize(previewp, w, h, TRUE, previewp->getMaxImageSize()))
			{
				resetSnapshotSizeOnUI(view, w, h) ;
			}

			previewp->setSize(w, h) ;
			previewp->updateSnapshot(FALSE, TRUE);
			checkAutoSnapshot(previewp, TRUE);
		}
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitQuality(LLUICtrl* ctrl, void* data)
{
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	S32 quality_val = llfloor((F32)slider->getValue().asReal());

	LLSnapshotLivePreview* previewp = getPreviewView((LLFloaterSnapshot *)data);
	if (previewp)
	{
		previewp->setSnapshotQuality(quality_val);
	}
	checkAutoSnapshot(previewp, TRUE);
}

// static
void LLFloaterSnapshot::Impl::onCommitFreezeFrame(LLUICtrl* ctrl, void* data)
{
	LLCheckBoxCtrl* check_box = (LLCheckBoxCtrl*)ctrl;
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
		
	if (!view || !check_box)
	{
		return;
	}

	gSavedSettings.setBOOL("UseFreezeFrame", check_box->get());

	updateLayout(view);
}

// static
void LLFloaterSnapshot::Impl::checkAspectRatio(LLFloaterSnapshot *view, S32 index)
{
	LLSnapshotLivePreview *previewp = getPreviewView(view) ;

	// Don't round texture sizes; textures are commonly stretched in world, profiles, etc and need to be "squashed" during upload, not cropped here
#if 0
	if(LLSnapshotLivePreview::SNAPSHOT_TEXTURE == getTypeIndex(view))
	{
		previewp->mKeepAspectRatio = FALSE ;
		return ;
	}
#endif
	
	if(0 == index) //current window size
	{
		view->impl.mAspectRatioCheckOff = true ;
		view->getChildView("keep_aspect_check")->setEnabled(FALSE) ;

		if(previewp)
		{
			previewp->mKeepAspectRatio = TRUE ;
		}
	}
	else if(-1 == index) //custom
	{
		view->impl.mAspectRatioCheckOff = false ;
		//if(LLSnapshotLivePreview::SNAPSHOT_TEXTURE != gSavedSettings.getS32("LastSnapshotType"))
		{
			view->getChildView("keep_aspect_check")->setEnabled(TRUE) ;

			if(previewp)
			{
				previewp->mKeepAspectRatio = gSavedSettings.getBOOL("KeepAspectForSnapshot") ;
			}
		}
	}
	else
	{
		view->impl.mAspectRatioCheckOff = true ;
		view->getChildView("keep_aspect_check")->setEnabled(FALSE) ;

		if(previewp)
		{
			previewp->mKeepAspectRatio = FALSE ;
		}
	}

	return ;
}

static std::string lastSnapshotWidthName()
{
	switch(gSavedSettings.getS32("LastSnapshotType"))
	{
	  // *TODO: Separate settings for Web snapshots and postcards
	  case LLSnapshotLivePreview::SNAPSHOT_WEB:		 return "LastSnapshotToEmailWidth";
	  case LLSnapshotLivePreview::SNAPSHOT_POSTCARD: return "LastSnapshotToEmailWidth";
	  case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:  return "LastSnapshotToInventoryWidth";
	  default:                                       return "LastSnapshotToDiskWidth";
	}
}
static std::string lastSnapshotHeightName()
{
	switch(gSavedSettings.getS32("LastSnapshotType"))
	{
	  // *TODO: Separate settings for Web snapshots and postcards
	  case LLSnapshotLivePreview::SNAPSHOT_WEB:	     return "LastSnapshotToEmailHeight";
	  case LLSnapshotLivePreview::SNAPSHOT_POSTCARD: return "LastSnapshotToEmailHeight";
	  case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:  return "LastSnapshotToInventoryHeight";
	  default:                                       return "LastSnapshotToDiskHeight";
	}
}

// static
void LLFloaterSnapshot::Impl::updateResolution(LLUICtrl* ctrl, void* data, BOOL do_update)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
		
	if (!view || !combobox)
	{
		return;
	}

	// save off all selected resolution values
	gSavedSettings.setS32("SnapshotPostcardLastResolution", view->getChild<LLComboBox>("postcard_size_combo")->getCurrentIndex());
	gSavedSettings.setS32("SnapshotTextureLastResolution",  view->getChild<LLComboBox>("texture_size_combo")->getCurrentIndex());
	gSavedSettings.setS32("SnapshotLocalLastResolution",    view->getChild<LLComboBox>("local_size_combo")->getCurrentIndex());

	std::string sdstring = combobox->getSelectedValue();
	LLSD sdres;
	std::stringstream sstream(sdstring);
	LLSDSerialize::fromNotation(sdres, sstream, sdstring.size());
		
	S32 width = sdres[0];
	S32 height = sdres[1];
	
	LLSnapshotLivePreview* previewp = getPreviewView(view);
	if (previewp && combobox->getCurrentIndex() >= 0)
	{
		S32 original_width = 0 , original_height = 0 ;
		previewp->getSize(original_width, original_height) ;
		
		if (width == 0 || height == 0)
		{
			// take resolution from current window size
			previewp->setSize(gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw());
		}
		else if (width == -1 || height == -1)
		{
			// load last custom value
			previewp->setSize(gSavedSettings.getS32(lastSnapshotWidthName()), gSavedSettings.getS32(lastSnapshotHeightName()));
		}
		else
		{
			// use the resolution from the selected pre-canned drop-down choice
			previewp->setSize(width, height);
		}

		checkAspectRatio(view, width) ;

		previewp->getSize(width, height);
	
		if (gSavedSettings.getBOOL("RenderUIInSnapshot") || gSavedSettings.getBOOL("RenderHUDInSnapshot"))
		{ //clamp snapshot resolution to window size when showing UI or HUD in snapshot
			width = llmin(width, gViewerWindow->getWindowWidthRaw());
			height = llmin(height, gViewerWindow->getWindowHeightRaw());
		}

		
		if(checkImageSize(previewp, width, height, TRUE, previewp->getMaxImageSize()))
		{
			resetSnapshotSizeOnUI(view, width, height) ;
		}
		
		if(view->getChild<LLUICtrl>("snapshot_width")->getValue().asInteger() != width || view->getChild<LLUICtrl>("snapshot_height")->getValue().asInteger() != height)
		{
			view->getChild<LLUICtrl>("snapshot_width")->setValue(width);
			view->getChild<LLUICtrl>("snapshot_height")->setValue(height);
		}

		if(original_width != width || original_height != height)
		{
			previewp->setSize(width, height);

			// hide old preview as the aspect ratio could be wrong
			checkAutoSnapshot(previewp, FALSE);
			getPreviewView(view)->updateSnapshot(FALSE, TRUE);
			if(do_update)
			{
				updateControls(view);
			}
		}
	}
}

// static
void LLFloaterSnapshot::Impl::onCommitLayerTypes(LLUICtrl* ctrl, void*data)
{
	LLComboBox* combobox = (LLComboBox*)ctrl;

	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
		
	if (view)
	{
		LLSnapshotLivePreview* previewp = getPreviewView(view);
		if (previewp)
		{
			previewp->setSnapshotBufferType((LLViewerWindow::ESnapshotType)combobox->getCurrentIndex());
		}
		checkAutoSnapshot(previewp, TRUE);
	}
}

//static 
void LLFloaterSnapshot::Impl::onCommitSnapshotType(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		gSavedSettings.setS32("LastSnapshotType", getTypeIndex(view));
		getPreviewView(view)->updateSnapshot(TRUE);
		updateControls(view);
	}
}


//static 
void LLFloaterSnapshot::Impl::onCommitSnapshotFormat(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;
	if (view)
	{
		gSavedSettings.setS32("SnapshotFormat", getFormatIndex(view));
		getPreviewView(view)->updateSnapshot(TRUE);
		updateControls(view);
	}
}



// Sets the named size combo to "custom" mode.
// static
void LLFloaterSnapshot::Impl::comboSetCustom(LLFloaterSnapshot* floater, const std::string& comboname)
{
	LLComboBox* combo = floater->getChild<LLComboBox>(comboname);

	combo->setCurrentByIndex(combo->getItemCount() - 1); // "custom" is always the last index

	if(comboname == "postcard_size_combo") 
	{
		gSavedSettings.setS32("SnapshotPostcardLastResolution", combo->getCurrentIndex());
	}
	else if(comboname == "texture_size_combo") 
	{
		gSavedSettings.setS32("SnapshotTextureLastResolution", combo->getCurrentIndex());
	}
	else if(comboname == "local_size_combo") 
	{
		gSavedSettings.setS32("SnapshotLocalLastResolution", combo->getCurrentIndex());
	}

	checkAspectRatio(floater, -1); // -1 means custom
}



//static
BOOL LLFloaterSnapshot::Impl::checkImageSize(LLSnapshotLivePreview* previewp, S32& width, S32& height, BOOL isWidthChanged, S32 max_value)
{
	S32 w = width ;
	S32 h = height ;

	//if texture, ignore aspect ratio setting, round image size to power of 2.
#if 0 // Don't round texture sizes; textures are commonly stretched in world, profiles, etc and need to be "squashed" during upload, not cropped here
	if(LLSnapshotLivePreview::SNAPSHOT_TEXTURE == gSavedSettings.getS32("LastSnapshotType"))
	{
		if(width > max_value)
		{
			width = max_value ;
		}
		if(height > max_value)
		{
			height = max_value ;
		}

		//round to nearest power of 2 based on the direction of movement
		// i.e. higher power of two if increasing texture resolution
		if(gSavedSettings.getS32("LastSnapshotToInventoryWidth") < width ||
			gSavedSettings.getS32("LastSnapshotToInventoryHeight") < height)
		{
			// Up arrow pressed
			width = get_next_power_two(width, MAX_TEXTURE_SIZE) ;
			height = get_next_power_two(height, MAX_TEXTURE_SIZE) ;
		}
		else
		{
			// Down or no change
			width = get_lower_power_two(width, MAX_TEXTURE_SIZE) ;
			height = get_lower_power_two(height, MAX_TEXTURE_SIZE) ;
		}
	}
	else
#endif
	if(previewp && previewp->mKeepAspectRatio)
	{
		if(gViewerWindow->getWindowWidthRaw() < 1 || gViewerWindow->getWindowHeightRaw() < 1)
		{
			return FALSE ;
		}

		//aspect ratio of the current window
		F32 aspect_ratio = (F32)gViewerWindow->getWindowWidthRaw() / gViewerWindow->getWindowHeightRaw() ;

		//change another value proportionally
		if(isWidthChanged)
		{
			height = (S32)(width / aspect_ratio) ;
		}
		else
		{
			width = (S32)(height * aspect_ratio) ;
		}

		//bound w/h by the max_value
		if(width > max_value || height > max_value)
		{
			if(width > height)
			{
				width = max_value ;
				height = (S32)(width / aspect_ratio) ;
			}
			else
			{
				height = max_value ;
				width = (S32)(height * aspect_ratio) ;
			}
		}
	}
	else
	{
	}

	return (w != width || h != height) ;
}

//static
void LLFloaterSnapshot::Impl::resetSnapshotSizeOnUI(LLFloaterSnapshot *view, S32 width, S32 height)
{
	view->getChild<LLSpinCtrl>("snapshot_width")->forceSetValue(width);
	view->getChild<LLSpinCtrl>("snapshot_height")->forceSetValue(height);
	gSavedSettings.setS32(lastSnapshotWidthName(), width);
	gSavedSettings.setS32(lastSnapshotHeightName(), height);
}

//static
void LLFloaterSnapshot::Impl::onCommitCustomResolution(LLUICtrl *ctrl, void* data)
{
	LLFloaterSnapshot *view = (LLFloaterSnapshot *)data;		
	if (view)
	{
		S32 w = llfloor((F32)view->getChild<LLUICtrl>("snapshot_width")->getValue().asReal());
		S32 h = llfloor((F32)view->getChild<LLUICtrl>("snapshot_height")->getValue().asReal());

		LLSnapshotLivePreview* previewp = getPreviewView(view);
		if (previewp)
		{
			S32 curw,curh;
			previewp->getSize(curw, curh);
			
			if (w != curw || h != curh)
			{
				BOOL update_ = FALSE ;
				//if to upload a snapshot, process spinner input in a special way.
#if 0  // Don't round texture sizes; textures are commonly stretched in world, profiles, etc and need to be "squashed" during upload, not cropped here
				if(LLSnapshotLivePreview::SNAPSHOT_TEXTURE == gSavedSettings.getS32("LastSnapshotType"))
				{
					S32 spinner_increment = (S32)((LLSpinCtrl*)ctrl)->getIncrement() ;
					S32 dw = w - curw ;
					S32 dh = h - curh ;
					dw = (dw == spinner_increment) ? 1 : ((dw == -spinner_increment) ? -1 : 0) ;
					dh = (dh == spinner_increment) ? 1 : ((dh == -spinner_increment) ? -1 : 0) ;

					if(dw)
					{
						w = (dw > 0) ? curw << dw : curw >> -dw ;
						update_ = TRUE ;
					}
					if(dh)
					{
						h = (dh > 0) ? curh << dh : curh >> -dh ;
						update_ = TRUE ;
					}
				}
#endif
				previewp->setMaxImageSize((S32)((LLSpinCtrl *)ctrl)->getMaxValue()) ;
				
				// Check image size changes the value of height and width
				if(checkImageSize(previewp, w, h, w != curw, previewp->getMaxImageSize())
					|| update_)
				{
					resetSnapshotSizeOnUI(view, w, h) ;
				}

				previewp->setSize(w,h);
				checkAutoSnapshot(previewp, FALSE);
				previewp->updateSnapshot(FALSE, TRUE);
				comboSetCustom(view, "postcard_size_combo");
				comboSetCustom(view, "texture_size_combo");
				comboSetCustom(view, "local_size_combo");
			}
		}

		gSavedSettings.setS32(lastSnapshotWidthName(), w);
		gSavedSettings.setS32(lastSnapshotHeightName(), h);

		updateControls(view);
	}
}

///----------------------------------------------------------------------------
/// Class LLFloaterSnapshot
///----------------------------------------------------------------------------

// Default constructor
LLFloaterSnapshot::LLFloaterSnapshot(const LLSD& key)
	: LLFloater(key),
	  impl (*(new Impl))
{
}

// Destroys the object
LLFloaterSnapshot::~LLFloaterSnapshot()
{
	LLView::deleteViewByHandle(impl.mPreviewHandle);

	//unfreeze everything else
	gSavedSettings.setBOOL("FreezeTime", FALSE);

	if (impl.mLastToolset)
	{
		LLToolMgr::getInstance()->setCurrentToolset(impl.mLastToolset);
	}

	delete &impl;
}


BOOL LLFloaterSnapshot::postBuild()
{
	// Kick start Web Sharing, to fetch its config data if it needs to.
	if (gSavedSettings.getBOOL("SnapshotSharingEnabled"))
	{
		LLWebSharing::instance().init();
	}

	childSetCommitCallback("snapshot_type_radio", Impl::onCommitSnapshotType, this);
	childSetCommitCallback("local_format_combo", Impl::onCommitSnapshotFormat, this);
	
	childSetAction("new_snapshot_btn", Impl::onClickNewSnapshot, this);

	childSetAction("more_btn", Impl::onClickMore, this);
	childSetAction("less_btn", Impl::onClickLess, this);

	childSetAction("upload_btn", Impl::onClickKeep, this);
	childSetAction("send_btn", Impl::onClickKeep, this);
	childSetCommitCallback("save_btn", Impl::onCommitSave, this);
	childSetAction("discard_btn", Impl::onClickDiscard, this);

	childSetCommitCallback("image_quality_slider", Impl::onCommitQuality, this);
	getChild<LLUICtrl>("image_quality_slider")->setValue(gSavedSettings.getS32("SnapshotQuality"));

	childSetCommitCallback("snapshot_width", Impl::onCommitCustomResolution, this);
	childSetCommitCallback("snapshot_height", Impl::onCommitCustomResolution, this);

	childSetCommitCallback("ui_check", Impl::onClickUICheck, this);
	getChild<LLUICtrl>("ui_check")->setValue(gSavedSettings.getBOOL("RenderUIInSnapshot"));

	childSetCommitCallback("hud_check", Impl::onClickHUDCheck, this);
	getChild<LLUICtrl>("hud_check")->setValue(gSavedSettings.getBOOL("RenderHUDInSnapshot"));

	childSetCommitCallback("keep_open_check", Impl::onClickKeepOpenCheck, this);
	getChild<LLUICtrl>("keep_open_check")->setValue(!gSavedSettings.getBOOL("CloseSnapshotOnKeep"));

	childSetCommitCallback("keep_aspect_check", Impl::onClickKeepAspectCheck, this);
	getChild<LLUICtrl>("keep_aspect_check")->setValue(gSavedSettings.getBOOL("KeepAspectForSnapshot"));

	childSetCommitCallback("layer_types", Impl::onCommitLayerTypes, this);
	getChild<LLUICtrl>("layer_types")->setValue("colors");
	getChildView("layer_types")->setEnabled(FALSE);

	getChild<LLUICtrl>("snapshot_width")->setValue(gSavedSettings.getS32(lastSnapshotWidthName()));
	getChild<LLUICtrl>("snapshot_height")->setValue(gSavedSettings.getS32(lastSnapshotHeightName()));

	getChild<LLUICtrl>("freeze_frame_check")->setValue(gSavedSettings.getBOOL("UseFreezeFrame"));
	childSetCommitCallback("freeze_frame_check", Impl::onCommitFreezeFrame, this);

	getChild<LLUICtrl>("auto_snapshot_check")->setValue(gSavedSettings.getBOOL("AutoSnapshot"));
	childSetCommitCallback("auto_snapshot_check", Impl::onClickAutoSnap, this);

	childSetCommitCallback("postcard_size_combo", Impl::onCommitResolution, this);
	childSetCommitCallback("texture_size_combo", Impl::onCommitResolution, this);
	childSetCommitCallback("local_size_combo", Impl::onCommitResolution, this);

	// create preview window
	LLRect full_screen_rect = getRootView()->getRect();
	LLSnapshotLivePreview::Params p;
	p.rect(full_screen_rect);
	LLSnapshotLivePreview* previewp = new LLSnapshotLivePreview(p);
	LLView* parent_view = gSnapshotFloaterView->getParent();
	
	parent_view->removeChild(gSnapshotFloaterView);
	// make sure preview is below snapshot floater
	parent_view->addChild(previewp);
	parent_view->addChild(gSnapshotFloaterView);
	
	//move snapshot floater to special purpose snapshotfloaterview
	gFloaterView->removeChild(this);
	gSnapshotFloaterView->addChild(this);

	impl.mPreviewHandle = previewp->getHandle();
	impl.updateControls(this);
	impl.updateLayout(this);
	
	return TRUE;
}

void LLFloaterSnapshot::draw()
{
	LLSnapshotLivePreview* previewp = impl.getPreviewView(this);

	if (previewp && (previewp->isSnapshotActive() || previewp->getThumbnailLock()))
	{
		// don't render snapshot window in snapshot, even if "show ui" is turned on
		return;
	}

	LLFloater::draw();

	if (previewp)
	{		
		if(previewp->getThumbnailImage())
		{
			LLRect thumbnail_rect = getChild<LLUICtrl>("thumbnail_placeholder")->getRect();

			S32 offset_x = (getRect().getWidth() - previewp->getThumbnailWidth()) / 2 ;
			S32 offset_y = thumbnail_rect.mBottom + (thumbnail_rect.getHeight() - previewp->getThumbnailHeight()) / 2 ;

			glMatrixMode(GL_MODELVIEW);
			// Apply floater transparency to the texture unless the floater is focused.
			F32 alpha = getTransparencyType() == TT_ACTIVE ? 1.0f : getCurrentTransparency();
			gl_draw_scaled_image(offset_x, offset_y, 
					previewp->getThumbnailWidth(), previewp->getThumbnailHeight(), 
					previewp->getThumbnailImage(), LLColor4::white % alpha);

			previewp->drawPreviewRect(offset_x, offset_y) ;
		}
	}
}

void LLFloaterSnapshot::onOpen(const LLSD& key)
{
	LLSnapshotLivePreview* preview = LLFloaterSnapshot::Impl::getPreviewView(this);
	if(preview)
	{
		preview->updateSnapshot(TRUE);
	}
	focusFirstItem(FALSE);
	gSnapshotFloaterView->setEnabled(TRUE);
	gSnapshotFloaterView->setVisible(TRUE);
	gSnapshotFloaterView->adjustToFitScreen(this, FALSE);
}

void LLFloaterSnapshot::onClose(bool app_quitting)
{
	getParent()->setMouseOpaque(FALSE);
}


//static 
void LLFloaterSnapshot::update()
{
	LLFloaterSnapshot* inst = LLFloaterReg::findTypedInstance<LLFloaterSnapshot>("snapshot");
	if (!inst)
		return;
	
	BOOL changed = FALSE;
	for (std::set<LLSnapshotLivePreview*>::iterator iter = LLSnapshotLivePreview::sList.begin();
		 iter != LLSnapshotLivePreview::sList.end(); ++iter)
	{
		changed |= LLSnapshotLivePreview::onIdle(*iter);
	}
	if(changed)
	{
		inst->impl.updateControls(inst);
	}
}


///----------------------------------------------------------------------------
/// Class LLSnapshotFloaterView
///----------------------------------------------------------------------------

LLSnapshotFloaterView::LLSnapshotFloaterView (const Params& p) : LLFloaterView (p)
{
}

LLSnapshotFloaterView::~LLSnapshotFloaterView()
{
}

BOOL LLSnapshotFloaterView::handleKey(KEY key, MASK mask, BOOL called_from_parent)
{
	// use default handler when not in freeze-frame mode
	if(!gSavedSettings.getBOOL("FreezeTime"))
	{
		return LLFloaterView::handleKey(key, mask, called_from_parent);
	}

	if (called_from_parent)
	{
		// pass all keystrokes down
		LLFloaterView::handleKey(key, mask, called_from_parent);
	}
	else
	{
		// bounce keystrokes back down
		LLFloaterView::handleKey(key, mask, TRUE);
	}
	return TRUE;
}

BOOL LLSnapshotFloaterView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// use default handler when not in freeze-frame mode
	if(!gSavedSettings.getBOOL("FreezeTime"))
	{
		return LLFloaterView::handleMouseDown(x, y, mask);
	}
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleMouseDown(x, y, mask) == NULL)
	{
		LLToolMgr::getInstance()->getCurrentTool()->handleMouseDown( x, y, mask );
	}
	return TRUE;
}

BOOL LLSnapshotFloaterView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// use default handler when not in freeze-frame mode
	if(!gSavedSettings.getBOOL("FreezeTime"))
	{
		return LLFloaterView::handleMouseUp(x, y, mask);
	}
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleMouseUp(x, y, mask) == NULL)
	{
		LLToolMgr::getInstance()->getCurrentTool()->handleMouseUp( x, y, mask );
	}
	return TRUE;
}

BOOL LLSnapshotFloaterView::handleHover(S32 x, S32 y, MASK mask)
{
	// use default handler when not in freeze-frame mode
	if(!gSavedSettings.getBOOL("FreezeTime"))
	{
		return LLFloaterView::handleHover(x, y, mask);
	}	
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleHover(x, y, mask) == NULL)
	{
		LLToolMgr::getInstance()->getCurrentTool()->handleHover( x, y, mask );
	}
	return TRUE;
}
