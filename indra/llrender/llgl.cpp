/** 
 * @file llgl.cpp
 * @brief LLGL implementation
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

// This file sets some global GL parameters, and implements some 
// useful functions for GL operations.

#define GLH_EXT_SINGLE_FILE

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llsys.h"

#include "llgl.h"
#include "llrender.h"

#include "llerror.h"
#include "llerrorcontrol.h"
#include "llquaternion.h"
#include "llmath.h"
#include "m4math.h"
#include "llstring.h"
#include "llmemtype.h"
#include "llstacktrace.h"

#include "llglheaders.h"

#ifdef _DEBUG
//#define GL_STATE_VERIFY
#endif


BOOL gDebugSession = FALSE;
BOOL gDebugGL = FALSE;
BOOL gClothRipple = FALSE;
BOOL gHeadlessClient = FALSE;
BOOL gGLActive = FALSE;

static const std::string HEADLESS_VENDOR_STRING("Linden Lab");
static const std::string HEADLESS_RENDERER_STRING("Headless");
static const std::string HEADLESS_VERSION_STRING("1.0");

std::ofstream gFailLog;

void ll_init_fail_log(std::string filename)
{
	gFailLog.open(filename.c_str());
}


void ll_fail(std::string msg)
{
	
	if (gDebugSession)
	{
		std::vector<std::string> lines;

		gFailLog << LLError::utcTime() << " " << msg << std::endl;

		gFailLog << "Stack Trace:" << std::endl;

		ll_get_stack_trace(lines);
		
		for(size_t i = 0; i < lines.size(); ++i)
		{
			gFailLog << lines[i] << std::endl;
		}

		gFailLog << "End of Stack Trace." << std::endl << std::endl;

		gFailLog.flush();
	}
};

void ll_close_fail_log()
{
	gFailLog.close();
}

LLMatrix4 gGLObliqueProjectionInverse;

#define LL_GL_NAME_POOLING 0

std::list<LLGLUpdate*> LLGLUpdate::sGLQ;

#if (LL_WINDOWS || LL_LINUX || LL_SOLARIS)  && !LL_MESA_HEADLESS
// ATI prototypes
// vertex blending prototypes
PFNGLWEIGHTPOINTERARBPROC			glWeightPointerARB = NULL;
PFNGLVERTEXBLENDARBPROC				glVertexBlendARB = NULL;
PFNGLWEIGHTFVARBPROC				glWeightfvARB = NULL;

// Vertex buffer object prototypes
PFNGLBINDBUFFERARBPROC				glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC			glDeleteBuffersARB = NULL;
PFNGLGENBUFFERSARBPROC				glGenBuffersARB = NULL;
PFNGLISBUFFERARBPROC				glIsBufferARB = NULL;
PFNGLBUFFERDATAARBPROC				glBufferDataARB = NULL;
PFNGLBUFFERSUBDATAARBPROC			glBufferSubDataARB = NULL;
PFNGLGETBUFFERSUBDATAARBPROC		glGetBufferSubDataARB = NULL;
PFNGLMAPBUFFERARBPROC				glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC				glUnmapBufferARB = NULL;
PFNGLGETBUFFERPARAMETERIVARBPROC	glGetBufferParameterivARB = NULL;
PFNGLGETBUFFERPOINTERVARBPROC		glGetBufferPointervARB = NULL;

// vertex object prototypes
PFNGLNEWOBJECTBUFFERATIPROC			glNewObjectBufferATI = NULL;
PFNGLISOBJECTBUFFERATIPROC			glIsObjectBufferATI = NULL;
PFNGLUPDATEOBJECTBUFFERATIPROC		glUpdateObjectBufferATI = NULL;
PFNGLGETOBJECTBUFFERFVATIPROC		glGetObjectBufferfvATI = NULL;
PFNGLGETOBJECTBUFFERIVATIPROC		glGetObjectBufferivATI = NULL;
PFNGLFREEOBJECTBUFFERATIPROC		glFreeObjectBufferATI = NULL;
PFNGLARRAYOBJECTATIPROC				glArrayObjectATI = NULL;
PFNGLVERTEXATTRIBARRAYOBJECTATIPROC	glVertexAttribArrayObjectATI = NULL;
PFNGLGETARRAYOBJECTFVATIPROC		glGetArrayObjectfvATI = NULL;
PFNGLGETARRAYOBJECTIVATIPROC		glGetArrayObjectivATI = NULL;
PFNGLVARIANTARRAYOBJECTATIPROC		glVariantObjectArrayATI = NULL;
PFNGLGETVARIANTARRAYOBJECTFVATIPROC	glGetVariantArrayObjectfvATI = NULL;
PFNGLGETVARIANTARRAYOBJECTIVATIPROC	glGetVariantArrayObjectivATI = NULL;

// GL_ARB_occlusion_query
PFNGLGENQUERIESARBPROC glGenQueriesARB = NULL;
PFNGLDELETEQUERIESARBPROC glDeleteQueriesARB = NULL;
PFNGLISQUERYARBPROC glIsQueryARB = NULL;
PFNGLBEGINQUERYARBPROC glBeginQueryARB = NULL;
PFNGLENDQUERYARBPROC glEndQueryARB = NULL;
PFNGLGETQUERYIVARBPROC glGetQueryivARB = NULL;
PFNGLGETQUERYOBJECTIVARBPROC glGetQueryObjectivARB = NULL;
PFNGLGETQUERYOBJECTUIVARBPROC glGetQueryObjectuivARB = NULL;

// GL_ARB_point_parameters
PFNGLPOINTPARAMETERFARBPROC glPointParameterfARB = NULL;
PFNGLPOINTPARAMETERFVARBPROC glPointParameterfvARB = NULL;

// GL_EXT_framebuffer_object
PFNGLISRENDERBUFFEREXTPROC glIsRenderbufferEXT = NULL;
PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = NULL;
PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT = NULL;
PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = NULL;
PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = NULL;
PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC glGetRenderbufferParameterivEXT = NULL;
PFNGLISFRAMEBUFFEREXTPROC glIsFramebufferEXT = NULL;
PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = NULL;
PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT = NULL;
PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = NULL;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT = NULL;
PFNGLFRAMEBUFFERTEXTURE1DEXTPROC glFramebufferTexture1DEXT = NULL;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT = NULL;
PFNGLFRAMEBUFFERTEXTURE3DEXTPROC glFramebufferTexture3DEXT = NULL;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = NULL;
PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC glGetFramebufferAttachmentParameterivEXT = NULL;
PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT = NULL;

// GL_EXT_framebuffer_multisample
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT = NULL;

// GL_EXT_framebuffer_blit
PFNGLBLITFRAMEBUFFEREXTPROC glBlitFramebufferEXT = NULL;

// GL_EXT_blend_func_separate
PFNGLBLENDFUNCSEPARATEEXTPROC glBlendFuncSeparateEXT = NULL;

// GL_ARB_draw_buffers
PFNGLDRAWBUFFERSARBPROC glDrawBuffersARB = NULL;

//shader object prototypes
PFNGLDELETEOBJECTARBPROC glDeleteObjectARB = NULL;
PFNGLGETHANDLEARBPROC glGetHandleARB = NULL;
PFNGLDETACHOBJECTARBPROC glDetachObjectARB = NULL;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB = NULL;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB = NULL;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB = NULL;
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB = NULL;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB = NULL;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB = NULL;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB = NULL;
PFNGLVALIDATEPROGRAMARBPROC glValidateProgramARB = NULL;
PFNGLUNIFORM1FARBPROC glUniform1fARB = NULL;
PFNGLUNIFORM2FARBPROC glUniform2fARB = NULL;
PFNGLUNIFORM3FARBPROC glUniform3fARB = NULL;
PFNGLUNIFORM4FARBPROC glUniform4fARB = NULL;
PFNGLUNIFORM1IARBPROC glUniform1iARB = NULL;
PFNGLUNIFORM2IARBPROC glUniform2iARB = NULL;
PFNGLUNIFORM3IARBPROC glUniform3iARB = NULL;
PFNGLUNIFORM4IARBPROC glUniform4iARB = NULL;
PFNGLUNIFORM1FVARBPROC glUniform1fvARB = NULL;
PFNGLUNIFORM2FVARBPROC glUniform2fvARB = NULL;
PFNGLUNIFORM3FVARBPROC glUniform3fvARB = NULL;
PFNGLUNIFORM4FVARBPROC glUniform4fvARB = NULL;
PFNGLUNIFORM1IVARBPROC glUniform1ivARB = NULL;
PFNGLUNIFORM2IVARBPROC glUniform2ivARB = NULL;
PFNGLUNIFORM3IVARBPROC glUniform3ivARB = NULL;
PFNGLUNIFORM4IVARBPROC glUniform4ivARB = NULL;
PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB = NULL;
PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB = NULL;
PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB = NULL;
PFNGLGETOBJECTPARAMETERFVARBPROC glGetObjectParameterfvARB = NULL;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB = NULL;
PFNGLGETINFOLOGARBPROC glGetInfoLogARB = NULL;
PFNGLGETATTACHEDOBJECTSARBPROC glGetAttachedObjectsARB = NULL;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB = NULL;
PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB = NULL;
PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB = NULL;
PFNGLGETUNIFORMIVARBPROC glGetUniformivARB = NULL;
PFNGLGETSHADERSOURCEARBPROC glGetShaderSourceARB = NULL;

// vertex shader prototypes
#if LL_LINUX || LL_SOLARIS
PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB = NULL;
PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB = NULL;
PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB = NULL;
PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB = NULL;
PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB = NULL;
PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB = NULL;
PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB = NULL;
PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB = NULL;
PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB = NULL;
PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB = NULL;
PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB = NULL;
PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB = NULL;
PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB = NULL;
PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB = NULL;
PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB = NULL;
PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB = NULL;
PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB = NULL;
PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB = NULL;
#endif // LL_LINUX || LL_SOLARIS
PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4nbvARB = NULL;
PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4nivARB = NULL;
PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4nsvARB = NULL;
PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4nubARB = NULL;
PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4nubvARB = NULL;
PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4nuivARB = NULL;
PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4nusvARB = NULL;
#if LL_LINUX  || LL_SOLARIS
PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB = NULL;
PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB = NULL;
PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB = NULL;
PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB = NULL;
PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB = NULL;
PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB = NULL;
PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB = NULL;
PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB = NULL;
PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB = NULL;
PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB = NULL;
PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB = NULL;
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB = NULL;
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB = NULL;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLGENPROGRAMSARBPROC glGenProgramsARB = NULL;
PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB = NULL;
PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB = NULL;
PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB = NULL;
PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB = NULL;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB = NULL;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB = NULL;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB = NULL;
PFNGLGETPROGRAMIVARBPROC glGetProgramivARB = NULL;
PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB = NULL;
PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB = NULL;
PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB = NULL;
PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB = NULL;
PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB = NULL;
PFNGLISPROGRAMARBPROC glIsProgramARB = NULL;
#endif // LL_LINUX || LL_SOLARIS
PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB = NULL;
PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB = NULL;
PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB = NULL;

#if LL_WINDOWS
PFNWGLSWAPINTERVALEXTPROC			wglSwapIntervalEXT = NULL;
#endif

#if LL_LINUX_NV_GL_HEADERS
// linux nvidia headers.  these define these differently to mesa's.  ugh.
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTextureARB = NULL;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = NULL;
#endif // LL_LINUX_NV_GL_HEADERS
#endif

LLGLManager gGLManager;

LLGLManager::LLGLManager() :
	mInited(FALSE),
	mIsDisabled(FALSE),

	mHasMultitexture(FALSE),
	mNumTextureUnits(1),
	mHasMipMapGeneration(FALSE),
	mHasCompressedTextures(FALSE),
	mHasFramebufferObject(FALSE),
	mHasFramebufferMultisample(FALSE),
	mHasBlendFuncSeparate(FALSE),

	mHasVertexBufferObject(FALSE),
	mHasPBuffer(FALSE),
	mHasShaderObjects(FALSE),
	mHasVertexShader(FALSE),
	mHasFragmentShader(FALSE),
	mHasOcclusionQuery(FALSE),
	mHasPointParameters(FALSE),
	mHasDrawBuffers(FALSE),
	mHasTextureRectangle(FALSE),

	mHasAnisotropic(FALSE),
	mHasARBEnvCombine(FALSE),
	mHasCubeMap(FALSE),

	mIsATI(FALSE),
	mIsNVIDIA(FALSE),
	mIsIntel(FALSE),
	mIsGF2or4MX(FALSE),
	mIsGF3(FALSE),
	mIsGFFX(FALSE),
	mATIOffsetVerticalLines(FALSE),
	mATIOldDriver(FALSE),

	mHasRequirements(TRUE),

	mHasSeparateSpecularColor(FALSE),

	mDebugGPU(FALSE),

	mDriverVersionMajor(1),
	mDriverVersionMinor(0),
	mDriverVersionRelease(0),
	mGLVersion(1.0f),
		
	mVRAM(0),
	mGLMaxVertexRange(0),
	mGLMaxIndexRange(0)
{
}

//---------------------------------------------------------------------
// Global initialization for GL
//---------------------------------------------------------------------
void LLGLManager::initWGL()
{
	mHasPBuffer = FALSE;
#if LL_WINDOWS && !LL_MESA_HEADLESS
	if (!glh_init_extensions("WGL_ARB_pixel_format"))
	{
		LL_WARNS("RenderInit") << "No ARB pixel format extensions" << LL_ENDL;
	}

	if (ExtensionExists("WGL_EXT_swap_control", gGLHExts.mSysExts))
	{
        GLH_EXT_NAME(wglSwapIntervalEXT) = (PFNWGLSWAPINTERVALEXTPROC)GLH_EXT_GET_PROC_ADDRESS("wglSwapIntervalEXT");
	}

	if( !glh_init_extensions("WGL_ARB_pbuffer") )
	{
		LL_WARNS("RenderInit") << "No ARB WGL PBuffer extensions" << LL_ENDL;
	}

	if( !glh_init_extensions("WGL_ARB_render_texture") )
	{
		LL_WARNS("RenderInit") << "No ARB WGL render texture extensions" << LL_ENDL;
	}

	mHasPBuffer = ExtensionExists("WGL_ARB_pbuffer", gGLHExts.mSysExts) &&
					ExtensionExists("WGL_ARB_render_texture", gGLHExts.mSysExts) &&
					ExtensionExists("WGL_ARB_pixel_format", gGLHExts.mSysExts);
#endif
}

// return false if unable (or unwilling due to old drivers) to init GL
bool LLGLManager::initGL()
{
	if (mInited)
	{
		LL_ERRS("RenderInit") << "Calling init on LLGLManager after already initialized!" << LL_ENDL;
	}

	GLint alpha_bits;
	glGetIntegerv( GL_ALPHA_BITS, &alpha_bits );
	if( 8 != alpha_bits )
	{
		LL_WARNS("RenderInit") << "Frame buffer has less than 8 bits of alpha.  Avatar texture compositing will fail." << LL_ENDL;
	}

	// Extract video card strings and convert to upper case to
	// work around driver-to-driver variation in capitalization.
	mGLVendor = std::string((const char *)glGetString(GL_VENDOR));
	LLStringUtil::toUpper(mGLVendor);

	mGLRenderer = std::string((const char *)glGetString(GL_RENDERER));
	LLStringUtil::toUpper(mGLRenderer);

	parse_gl_version( &mDriverVersionMajor, 
		&mDriverVersionMinor, 
		&mDriverVersionRelease, 
		&mDriverVersionVendorString );

	mGLVersion = mDriverVersionMajor + mDriverVersionMinor * .1f;
	
	// Trailing space necessary to keep "nVidia Corpor_ati_on" cards
	// from being recognized as ATI.
	if (mGLVendor.substr(0,4) == "ATI ")
	{
		mGLVendorShort = "ATI";
		BOOL mobile = FALSE;
		if (mGLRenderer.find("MOBILITY") != std::string::npos)
		{
			mobile = TRUE;
		}
		mIsATI = TRUE;

#if LL_WINDOWS && !LL_MESA_HEADLESS
		if (mDriverVersionRelease < 3842)
		{
			mATIOffsetVerticalLines = TRUE;
		}
#endif // LL_WINDOWS

#if (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
		// release 7277 is a point at which we verify that ATI OpenGL
		// drivers get pretty stable with SL, ~Catalyst 8.2,
		// for both Win32 and Linux.
		if (mDriverVersionRelease < 7277 &&
		    mDriverVersionRelease != 0) // 0 == Undetectable driver version - these get to pretend to be new ATI drivers, though that decision may be revisited.
		{
			mATIOldDriver = TRUE;
		}
#endif // (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
	}
	else if (mGLVendor.find("NVIDIA ") != std::string::npos)
	{
		mGLVendorShort = "NVIDIA";
		mIsNVIDIA = TRUE;
		if (   mGLRenderer.find("GEFORCE4 MX") != std::string::npos
			|| mGLRenderer.find("GEFORCE2") != std::string::npos
			|| mGLRenderer.find("GEFORCE 2") != std::string::npos
			|| mGLRenderer.find("GEFORCE4 460 GO") != std::string::npos
			|| mGLRenderer.find("GEFORCE4 440 GO") != std::string::npos
			|| mGLRenderer.find("GEFORCE4 420 GO") != std::string::npos)
		{
			mIsGF2or4MX = TRUE;
		}
		else if (mGLRenderer.find("GEFORCE FX") != std::string::npos
				 || mGLRenderer.find("QUADRO FX") != std::string::npos
				 || mGLRenderer.find("NV34") != std::string::npos)
		{
			mIsGFFX = TRUE;
		}
		else if(mGLRenderer.find("GEFORCE3") != std::string::npos)
		{
			mIsGF3 = TRUE;
		}

	}
	else if (mGLVendor.find("INTEL") != std::string::npos
#if LL_LINUX
		 // The Mesa-based drivers put this in the Renderer string,
		 // not the Vendor string.
		 || mGLRenderer.find("INTEL") != std::string::npos
#endif //LL_LINUX
		 )
	{
		mGLVendorShort = "INTEL";
		mIsIntel = TRUE;
	}
	else
	{
		mGLVendorShort = "MISC";
	}
	
	// This is called here because it depends on the setting of mIsGF2or4MX, and sets up mHasMultitexture.
	initExtensions();

	if (mHasMultitexture)
	{
		GLint num_tex_units;		
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num_tex_units);
		mNumTextureUnits = llmin(num_tex_units, (GLint)MAX_GL_TEXTURE_UNITS);
		if (mIsIntel)
		{
			mNumTextureUnits = llmin(mNumTextureUnits, 2);
		}
	}
	else
	{
		mHasRequirements = FALSE;

		// We don't support cards that don't support the GL_ARB_multitexture extension
		LL_WARNS("RenderInit") << "GL Drivers do not support GL_ARB_multitexture" << LL_ENDL;
		return false;
	}
	
	setToDebugGPU();

	initGLStates();
	return true;
}

void LLGLManager::setToDebugGPU()
{
	//"MOBILE INTEL(R) 965 EXPRESS CHIP", 
	if (mGLRenderer.find("INTEL") != std::string::npos && mGLRenderer.find("965") != std::string::npos)
	{
		mDebugGPU = TRUE ;
	}

	return ;
}

void LLGLManager::getGLInfo(LLSD& info)
{
	if (gHeadlessClient)
	{
		info["GLInfo"]["GLVendor"] = HEADLESS_VENDOR_STRING;
		info["GLInfo"]["GLRenderer"] = HEADLESS_RENDERER_STRING;
		info["GLInfo"]["GLVersion"] = HEADLESS_VERSION_STRING;
		return;
	}
	else
	{
		info["GLInfo"]["GLVendor"] = std::string((const char *)glGetString(GL_VENDOR));
		info["GLInfo"]["GLRenderer"] = std::string((const char *)glGetString(GL_RENDERER));
		info["GLInfo"]["GLVersion"] = std::string((const char *)glGetString(GL_VERSION));
	}

#if !LL_MESA_HEADLESS
	std::string all_exts = ll_safe_string((const char *)gGLHExts.mSysExts);
	boost::char_separator<char> sep(" ");
	boost::tokenizer<boost::char_separator<char> > tok(all_exts, sep);
	for(boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin(); i != tok.end(); ++i)
	{
		info["GLInfo"]["GLExtensions"].append(*i);
	}
#endif
}

std::string LLGLManager::getGLInfoString()
{
	std::string info_str;

	if (gHeadlessClient)
	{
		info_str += std::string("GL_VENDOR      ") + HEADLESS_VENDOR_STRING + std::string("\n");
		info_str += std::string("GL_RENDERER    ") + HEADLESS_RENDERER_STRING + std::string("\n");
		info_str += std::string("GL_VERSION     ") + HEADLESS_VERSION_STRING + std::string("\n");
	}
	else
	{
		info_str += std::string("GL_VENDOR      ") + ll_safe_string((const char *)glGetString(GL_VENDOR)) + std::string("\n");
		info_str += std::string("GL_RENDERER    ") + ll_safe_string((const char *)glGetString(GL_RENDERER)) + std::string("\n");
		info_str += std::string("GL_VERSION     ") + ll_safe_string((const char *)glGetString(GL_VERSION)) + std::string("\n");
	}

#if !LL_MESA_HEADLESS 
	std::string all_exts= ll_safe_string(((const char *)gGLHExts.mSysExts));
	LLStringUtil::replaceChar(all_exts, ' ', '\n');
	info_str += std::string("GL_EXTENSIONS:\n") + all_exts + std::string("\n");
#endif
	
	return info_str;
}

void LLGLManager::printGLInfoString()
{
	if (gHeadlessClient)
	{
		LL_INFOS("RenderInit") << "GL_VENDOR:     " << HEADLESS_VENDOR_STRING << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_RENDERER:   " << HEADLESS_RENDERER_STRING << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_VERSION:    " << HEADLESS_VERSION_STRING << LL_ENDL;
	}
	else
	{
		LL_INFOS("RenderInit") << "GL_VENDOR:     " << ((const char *)glGetString(GL_VENDOR)) << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_RENDERER:   " << ((const char *)glGetString(GL_RENDERER)) << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_VERSION:    " << ((const char *)glGetString(GL_VERSION)) << LL_ENDL;
	}

#if !LL_MESA_HEADLESS
	std::string all_exts= ll_safe_string(((const char *)gGLHExts.mSysExts));
	LLStringUtil::replaceChar(all_exts, ' ', '\n');
	LL_DEBUGS("RenderInit") << "GL_EXTENSIONS:\n" << all_exts << LL_ENDL;
#endif
}

std::string LLGLManager::getRawGLString()
{
	std::string gl_string;
	if (gHeadlessClient)
	{
		gl_string = HEADLESS_VENDOR_STRING + " " + HEADLESS_RENDERER_STRING;
	}
	else
	{
		gl_string = ll_safe_string((char*)glGetString(GL_VENDOR)) + " " + ll_safe_string((char*)glGetString(GL_RENDERER));
	}
	return gl_string;
}

void LLGLManager::shutdownGL()
{
	if (mInited)
	{
		glFinish();
		stop_glerror();
		mInited = FALSE;
	}
}

// these are used to turn software blending on. They appear in the Debug/Avatar menu
// presence of vertex skinning/blending or vertex programs will set these to FALSE by default.

void LLGLManager::initExtensions()
{
#if LL_MESA_HEADLESS
# ifdef GL_ARB_multitexture
	mHasMultitexture = TRUE;
# else
	mHasMultitexture = FALSE;
# endif // GL_ARB_multitexture
# ifdef GL_ARB_texture_env_combine
	mHasARBEnvCombine = TRUE;	
# else
	mHasARBEnvCombine = FALSE;
# endif // GL_ARB_texture_env_combine
# ifdef GL_ARB_texture_compression
	mHasCompressedTextures = TRUE;
# else
	mHasCompressedTextures = FALSE;
# endif // GL_ARB_texture_compression
# ifdef GL_ARB_vertex_buffer_object
	mHasVertexBufferObject = TRUE;
# else
	mHasVertexBufferObject = FALSE;
# endif // GL_ARB_vertex_buffer_object
# ifdef GL_EXT_framebuffer_object
	mHasFramebufferObject = TRUE;
# else
	mHasFramebufferObject = FALSE;
# endif // GL_EXT_framebuffer_object
# ifdef GL_EXT_framebuffer_multisample
	mHasFramebufferMultisample = TRUE;
# else
	mHasFramebufferMultisample = FALSE;
# endif // GL_EXT_framebuffer_multisample
# ifdef GL_ARB_draw_buffers
	mHasDrawBuffers = TRUE;
#else
	mHasDrawBuffers = FALSE;
# endif // GL_ARB_draw_buffers
# if defined(GL_NV_depth_clamp) || defined(GL_ARB_depth_clamp)
	mHasDepthClamp = TRUE;
#else
	mHasDepthClamp = FALSE;
#endif // defined(GL_NV_depth_clamp) || defined(GL_ARB_depth_clamp)
# if GL_EXT_blend_func_separate
	mHasBlendFuncSeparate = TRUE;
#else
	mHasBlendFuncSeparate = FALSE;
# endif // GL_EXT_blend_func_separate
	mHasMipMapGeneration = FALSE;
	mHasSeparateSpecularColor = FALSE;
	mHasAnisotropic = FALSE;
	mHasCubeMap = FALSE;
	mHasOcclusionQuery = FALSE;
	mHasPointParameters = FALSE;
	mHasShaderObjects = FALSE;
	mHasVertexShader = FALSE;
	mHasFragmentShader = FALSE;
	mHasTextureRectangle = FALSE;
#else // LL_MESA_HEADLESS
	mHasMultitexture = glh_init_extensions("GL_ARB_multitexture");
	mHasMipMapGeneration = glh_init_extensions("GL_SGIS_generate_mipmap");
	mHasSeparateSpecularColor = glh_init_extensions("GL_EXT_separate_specular_color");
	mHasAnisotropic = glh_init_extensions("GL_EXT_texture_filter_anisotropic");
	glh_init_extensions("GL_ARB_texture_cube_map");
	mHasCubeMap = ExtensionExists("GL_ARB_texture_cube_map", gGLHExts.mSysExts);
	mHasARBEnvCombine = ExtensionExists("GL_ARB_texture_env_combine", gGLHExts.mSysExts);
	mHasCompressedTextures = glh_init_extensions("GL_ARB_texture_compression");
	mHasOcclusionQuery = ExtensionExists("GL_ARB_occlusion_query", gGLHExts.mSysExts);
	mHasVertexBufferObject = ExtensionExists("GL_ARB_vertex_buffer_object", gGLHExts.mSysExts);
	mHasDepthClamp = ExtensionExists("GL_ARB_depth_clamp", gGLHExts.mSysExts) || ExtensionExists("GL_NV_depth_clamp", gGLHExts.mSysExts);
	// mask out FBO support when packed_depth_stencil isn't there 'cause we need it for LLRenderTarget -Brad
	mHasFramebufferObject = ExtensionExists("GL_EXT_framebuffer_object", gGLHExts.mSysExts)
		&& ExtensionExists("GL_EXT_packed_depth_stencil", gGLHExts.mSysExts);
	mHasFramebufferMultisample = mHasFramebufferObject && ExtensionExists("GL_EXT_framebuffer_multisample", gGLHExts.mSysExts);
	mHasDrawBuffers = ExtensionExists("GL_ARB_draw_buffers", gGLHExts.mSysExts);
	mHasBlendFuncSeparate = ExtensionExists("GL_EXT_blend_func_separate", gGLHExts.mSysExts);
	mHasTextureRectangle = ExtensionExists("GL_ARB_texture_rectangle", gGLHExts.mSysExts);
#if !LL_DARWIN
	mHasPointParameters = !mIsATI && ExtensionExists("GL_ARB_point_parameters", gGLHExts.mSysExts);
#endif
	mHasShaderObjects = ExtensionExists("GL_ARB_shader_objects", gGLHExts.mSysExts) && ExtensionExists("GL_ARB_shading_language_100", gGLHExts.mSysExts);
	mHasVertexShader = ExtensionExists("GL_ARB_vertex_program", gGLHExts.mSysExts) && ExtensionExists("GL_ARB_vertex_shader", gGLHExts.mSysExts)
						&& ExtensionExists("GL_ARB_shading_language_100", gGLHExts.mSysExts);
	mHasFragmentShader = ExtensionExists("GL_ARB_fragment_shader", gGLHExts.mSysExts) && ExtensionExists("GL_ARB_shading_language_100", gGLHExts.mSysExts);
#endif

#if LL_LINUX || LL_SOLARIS
	llinfos << "initExtensions() checking shell variables to adjust features..." << llendl;
	// Our extension support for the Linux Client is very young with some
	// potential driver gotchas, so offer a semi-secret way to turn it off.
	if (getenv("LL_GL_NOEXT"))
	{
		//mHasMultitexture = FALSE; // NEEDED!
		mHasDepthClamp = FALSE;
		mHasARBEnvCombine = FALSE;
		mHasCompressedTextures = FALSE;
		mHasVertexBufferObject = FALSE;
		mHasFramebufferObject = FALSE;
		mHasFramebufferMultisample = FALSE;
		mHasDrawBuffers = FALSE;
		mHasBlendFuncSeparate = FALSE;
		mHasMipMapGeneration = FALSE;
		mHasSeparateSpecularColor = FALSE;
		mHasAnisotropic = FALSE;
		mHasCubeMap = FALSE;
		mHasOcclusionQuery = FALSE;
		mHasPointParameters = FALSE;
		mHasShaderObjects = FALSE;
		mHasVertexShader = FALSE;
		mHasFragmentShader = FALSE;
		LL_WARNS("RenderInit") << "GL extension support DISABLED via LL_GL_NOEXT" << LL_ENDL;
	}
	else if (getenv("LL_GL_BASICEXT"))	/* Flawfinder: ignore */
	{
		// This switch attempts to turn off all support for exotic
		// extensions which I believe correspond to fatal driver
		// bug reports.  This should be the default until we get a
		// proper blacklist/whitelist on Linux.
		mHasMipMapGeneration = FALSE;
		mHasAnisotropic = FALSE;
		//mHasCubeMap = FALSE; // apparently fatal on Intel 915 & similar
		//mHasOcclusionQuery = FALSE; // source of many ATI system hangs
		mHasShaderObjects = FALSE;
		mHasVertexShader = FALSE;
		mHasFragmentShader = FALSE;
		mHasBlendFuncSeparate = FALSE;
		LL_WARNS("RenderInit") << "GL extension support forced to SIMPLE level via LL_GL_BASICEXT" << LL_ENDL;
	}
	if (getenv("LL_GL_BLACKLIST"))	/* Flawfinder: ignore */
	{
		// This lets advanced troubleshooters disable specific
		// GL extensions to isolate problems with their hardware.
		// SL-28126
		const char *const blacklist = getenv("LL_GL_BLACKLIST");	/* Flawfinder: ignore */
		LL_WARNS("RenderInit") << "GL extension support partially disabled via LL_GL_BLACKLIST: " << blacklist << LL_ENDL;
		if (strchr(blacklist,'a')) mHasARBEnvCombine = FALSE;
		if (strchr(blacklist,'b')) mHasCompressedTextures = FALSE;
		if (strchr(blacklist,'c')) mHasVertexBufferObject = FALSE;
		if (strchr(blacklist,'d')) mHasMipMapGeneration = FALSE;//S
// 		if (strchr(blacklist,'f')) mHasNVVertexArrayRange = FALSE;//S
// 		if (strchr(blacklist,'g')) mHasNVFence = FALSE;//S
		if (strchr(blacklist,'h')) mHasSeparateSpecularColor = FALSE;
		if (strchr(blacklist,'i')) mHasAnisotropic = FALSE;//S
		if (strchr(blacklist,'j')) mHasCubeMap = FALSE;//S
// 		if (strchr(blacklist,'k')) mHasATIVAO = FALSE;//S
		if (strchr(blacklist,'l')) mHasOcclusionQuery = FALSE;
		if (strchr(blacklist,'m')) mHasShaderObjects = FALSE;//S
		if (strchr(blacklist,'n')) mHasVertexShader = FALSE;//S
		if (strchr(blacklist,'o')) mHasFragmentShader = FALSE;//S
		if (strchr(blacklist,'p')) mHasPointParameters = FALSE;//S
		if (strchr(blacklist,'q')) mHasFramebufferObject = FALSE;//S
		if (strchr(blacklist,'r')) mHasDrawBuffers = FALSE;//S
		if (strchr(blacklist,'s')) mHasFramebufferMultisample = FALSE;
		if (strchr(blacklist,'t')) mHasTextureRectangle = FALSE;
		if (strchr(blacklist,'u')) mHasBlendFuncSeparate = FALSE;//S
		if (strchr(blacklist,'v')) mHasDepthClamp = FALSE;
		
	}
#endif // LL_LINUX || LL_SOLARIS
	
	if (!mHasMultitexture)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize multitexturing" << LL_ENDL;
	}
	if (!mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize mipmap generation" << LL_ENDL;
	}
	if (!mHasARBEnvCombine)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_texture_env_combine" << LL_ENDL;
	}
	if (!mHasSeparateSpecularColor)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize separate specular color" << LL_ENDL;
	}
	if (!mHasAnisotropic)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize anisotropic filtering" << LL_ENDL;
	}
	if (!mHasCompressedTextures)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_texture_compression" << LL_ENDL;
	}
	if (!mHasOcclusionQuery)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_occlusion_query" << LL_ENDL;
	}
	if (!mHasPointParameters)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_point_parameters" << LL_ENDL;
	}
	if (!mHasShaderObjects)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_shader_objects" << LL_ENDL;
	}
	if (!mHasVertexShader)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_vertex_shader" << LL_ENDL;
	}
	if (!mHasFragmentShader)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_fragment_shader" << LL_ENDL;
	}
	if (!mHasBlendFuncSeparate)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_EXT_blend_func_separate" << LL_ENDL;
	}
	if (!mHasDrawBuffers)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_draw_buffers" << LL_ENDL;
	}

	// Disable certain things due to known bugs
	if (mIsIntel && mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Disabling mip-map generation for Intel GPUs" << LL_ENDL;
		mHasMipMapGeneration = FALSE;
	}
	if (mIsATI && mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Disabling mip-map generation for ATI GPUs (performance opt)" << LL_ENDL;
		mHasMipMapGeneration = FALSE;
	}
	
	// Misc
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*) &mGLMaxVertexRange);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*) &mGLMaxIndexRange);
	
#if (LL_WINDOWS || LL_LINUX || LL_SOLARIS) && !LL_MESA_HEADLESS
	LL_DEBUGS("RenderInit") << "GL Probe: Getting symbols" << LL_ENDL;
	if (mHasVertexBufferObject)
	{
		glBindBufferARB = (PFNGLBINDBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBindBufferARB");
		if (glBindBufferARB)
		{
			glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS("glDeleteBuffersARB");
			glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGenBuffersARB");
			glIsBufferARB = (PFNGLISBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glIsBufferARB");
			glBufferDataARB = (PFNGLBUFFERDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBufferDataARB");
			glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBufferSubDataARB");
			glGetBufferSubDataARB = (PFNGLGETBUFFERSUBDATAARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetBufferSubDataARB");
			glMapBufferARB = (PFNGLMAPBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glMapBufferARB");
			glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)GLH_EXT_GET_PROC_ADDRESS("glUnmapBufferARB");
			glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetBufferParameterivARB");
			glGetBufferPointervARB = (PFNGLGETBUFFERPOINTERVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetBufferPointervARB");
		}
		else
		{
			mHasVertexBufferObject = FALSE;
		}
	}
	if (mHasFramebufferObject)
	{
		llinfos << "initExtensions() FramebufferObject-related procs..." << llendl;
		glIsRenderbufferEXT = (PFNGLISRENDERBUFFEREXTPROC) GLH_EXT_GET_PROC_ADDRESS("glIsRenderbufferEXT");
		glBindRenderbufferEXT = (PFNGLBINDRENDERBUFFEREXTPROC) GLH_EXT_GET_PROC_ADDRESS("glBindRenderbufferEXT");
		glDeleteRenderbuffersEXT = (PFNGLDELETERENDERBUFFERSEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteRenderbuffersEXT");
		glGenRenderbuffersEXT = (PFNGLGENRENDERBUFFERSEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glGenRenderbuffersEXT");
		glRenderbufferStorageEXT = (PFNGLRENDERBUFFERSTORAGEEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glRenderbufferStorageEXT");
		glGetRenderbufferParameterivEXT = (PFNGLGETRENDERBUFFERPARAMETERIVEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glGetRenderbufferParameterivEXT");
		glIsFramebufferEXT = (PFNGLISFRAMEBUFFEREXTPROC) GLH_EXT_GET_PROC_ADDRESS("glIsFramebufferEXT");
		glBindFramebufferEXT = (PFNGLBINDFRAMEBUFFEREXTPROC) GLH_EXT_GET_PROC_ADDRESS("glBindFramebufferEXT");
		glDeleteFramebuffersEXT = (PFNGLDELETEFRAMEBUFFERSEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteFramebuffersEXT");
		glGenFramebuffersEXT = (PFNGLGENFRAMEBUFFERSEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glGenFramebuffersEXT");
		glCheckFramebufferStatusEXT = (PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glCheckFramebufferStatusEXT");
		glFramebufferTexture1DEXT = (PFNGLFRAMEBUFFERTEXTURE1DEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture1DEXT");
		glFramebufferTexture2DEXT = (PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture2DEXT");
		glFramebufferTexture3DEXT = (PFNGLFRAMEBUFFERTEXTURE3DEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferTexture3DEXT");
		glFramebufferRenderbufferEXT = (PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) GLH_EXT_GET_PROC_ADDRESS("glFramebufferRenderbufferEXT");
		glGetFramebufferAttachmentParameterivEXT = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glGetFramebufferAttachmentParameterivEXT");
		glGenerateMipmapEXT = (PFNGLGENERATEMIPMAPEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glGenerateMipmapEXT");
	}
	if (mHasFramebufferMultisample)
	{
		glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glRenderbufferStorageMultisampleEXT");
		glBlitFramebufferEXT = (PFNGLBLITFRAMEBUFFEREXTPROC) GLH_EXT_GET_PROC_ADDRESS("glBlitFramebufferEXT");
	}
	if (mHasDrawBuffers)
	{
		glDrawBuffersARB = (PFNGLDRAWBUFFERSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDrawBuffersARB");
	}
	if (mHasBlendFuncSeparate)
	{
		glBlendFuncSeparateEXT = (PFNGLBLENDFUNCSEPARATEEXTPROC) GLH_EXT_GET_PROC_ADDRESS("glBlendFuncSeparateEXT");
	}
#if (!LL_LINUX && !LL_SOLARIS) || LL_LINUX_NV_GL_HEADERS
	// This is expected to be a static symbol on Linux GL implementations, except if we use the nvidia headers - bah
	glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC)GLH_EXT_GET_PROC_ADDRESS("glDrawRangeElements");
	if (!glDrawRangeElements)
	{
		mGLMaxVertexRange = 0;
		mGLMaxIndexRange = 0;
	}
#endif // !LL_LINUX || LL_LINUX_NV_GL_HEADERS
#if LL_LINUX_NV_GL_HEADERS
	// nvidia headers are critically different from mesa-esque
 	glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS("glActiveTextureARB");
 	glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)GLH_EXT_GET_PROC_ADDRESS("glClientActiveTextureARB");
#endif // LL_LINUX_NV_GL_HEADERS

	if (mHasOcclusionQuery)
	{
		llinfos << "initExtensions() OcclusionQuery-related procs..." << llendl;
		glGenQueriesARB = (PFNGLGENQUERIESARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGenQueriesARB");
		glDeleteQueriesARB = (PFNGLDELETEQUERIESARBPROC)GLH_EXT_GET_PROC_ADDRESS("glDeleteQueriesARB");
		glIsQueryARB = (PFNGLISQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS("glIsQueryARB");
		glBeginQueryARB = (PFNGLBEGINQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS("glBeginQueryARB");
		glEndQueryARB = (PFNGLENDQUERYARBPROC)GLH_EXT_GET_PROC_ADDRESS("glEndQueryARB");
		glGetQueryivARB = (PFNGLGETQUERYIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetQueryivARB");
		glGetQueryObjectivARB = (PFNGLGETQUERYOBJECTIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetQueryObjectivARB");
		glGetQueryObjectuivARB = (PFNGLGETQUERYOBJECTUIVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glGetQueryObjectuivARB");
	}
	if (mHasPointParameters)
	{
		llinfos << "initExtensions() PointParameters-related procs..." << llendl;
		glPointParameterfARB = (PFNGLPOINTPARAMETERFARBPROC)GLH_EXT_GET_PROC_ADDRESS("glPointParameterfARB");
		glPointParameterfvARB = (PFNGLPOINTPARAMETERFVARBPROC)GLH_EXT_GET_PROC_ADDRESS("glPointParameterfvARB");
	}
	if (mHasShaderObjects)
	{
		glDeleteObjectARB = (PFNGLDELETEOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteObjectARB");
		glGetHandleARB = (PFNGLGETHANDLEARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetHandleARB");
		glDetachObjectARB = (PFNGLDETACHOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDetachObjectARB");
		glCreateShaderObjectARB = (PFNGLCREATESHADEROBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glCreateShaderObjectARB");
		glShaderSourceARB = (PFNGLSHADERSOURCEARBPROC) GLH_EXT_GET_PROC_ADDRESS("glShaderSourceARB");
		glCompileShaderARB = (PFNGLCOMPILESHADERARBPROC) GLH_EXT_GET_PROC_ADDRESS("glCompileShaderARB");
		glCreateProgramObjectARB = (PFNGLCREATEPROGRAMOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glCreateProgramObjectARB");
		glAttachObjectARB = (PFNGLATTACHOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glAttachObjectARB");
		glLinkProgramARB = (PFNGLLINKPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glLinkProgramARB");
		glUseProgramObjectARB = (PFNGLUSEPROGRAMOBJECTARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUseProgramObjectARB");
		glValidateProgramARB = (PFNGLVALIDATEPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glValidateProgramARB");
		glUniform1fARB = (PFNGLUNIFORM1FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1fARB");
		glUniform2fARB = (PFNGLUNIFORM2FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2fARB");
		glUniform3fARB = (PFNGLUNIFORM3FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3fARB");
		glUniform4fARB = (PFNGLUNIFORM4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4fARB");
		glUniform1iARB = (PFNGLUNIFORM1IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1iARB");
		glUniform2iARB = (PFNGLUNIFORM2IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2iARB");
		glUniform3iARB = (PFNGLUNIFORM3IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3iARB");
		glUniform4iARB = (PFNGLUNIFORM4IARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4iARB");
		glUniform1fvARB = (PFNGLUNIFORM1FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1fvARB");
		glUniform2fvARB = (PFNGLUNIFORM2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2fvARB");
		glUniform3fvARB = (PFNGLUNIFORM3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3fvARB");
		glUniform4fvARB = (PFNGLUNIFORM4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4fvARB");
		glUniform1ivARB = (PFNGLUNIFORM1IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform1ivARB");
		glUniform2ivARB = (PFNGLUNIFORM2IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform2ivARB");
		glUniform3ivARB = (PFNGLUNIFORM3IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform3ivARB");
		glUniform4ivARB = (PFNGLUNIFORM4IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniform4ivARB");
		glUniformMatrix2fvARB = (PFNGLUNIFORMMATRIX2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix2fvARB");
		glUniformMatrix3fvARB = (PFNGLUNIFORMMATRIX3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix3fvARB");
		glUniformMatrix4fvARB = (PFNGLUNIFORMMATRIX4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glUniformMatrix4fvARB");
		glGetObjectParameterfvARB = (PFNGLGETOBJECTPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetObjectParameterfvARB");
		glGetObjectParameterivARB = (PFNGLGETOBJECTPARAMETERIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetObjectParameterivARB");
		glGetInfoLogARB = (PFNGLGETINFOLOGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetInfoLogARB");
		glGetAttachedObjectsARB = (PFNGLGETATTACHEDOBJECTSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetAttachedObjectsARB");
		glGetUniformLocationARB = (PFNGLGETUNIFORMLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetUniformLocationARB");
		glGetActiveUniformARB = (PFNGLGETACTIVEUNIFORMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetActiveUniformARB");
		glGetUniformfvARB = (PFNGLGETUNIFORMFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetUniformfvARB");
		glGetUniformivARB = (PFNGLGETUNIFORMIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetUniformivARB");
		glGetShaderSourceARB = (PFNGLGETSHADERSOURCEARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetShaderSourceARB");
	}
	if (mHasVertexShader)
	{
		llinfos << "initExtensions() VertexShader-related procs..." << llendl;
		glGetAttribLocationARB = (PFNGLGETATTRIBLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetAttribLocationARB");
		glBindAttribLocationARB = (PFNGLBINDATTRIBLOCATIONARBPROC) GLH_EXT_GET_PROC_ADDRESS("glBindAttribLocationARB");
		glGetActiveAttribARB = (PFNGLGETACTIVEATTRIBARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetActiveAttribARB");
		glVertexAttrib1dARB = (PFNGLVERTEXATTRIB1DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1dARB");
		glVertexAttrib1dvARB = (PFNGLVERTEXATTRIB1DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1dvARB");
		glVertexAttrib1fARB = (PFNGLVERTEXATTRIB1FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1fARB");
		glVertexAttrib1fvARB = (PFNGLVERTEXATTRIB1FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1fvARB");
		glVertexAttrib1sARB = (PFNGLVERTEXATTRIB1SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1sARB");
		glVertexAttrib1svARB = (PFNGLVERTEXATTRIB1SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib1svARB");
		glVertexAttrib2dARB = (PFNGLVERTEXATTRIB2DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2dARB");
		glVertexAttrib2dvARB = (PFNGLVERTEXATTRIB2DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2dvARB");
		glVertexAttrib2fARB = (PFNGLVERTEXATTRIB2FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2fARB");
		glVertexAttrib2fvARB = (PFNGLVERTEXATTRIB2FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2fvARB");
		glVertexAttrib2sARB = (PFNGLVERTEXATTRIB2SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2sARB");
		glVertexAttrib2svARB = (PFNGLVERTEXATTRIB2SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib2svARB");
		glVertexAttrib3dARB = (PFNGLVERTEXATTRIB3DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3dARB");
		glVertexAttrib3dvARB = (PFNGLVERTEXATTRIB3DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3dvARB");
		glVertexAttrib3fARB = (PFNGLVERTEXATTRIB3FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3fARB");
		glVertexAttrib3fvARB = (PFNGLVERTEXATTRIB3FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3fvARB");
		glVertexAttrib3sARB = (PFNGLVERTEXATTRIB3SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3sARB");
		glVertexAttrib3svARB = (PFNGLVERTEXATTRIB3SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib3svARB");
		glVertexAttrib4nbvARB = (PFNGLVERTEXATTRIB4NBVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nbvARB");
		glVertexAttrib4nivARB = (PFNGLVERTEXATTRIB4NIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nivARB");
		glVertexAttrib4nsvARB = (PFNGLVERTEXATTRIB4NSVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nsvARB");
		glVertexAttrib4nubARB = (PFNGLVERTEXATTRIB4NUBARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nubARB");
		glVertexAttrib4nubvARB = (PFNGLVERTEXATTRIB4NUBVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nubvARB");
		glVertexAttrib4nuivARB = (PFNGLVERTEXATTRIB4NUIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nuivARB");
		glVertexAttrib4nusvARB = (PFNGLVERTEXATTRIB4NUSVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4nusvARB");
		glVertexAttrib4bvARB = (PFNGLVERTEXATTRIB4BVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4bvARB");
		glVertexAttrib4dARB = (PFNGLVERTEXATTRIB4DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4dARB");
		glVertexAttrib4dvARB = (PFNGLVERTEXATTRIB4DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4dvARB");
		glVertexAttrib4fARB = (PFNGLVERTEXATTRIB4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4fARB");
		glVertexAttrib4fvARB = (PFNGLVERTEXATTRIB4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4fvARB");
		glVertexAttrib4ivARB = (PFNGLVERTEXATTRIB4IVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4ivARB");
		glVertexAttrib4sARB = (PFNGLVERTEXATTRIB4SARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4sARB");
		glVertexAttrib4svARB = (PFNGLVERTEXATTRIB4SVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4svARB");
		glVertexAttrib4ubvARB = (PFNGLVERTEXATTRIB4UBVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4ubvARB");
		glVertexAttrib4uivARB = (PFNGLVERTEXATTRIB4UIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4uivARB");
		glVertexAttrib4usvARB = (PFNGLVERTEXATTRIB4USVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttrib4usvARB");
		glVertexAttribPointerARB = (PFNGLVERTEXATTRIBPOINTERARBPROC) GLH_EXT_GET_PROC_ADDRESS("glVertexAttribPointerARB");
		glEnableVertexAttribArrayARB = (PFNGLENABLEVERTEXATTRIBARRAYARBPROC) GLH_EXT_GET_PROC_ADDRESS("glEnableVertexAttribArrayARB");
		glDisableVertexAttribArrayARB = (PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDisableVertexAttribArrayARB");
		glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramStringARB");
		glBindProgramARB = (PFNGLBINDPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glBindProgramARB");
		glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glDeleteProgramsARB");
		glGenProgramsARB = (PFNGLGENPROGRAMSARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGenProgramsARB");
		glProgramEnvParameter4dARB = (PFNGLPROGRAMENVPARAMETER4DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4dARB");
		glProgramEnvParameter4dvARB = (PFNGLPROGRAMENVPARAMETER4DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4dvARB");
		glProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4fARB");
		glProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramEnvParameter4fvARB");
		glProgramLocalParameter4dARB = (PFNGLPROGRAMLOCALPARAMETER4DARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4dARB");
		glProgramLocalParameter4dvARB = (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4dvARB");
		glProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4fARB");
		glProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glProgramLocalParameter4fvARB");
		glGetProgramEnvParameterdvARB = (PFNGLGETPROGRAMENVPARAMETERDVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramEnvParameterdvARB");
		glGetProgramEnvParameterfvARB = (PFNGLGETPROGRAMENVPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramEnvParameterfvARB");
		glGetProgramLocalParameterdvARB = (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramLocalParameterdvARB");
		glGetProgramLocalParameterfvARB = (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramLocalParameterfvARB");
		glGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramivARB");
		glGetProgramStringARB = (PFNGLGETPROGRAMSTRINGARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetProgramStringARB");
		glGetVertexAttribdvARB = (PFNGLGETVERTEXATTRIBDVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetVertexAttribdvARB");
		glGetVertexAttribfvARB = (PFNGLGETVERTEXATTRIBFVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetVertexAttribfvARB");
		glGetVertexAttribivARB = (PFNGLGETVERTEXATTRIBIVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glGetVertexAttribivARB");
		glGetVertexAttribPointervARB = (PFNGLGETVERTEXATTRIBPOINTERVARBPROC) GLH_EXT_GET_PROC_ADDRESS("glgetVertexAttribPointervARB");
		glIsProgramARB = (PFNGLISPROGRAMARBPROC) GLH_EXT_GET_PROC_ADDRESS("glIsProgramARB");
	}
	LL_DEBUGS("RenderInit") << "GL Probe: Got symbols" << LL_ENDL;
#endif

	mInited = TRUE;
}

void rotate_quat(LLQuaternion& rotation)
{
	F32 angle_radians, x, y, z;
	rotation.getAngleAxis(&angle_radians, &x, &y, &z);
	glRotatef(angle_radians * RAD_TO_DEG, x, y, z);
}

void flush_glerror()
{
	glGetError();
}

//this function outputs gl error to the log file, does not crash the code.
void log_glerror()
{
	if (LL_UNLIKELY(!gGLManager.mInited))
	{
		return ;
	}
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
	while (LL_UNLIKELY(error))
	{
		GLubyte const * gl_error_msg = gluErrorString(error);
		if (NULL != gl_error_msg)
		{
			llwarns << "GL Error: " << error << " GL Error String: " << gl_error_msg << llendl ;			
		}
		else
		{
			// gluErrorString returns NULL for some extensions' error codes.
			// you'll probably have to grep for the number in glext.h.
			llwarns << "GL Error: UNKNOWN 0x" << std::hex << error << std::dec << llendl;
		}
		error = glGetError();
	}
}

void do_assert_glerror()
{
	if (LL_UNLIKELY(!gGLManager.mInited))
	{
		LL_ERRS("RenderInit") << "GL not initialized" << LL_ENDL;
	}
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
	BOOL quit = FALSE;
	while (LL_UNLIKELY(error))
	{
		quit = TRUE;
		GLubyte const * gl_error_msg = gluErrorString(error);
		if (NULL != gl_error_msg)
		{
			LL_WARNS("RenderState") << "GL Error:" << error<< LL_ENDL;
			LL_WARNS("RenderState") << "GL Error String:" << gl_error_msg << LL_ENDL;

			if (gDebugSession)
			{
				gFailLog << "GL Error:" << gl_error_msg << std::endl;
			}
		}
		else
		{
			// gluErrorString returns NULL for some extensions' error codes.
			// you'll probably have to grep for the number in glext.h.
			LL_WARNS("RenderState") << "GL Error: UNKNOWN 0x" << std::hex << error << std::dec << LL_ENDL;

			if (gDebugSession)
			{
				gFailLog << "GL Error: UNKNOWN 0x" << std::hex << error << std::dec << std::endl;
			}
		}
		error = glGetError();
	}

	if (quit)
	{
		if (gDebugSession)
		{
			ll_fail("assert_glerror failed");
		}
		else
		{
			llerrs << "One or more unhandled GL errors." << llendl;
		}
	}
}

void assert_glerror()
{
	if (!gGLActive)
	{
		//llwarns << "GL used while not active!" << llendl;

		if (gDebugSession)
		{
			//ll_fail("GL used while not active");
		}
	}

	if (gDebugGL) 
	{
		do_assert_glerror();
	}
}
	

void clear_glerror()
{
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
}

///////////////////////////////////////////////////////////////
//
// LLGLState
//

// Static members
boost::unordered_map<LLGLenum, LLGLboolean> LLGLState::sStateMap;

GLboolean LLGLDepthTest::sDepthEnabled = GL_FALSE; // OpenGL default
GLenum LLGLDepthTest::sDepthFunc = GL_LESS; // OpenGL default
GLboolean LLGLDepthTest::sWriteEnabled = GL_TRUE; // OpenGL default

//static
void LLGLState::initClass() 
{
	sStateMap[GL_DITHER] = GL_TRUE;
	// sStateMap[GL_TEXTURE_2D] = GL_TRUE;
	
	//make sure multisample defaults to disabled
	sStateMap[GL_MULTISAMPLE_ARB] = GL_FALSE;
	glDisable(GL_MULTISAMPLE_ARB);

	sStateMap[GL_MULTISAMPLE_ARB] = GL_FALSE;
	glDisable(GL_MULTISAMPLE_ARB);

	glEnableClientState(GL_VERTEX_ARRAY);
}

//static
void LLGLState::restoreGL()
{
	sStateMap.clear();
	initClass();
}

//static
// Really shouldn't be needed, but seems we sometimes do.
void LLGLState::resetTextureStates()
{
	gGL.flush();
	GLint maxTextureUnits;
	
	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &maxTextureUnits);
	for (S32 j = maxTextureUnits-1; j >=0; j--)
	{
		gGL.getTexUnit(j)->activate();
		glClientActiveTextureARB(GL_TEXTURE0_ARB+j);
		j == 0 ? gGL.getTexUnit(j)->enable(LLTexUnit::TT_TEXTURE) : gGL.getTexUnit(j)->disable();
	}
}

void LLGLState::dumpStates() 
{
	LL_INFOS("RenderState") << "GL States:" << LL_ENDL;
	for (boost::unordered_map<LLGLenum, LLGLboolean>::iterator iter = sStateMap.begin();
		 iter != sStateMap.end(); ++iter)
	{
		LL_INFOS("RenderState") << llformat(" 0x%04x : %s",(S32)iter->first,iter->second?"TRUE":"FALSE") << LL_ENDL;
	}
}

void LLGLState::checkStates(const std::string& msg)  
{
	if (!gDebugGL)
	{
		return;
	}

	stop_glerror();

	GLint src;
	GLint dst;
	glGetIntegerv(GL_BLEND_SRC, &src);
	glGetIntegerv(GL_BLEND_DST, &dst);
	
	BOOL error = FALSE;

	if (src != GL_SRC_ALPHA || dst != GL_ONE_MINUS_SRC_ALPHA)
	{
		if (gDebugSession)
		{
			gFailLog << "Blend function corrupted: " << std::hex << src << " " << std::hex << dst << "  " << msg << std::dec << std::endl;
			error = TRUE;
		}
		else
		{
			LL_GL_ERRS << "Blend function corrupted: " << std::hex << src << " " << std::hex << dst << "  " << msg << std::dec << LL_ENDL;
		}
	}
	
	for (boost::unordered_map<LLGLenum, LLGLboolean>::iterator iter = sStateMap.begin();
		 iter != sStateMap.end(); ++iter)
	{
		LLGLenum state = iter->first;
		LLGLboolean cur_state = iter->second;
		LLGLboolean gl_state = glIsEnabled(state);
		if(cur_state != gl_state)
		{
			dumpStates();
			if (gDebugSession)
			{
				gFailLog << llformat("LLGLState error. State: 0x%04x",state) << std::endl;
				error = TRUE;
			}
			else
			{
				LL_GL_ERRS << llformat("LLGLState error. State: 0x%04x",state) << LL_ENDL;
			}
		}
	}
	
	if (error)
	{
		ll_fail("LLGLState::checkStates failed.");
	}
	stop_glerror();
}

void LLGLState::checkTextureChannels(const std::string& msg)
{
	if (!gDebugGL)
	{
		return;
	}

	stop_glerror();

	GLint activeTexture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
	stop_glerror();

	BOOL error = FALSE;

	if (activeTexture == GL_TEXTURE0_ARB)
	{
		GLint tex_env_mode = 0;

		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &tex_env_mode);
		stop_glerror();

		if (tex_env_mode != GL_MODULATE)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL_TEXTURE_ENV_MODE invalid: " << std::hex << tex_env_mode << std::dec << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL_TEXTURE_ENV_MODE invalid: " << std::hex << tex_env_mode << std::dec << std::endl;
			}
		}
	}

	GLint maxTextureUnits = 0;
	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &maxTextureUnits);
	stop_glerror();

	static const char* label[] =
	{
		"GL_TEXTURE_2D",
		"GL_TEXTURE_COORD_ARRAY",
		"GL_TEXTURE_1D",
		"GL_TEXTURE_CUBE_MAP_ARB",
		"GL_TEXTURE_GEN_S",
		"GL_TEXTURE_GEN_T",
		"GL_TEXTURE_GEN_Q",
		"GL_TEXTURE_GEN_R",
		"GL_TEXTURE_RECTANGLE_ARB"
	};

	static GLint value[] =
	{
		GL_TEXTURE_2D,
		GL_TEXTURE_COORD_ARRAY,
		GL_TEXTURE_1D,
		GL_TEXTURE_CUBE_MAP_ARB,
		GL_TEXTURE_GEN_S,
		GL_TEXTURE_GEN_T,
		GL_TEXTURE_GEN_Q,
		GL_TEXTURE_GEN_R,
		GL_TEXTURE_RECTANGLE_ARB
	};

	GLint stackDepth = 0;

	glh::matrix4f mat;
	glh::matrix4f identity;
	identity.identity();

	for (GLint i = 1; i < maxTextureUnits; i++)
	{
		gGL.getTexUnit(i)->activate();
		glClientActiveTextureARB(GL_TEXTURE0_ARB+i);
		stop_glerror();
		glGetIntegerv(GL_TEXTURE_STACK_DEPTH, &stackDepth);
		stop_glerror();

		if (stackDepth != 1)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "Texture matrix stack corrupted." << LL_ENDL;

			if (gDebugSession)
			{
				gFailLog << "Texture matrix stack corrupted." << std::endl;
			}
		}

		glGetFloatv(GL_TEXTURE_MATRIX, (GLfloat*) mat.m);
		stop_glerror();

		if (mat != identity)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "Texture matrix in channel " << i << " corrupt." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "Texture matrix in channel " << i << " corrupt." << std::endl;
			}
		}

		
		for (S32 j = (i == 0 ? 1 : 0); 
			j < (gGLManager.mHasTextureRectangle ? 9 : 8); j++)
		{
			if (glIsEnabled(value[j]))
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture channel " << i << " still has " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "Texture channel " << i << " still has " << label[j] << " enabled." << std::endl;
				}
			}
			stop_glerror();
		}

		glGetFloatv(GL_TEXTURE_MATRIX, mat.m);
		stop_glerror();

		if (mat != identity)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "Texture matrix " << i << " is not identity." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "Texture matrix " << i << " is not identity." << std::endl;
			}
		}
	}

	gGL.getTexUnit(0)->activate();
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	stop_glerror();

	if (error)
	{
		if (gDebugSession)
		{
			ll_fail("LLGLState::checkTextureChannels failed.");
		}
		else
		{
			LL_GL_ERRS << "GL texture state corruption detected.  " << msg << LL_ENDL;
		}
	}
}

void LLGLState::checkClientArrays(const std::string& msg, U32 data_mask)
{
	if (!gDebugGL)
	{
		return;
	}

	stop_glerror();
	BOOL error = FALSE;

	GLint active_texture;
	glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE_ARB, &active_texture);

	if (active_texture != GL_TEXTURE0_ARB)
	{
		llwarns << "Client active texture corrupted: " << active_texture << llendl;
		if (gDebugSession)
		{
			gFailLog << "Client active texture corrupted: " << active_texture << std::endl;
		}
		error = TRUE;
	}

	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &active_texture);
	if (active_texture != GL_TEXTURE0_ARB)
	{
		llwarns << "Active texture corrupted: " << active_texture << llendl;
		if (gDebugSession)
		{
			gFailLog << "Active texture corrupted: " << active_texture << std::endl;
		}
		error = TRUE;
	}

	static const char* label[] =
	{
		"GL_VERTEX_ARRAY",
		"GL_NORMAL_ARRAY",
		"GL_COLOR_ARRAY",
		"GL_TEXTURE_COORD_ARRAY"
	};

	static GLint value[] =
	{
		GL_VERTEX_ARRAY,
		GL_NORMAL_ARRAY,
		GL_COLOR_ARRAY,
		GL_TEXTURE_COORD_ARRAY
	};

	 U32 mask[] = 
	{ //copied from llvertexbuffer.h
		0x0001, //MAP_VERTEX,
		0x0002, //MAP_NORMAL,
		0x0010, //MAP_COLOR,
		0x0004, //MAP_TEXCOORD
	};


	for (S32 j = 0; j < 4; j++)
	{
		if (glIsEnabled(value[j]))
		{
			if (!(mask[j] & data_mask))
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL still has " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "GL still has " << label[j] << " enabled." << std::endl;
				}
			}
		}
		else
		{
			if (mask[j] & data_mask)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL does not have " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "GL does not have " << label[j] << " enabled." << std::endl;
				}
			}
		}
	}

	glClientActiveTextureARB(GL_TEXTURE1_ARB);
	gGL.getTexUnit(1)->activate();
	if (glIsEnabled(GL_TEXTURE_COORD_ARRAY))
	{
		if (!(data_mask & 0x0008))
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL still has GL_TEXTURE_COORD_ARRAY enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL still has GL_TEXTURE_COORD_ARRAY enabled on channel 1." << std::endl;
			}
		}
	}
	else
	{
		if (data_mask & 0x0008)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL does not have GL_TEXTURE_COORD_ARRAY enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL does not have GL_TEXTURE_COORD_ARRAY enabled on channel 1." << std::endl;
			}
		}
	}

	if (glIsEnabled(GL_TEXTURE_2D))
	{
		if (!(data_mask & 0x0008))
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL still has GL_TEXTURE_2D enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL still has GL_TEXTURE_2D enabled on channel 1." << std::endl;
			}
		}
	}
	else
	{
		if (data_mask & 0x0008)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL does not have GL_TEXTURE_2D enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL does not have GL_TEXTURE_2D enabled on channel 1." << std::endl;
			}
		}
	}

	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	gGL.getTexUnit(0)->activate();

	if (gGLManager.mHasVertexShader)
	{	//make sure vertex attribs are all disabled
		GLint count;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS_ARB, &count);
		for (GLint i = 0; i < count; i++)
		{
			GLint enabled;
			glGetVertexAttribivARB((GLuint) i, GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB, &enabled);
			if (enabled)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL still has vertex attrib array " << i << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog <<  "GL still has vertex attrib array " << i << " enabled." << std::endl;
				}
			}
		}
	}

	if (error)
	{
		if (gDebugSession)
		{
			ll_fail("LLGLState::checkClientArrays failed.");
		}
		else
		{
			LL_GL_ERRS << "GL client array corruption detected.  " << msg << LL_ENDL;
		}
	}
}

///////////////////////////////////////////////////////////////////////

LLGLState::LLGLState(LLGLenum state, S32 enabled) :
	mState(state), mWasEnabled(FALSE), mIsEnabled(FALSE)
{
	stop_glerror();
	if (state)
	{
		mWasEnabled = sStateMap[state];
		llassert(mWasEnabled == glIsEnabled(state));
		setEnabled(enabled);
		stop_glerror();
	}
}

void LLGLState::setEnabled(S32 enabled)
{
	if (!mState)
	{
		return;
	}
	if (enabled == CURRENT_STATE)
	{
		enabled = sStateMap[mState] == GL_TRUE ? TRUE : FALSE;
	}
	else if (enabled == TRUE && sStateMap[mState] != GL_TRUE)
	{
		gGL.flush();
		glEnable(mState);
		sStateMap[mState] = GL_TRUE;
	}
	else if (enabled == FALSE && sStateMap[mState] != GL_FALSE)
	{
		gGL.flush();
		glDisable(mState);
		sStateMap[mState] = GL_FALSE;
	}
	mIsEnabled = enabled;
}

LLGLState::~LLGLState() 
{
	stop_glerror();
	if (mState)
	{
		if (gDebugGL)
		{
			if (!gDebugSession)
			{
				llassert_always(sStateMap[mState] == glIsEnabled(mState));
			}
			else
			{
				if (sStateMap[mState] != glIsEnabled(mState))
				{
					ll_fail("GL enabled state does not match expected");
				}
			}
		}

		if (mIsEnabled != mWasEnabled)
		{
			gGL.flush();
			if (mWasEnabled)
			{
				glEnable(mState);
				sStateMap[mState] = GL_TRUE;
			}
			else
			{
				glDisable(mState);
				sStateMap[mState] = GL_FALSE;
			}
		}
	}
	stop_glerror();
}

////////////////////////////////////////////////////////////////////////////////

void LLGLManager::initGLStates()
{
	//gl states moved to classes in llglstates.h
	LLGLState::initClass();
}

////////////////////////////////////////////////////////////////////////////////

void enable_vertex_weighting(const S32 index)
{
#if GL_ARB_vertex_program
	if (index > 0) glEnableVertexAttribArrayARB(index);	// vertex weights
#endif
}

void disable_vertex_weighting(const S32 index)
{
#if GL_ARB_vertex_program
	if (index > 0) glDisableVertexAttribArrayARB(index);	// vertex weights
#endif
}

void enable_binormals(const S32 index)
{
#if GL_ARB_vertex_program
	if (index > 0)
	{
		glEnableVertexAttribArrayARB(index);	// binormals
	}
#endif
}

void disable_binormals(const S32 index)
{
#if GL_ARB_vertex_program
	if (index > 0)
	{
		glDisableVertexAttribArrayARB(index);	// binormals
	}
#endif
}


void enable_cloth_weights(const S32 index)
{
#if GL_ARB_vertex_program
	if (index > 0)	glEnableVertexAttribArrayARB(index);
#endif
}

void disable_cloth_weights(const S32 index)
{
#if GL_ARB_vertex_program
	if (index > 0) glDisableVertexAttribArrayARB(index);
#endif
}

void set_vertex_weights(const S32 index, const U32 stride, const F32 *weights)
{
#if GL_ARB_vertex_program
	if (index > 0) glVertexAttribPointerARB(index, 1, GL_FLOAT, FALSE, stride, weights);
	stop_glerror();
#endif
}

void set_vertex_clothing_weights(const S32 index, const U32 stride, const LLVector4 *weights)
{
#if GL_ARB_vertex_program
	if (index > 0) glVertexAttribPointerARB(index, 4, GL_FLOAT, TRUE, stride, weights);
	stop_glerror();
#endif
}

void set_binormals(const S32 index, const U32 stride,const LLVector3 *binormals)
{
#if GL_ARB_vertex_program
	if (index > 0) glVertexAttribPointerARB(index, 3, GL_FLOAT, FALSE, stride, binormals);
	stop_glerror();
#endif
}

void parse_gl_version( S32* major, S32* minor, S32* release, std::string* vendor_specific )
{
	// GL_VERSION returns a null-terminated string with the format: 
	// <major>.<minor>[.<release>] [<vendor specific>]

	const char* version = (const char*) glGetString(GL_VERSION);
	*major = 0;
	*minor = 0;
	*release = 0;
	vendor_specific->assign("");

	if( !version )
	{
		return;
	}

	std::string ver_copy( version );
	S32 len = (S32)strlen( version );	/* Flawfinder: ignore */
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for( ; i < len; i++ )
	{
		if( '.' == version[i] )
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(major_str, *major);

	if( '.' == version[i] )
	{
		i++;
	}

	// Find the minor version
	start = i;
	for( ; i < len; i++ )
	{
		if( ('.' == version[i]) || isspace(version[i]) )
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(minor_str, *minor);

	// Find the release number (optional)
	if( '.' == version[i] )
	{
		i++;

		start = i;
		for( ; i < len; i++ )
		{
			if( isspace(version[i]) )
			{
				break;
			}
		}

		std::string release_str = ver_copy.substr(start,i-start);
		LLStringUtil::convertToS32(release_str, *release);
	}

	// Skip over any white space
	while( version[i] && isspace( version[i] ) )
	{
		i++;
	}

	// Copy the vendor-specific string (optional)
	if( version[i] )
	{
		vendor_specific->assign( version + i );
	}
}

LLGLUserClipPlane::LLGLUserClipPlane(const LLPlane& p, const glh::matrix4f& modelview, const glh::matrix4f& projection)
{
	mModelview = modelview;
	mProjection = projection;

	setPlane(p.mV[0], p.mV[1], p.mV[2], p.mV[3]);
}

void LLGLUserClipPlane::setPlane(F32 a, F32 b, F32 c, F32 d)
{
	glh::matrix4f& P = mProjection;
	glh::matrix4f& M = mModelview;
    
	glh::matrix4f invtrans_MVP = (P * M).inverse().transpose();
    glh::vec4f oplane(a,b,c,d);
    glh::vec4f cplane;
    invtrans_MVP.mult_matrix_vec(oplane, cplane);

    cplane /= fabs(cplane[2]); // normalize such that depth is not scaled
    cplane[3] -= 1;

    if(cplane[2] < 0)
        cplane *= -1;

    glh::matrix4f suffix;
    suffix.set_row(2, cplane);
    glh::matrix4f newP = suffix * P;
    glMatrixMode(GL_PROJECTION);
	glPushMatrix();
    glLoadMatrixf(newP.m);
	gGLObliqueProjectionInverse = LLMatrix4(newP.inverse().transpose().m);
    glMatrixMode(GL_MODELVIEW);
}

LLGLUserClipPlane::~LLGLUserClipPlane()
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

LLGLNamePool::LLGLNamePool()
{
}

LLGLNamePool::~LLGLNamePool()
{
}

void LLGLNamePool::upkeep()
{
	std::sort(mNameList.begin(), mNameList.end(), CompareUsed());
}

void LLGLNamePool::cleanup()
{
	for (name_list_t::iterator iter = mNameList.begin(); iter != mNameList.end(); ++iter)
	{
		releaseName(iter->name);
	}

	mNameList.clear();
}

GLuint LLGLNamePool::allocate()
{
#if LL_GL_NAME_POOLING
	for (name_list_t::iterator iter = mNameList.begin(); iter != mNameList.end(); ++iter)
	{
		if (!iter->used)
		{
			iter->used = TRUE;
			return iter->name;
		}
	}

	NameEntry entry;
	entry.name = allocateName();
	entry.used = TRUE;
	mNameList.push_back(entry);

	return entry.name;
#else
	return allocateName();
#endif
}

void LLGLNamePool::release(GLuint name)
{
#if LL_GL_NAME_POOLING
	for (name_list_t::iterator iter = mNameList.begin(); iter != mNameList.end(); ++iter)
	{
		if (iter->name == name)
		{
			if (iter->used)
			{
				iter->used = FALSE;
				return;
			}
			else
			{
				llerrs << "Attempted to release a pooled name that is not in use!" << llendl;
			}
		}
	}
	llerrs << "Attempted to release a non pooled name!" << llendl;
#else
	releaseName(name);
#endif
}

//static
void LLGLNamePool::upkeepPools()
{
	LLMemType mt(LLMemType::MTYPE_UPKEEP_POOLS);
	tracker_t::LLInstanceTrackerScopedGuard guard;
	for (tracker_t::instance_iter iter = guard.beginInstances(); iter != guard.endInstances(); ++iter)
	{
		LLGLNamePool & pool = *iter;
		pool.upkeep();
	}
}

//static
void LLGLNamePool::cleanupPools()
{
	tracker_t::LLInstanceTrackerScopedGuard guard;
	for (tracker_t::instance_iter iter = guard.beginInstances(); iter != guard.endInstances(); ++iter)
	{
		LLGLNamePool & pool = *iter;
		pool.cleanup();
	}
}

LLGLDepthTest::LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled, GLenum depth_func)
: mPrevDepthEnabled(sDepthEnabled), mPrevDepthFunc(sDepthFunc), mPrevWriteEnabled(sWriteEnabled)
{
	stop_glerror();
	
	checkState();

	if (!depth_enabled)
	{ // always disable depth writes if depth testing is disabled
	  // GL spec defines this as a requirement, but some implementations allow depth writes with testing disabled
	  // The proper way to write to depth buffer with testing disabled is to enable testing and use a depth_func of GL_ALWAYS
		write_enabled = FALSE;
	}

	if (depth_enabled != sDepthEnabled)
	{
		gGL.flush();
		if (depth_enabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = depth_enabled;
	}
	if (depth_func != sDepthFunc)
	{
		gGL.flush();
		glDepthFunc(depth_func);
		sDepthFunc = depth_func;
	}
	if (write_enabled != sWriteEnabled)
	{
		gGL.flush();
		glDepthMask(write_enabled);
		sWriteEnabled = write_enabled;
	}
}

LLGLDepthTest::~LLGLDepthTest()
{
	checkState();
	if (sDepthEnabled != mPrevDepthEnabled )
	{
		gGL.flush();
		if (mPrevDepthEnabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = mPrevDepthEnabled;
	}
	if (sDepthFunc != mPrevDepthFunc)
	{
		gGL.flush();
		glDepthFunc(mPrevDepthFunc);
		sDepthFunc = mPrevDepthFunc;
	}
	if (sWriteEnabled != mPrevWriteEnabled )
	{
		gGL.flush();
		glDepthMask(mPrevWriteEnabled);
		sWriteEnabled = mPrevWriteEnabled;
	}
}

void LLGLDepthTest::checkState()
{
	if (gDebugGL)
	{
		GLint func = 0;
		GLboolean mask = FALSE;

		glGetIntegerv(GL_DEPTH_FUNC, &func);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);

		if (glIsEnabled(GL_DEPTH_TEST) != sDepthEnabled ||
			sWriteEnabled != mask ||
			sDepthFunc != func)
		{
			if (gDebugSession)
			{
				gFailLog << "Unexpected depth testing state." << std::endl;
			}
			else
			{
				LL_GL_ERRS << "Unexpected depth testing state." << LL_ENDL;
			}
		}
	}
}

LLGLSquashToFarClip::LLGLSquashToFarClip(glh::matrix4f P)
{
	for (U32 i = 0; i < 4; i++)
	{
		P.element(2, i) = P.element(3, i) * 0.99999f;
	}

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(P.m);
	glMatrixMode(GL_MODELVIEW);
}

LLGLSquashToFarClip::~LLGLSquashToFarClip()
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

