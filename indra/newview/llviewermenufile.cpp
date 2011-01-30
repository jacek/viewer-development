/** 
 * @file llviewermenufile.cpp
 * @brief "File" menu in the main menu bar.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llviewermenufile.h"

// project includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llfilepicker.h"
#include "llfloaterreg.h"
#include "llbuycurrencyhtml.h"
#include "llfloatermodelpreview.h"
#include "llfloatersnapshot.h"
#include "llimage.h"
#include "llimagebmp.h"
#include "llimagepng.h"
#include "llimagej2c.h"
#include "llimagejpeg.h"
#include "llimagetga.h"
#include "llinventorymodel.h"	// gInventory
#include "llresourcedata.h"
#include "llfloaterperms.h"
#include "llstatusbar.h"
#include "llviewercontrol.h"	// gSavedSettings
#include "llviewertexturelist.h"
#include "lluictrlfactory.h"
#include "llvfile.h"
#include "llvfs.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"	// gMenuHolder
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llappviewer.h"
#include "lluploaddialog.h"
#include "lltrans.h"
#include "llfloaterbuycurrency.h"

// linden libraries
#include "llassetuploadresponders.h"
#include "lleconomy.h"
#include "llhttpclient.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llstring.h"
#include "lltransactiontypes.h"
#include "lluuid.h"
#include "llvorbisencode.h"
#include "message.h"

// system libraries
#include <boost/tokenizer.hpp>

class LLFileEnableUpload : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		bool new_value = gStatusBar && LLGlobalEconomy::Singleton::getInstance() && (gStatusBar->getBalance() >= LLGlobalEconomy::Singleton::getInstance()->getPriceUpload());
		return new_value;
	}
};

class LLFileEnableUploadModel : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		return true;
	}
};

class LLMeshEnabled : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		return gSavedSettings.getBOOL("MeshEnabled");
	}
};

class LLMeshUploadVisible : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		return gSavedSettings.getBOOL("MeshEnabled") && 
			   LLViewerParcelMgr::getInstance()->allowAgentBuild() && 
			   !gAgent.getRegion()->getCapability("ObjectAdd").empty();
	}
};

LLMutex* LLFilePickerThread::sMutex = NULL;
std::queue<LLFilePickerThread*> LLFilePickerThread::sDeadQ;

void LLFilePickerThread::getFile()
{
#if LL_WINDOWS
	start();
#else
	run();
#endif
}

//virtual 
void LLFilePickerThread::run()
{
	LLFilePicker picker;
#if LL_WINDOWS
	if (picker.getOpenFile(mFilter, false))
	{
		mFile = picker.getFirstFile();
	}
#else
	if (picker.getOpenFile(mFilter, true))
	{
		mFile = picker.getFirstFile();
	}
#endif

	{
		LLMutexLock lock(sMutex);
		sDeadQ.push(this);
	}

}

//static
void LLFilePickerThread::initClass()
{
	sMutex = new LLMutex(NULL);
}

//static
void LLFilePickerThread::cleanupClass()
{
	clearDead();
	
	delete sMutex;
	sMutex = NULL;
}

//static
void LLFilePickerThread::clearDead()
{
	if (!sDeadQ.empty())
	{
		LLMutexLock lock(sMutex);
		while (!sDeadQ.empty())
		{
			LLFilePickerThread* thread = sDeadQ.front();
			thread->notify(thread->mFile);
			delete thread;
			sDeadQ.pop();
		}
	}
}


//============================================================================

#if LL_WINDOWS
static std::string SOUND_EXTENSIONS = "wav";
static std::string IMAGE_EXTENSIONS = "tga bmp jpg jpeg png";
static std::string ANIM_EXTENSIONS =  "bvh";
#ifdef _CORY_TESTING
static std::string GEOMETRY_EXTENSIONS = "slg";
#endif
static std::string XML_EXTENSIONS = "xml";
static std::string SLOBJECT_EXTENSIONS = "slobject";
#endif
static std::string ALL_FILE_EXTENSIONS = "*.*";
static std::string MODEL_EXTENSIONS = "dae";

std::string build_extensions_string(LLFilePicker::ELoadFilter filter)
{
	switch(filter)
	{
#if LL_WINDOWS
	case LLFilePicker::FFLOAD_IMAGE:
		return IMAGE_EXTENSIONS;
	case LLFilePicker::FFLOAD_WAV:
		return SOUND_EXTENSIONS;
	case LLFilePicker::FFLOAD_ANIM:
		return ANIM_EXTENSIONS;
	case LLFilePicker::FFLOAD_SLOBJECT:
		return SLOBJECT_EXTENSIONS;
	case LLFilePicker::FFLOAD_MODEL:
		return MODEL_EXTENSIONS;
#ifdef _CORY_TESTING
	case LLFilePicker::FFLOAD_GEOMETRY:
		return GEOMETRY_EXTENSIONS;
#endif
	case LLFilePicker::FFLOAD_XML:
	    return XML_EXTENSIONS;
	case LLFilePicker::FFLOAD_ALL:
		return ALL_FILE_EXTENSIONS;
#endif
    default:
	return ALL_FILE_EXTENSIONS;
	}
}

/**
   char* upload_pick(void* data)

   If applicable, brings up a file chooser in which the user selects a file
   to upload for a particular task.  If the file is valid for the given action,
   returns the string to the full path filename, else returns NULL.
   Data is the load filter for the type of file as defined in LLFilePicker.
**/
const std::string upload_pick(void* data)
{
 	if( gAgentCamera.cameraMouselook() )
	{
		gAgentCamera.changeCameraToDefault();
		// This doesn't seem necessary. JC
		// display();
	}

	LLFilePicker::ELoadFilter type;
	if(data)
	{
		type = (LLFilePicker::ELoadFilter)((intptr_t)data);
	}
	else
	{
		type = LLFilePicker::FFLOAD_ALL;
	}

	LLFilePicker& picker = LLFilePicker::instance();
	if (!picker.getOpenFile(type))
	{
		llinfos << "Couldn't import objects from file" << llendl;
		return std::string();
	}

	
	const std::string& filename = picker.getFirstFile();
	std::string ext = gDirUtilp->getExtension(filename);

	//strincmp doesn't like NULL pointers
	if (ext.empty())
	{
		std::string short_name = gDirUtilp->getBaseFileName(filename);
		
		// No extension
		LLSD args;
		args["FILE"] = short_name;
		LLNotificationsUtil::add("NoFileExtension", args);
		return std::string();
	}
	else
	{
		//so there is an extension
		//loop over the valid extensions and compare to see
		//if the extension is valid

		//now grab the set of valid file extensions
		std::string valid_extensions = build_extensions_string(type);

		BOOL ext_valid = FALSE;
		
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(" ");
		tokenizer tokens(valid_extensions, sep);
		tokenizer::iterator token_iter;

		//now loop over all valid file extensions
		//and compare them to the extension of the file
		//to be uploaded
		for( token_iter = tokens.begin();
			 token_iter != tokens.end() && ext_valid != TRUE;
			 ++token_iter)
		{
			const std::string& cur_token = *token_iter;

			if (cur_token == ext || cur_token == "*.*")
			{
				//valid extension
				//or the acceptable extension is any
				ext_valid = TRUE;
			}
		}//end for (loop over all tokens)

		if (ext_valid == FALSE)
		{
			//should only get here if the extension exists
			//but is invalid
			LLSD args;
			args["EXTENSION"] = ext;
			args["VALIDS"] = valid_extensions;
			LLNotificationsUtil::add("InvalidFileExtension", args);
			return std::string();
		}
	}//end else (non-null extension)

	//valid file extension
	
	//now we check to see
	//if the file is actually a valid image/sound/etc.
	if (type == LLFilePicker::FFLOAD_WAV)
	{
		// pre-qualify wavs to make sure the format is acceptable
		std::string error_msg;
		if (check_for_invalid_wav_formats(filename,error_msg))
		{
			llinfos << error_msg << ": " << filename << llendl;
			LLSD args;
			args["FILE"] = filename;
			LLNotificationsUtil::add( error_msg, args );
			return std::string();
		}
	}//end if a wave/sound file

	
	return filename;
}

class LLFileUploadImage : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		std::string filename = upload_pick((void *)LLFilePicker::FFLOAD_IMAGE);
		if (!filename.empty())
		{
			LLFloaterReg::showInstance("upload_image", LLSD(filename));
		}
		return TRUE;
	}
};

class LLFileUploadModel : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		LLFloaterModelPreview* fmp = (LLFloaterModelPreview*) LLFloaterReg::getInstance("upload_model");
		if (fmp)
		{
			fmp->loadModel(3);
		}
		
		return TRUE;
	}
};
	
class LLFileUploadSound : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		std::string filename = upload_pick((void*)LLFilePicker::FFLOAD_WAV);
		if (!filename.empty())
		{
			LLFloaterReg::showInstance("upload_sound", LLSD(filename));
		}
		return true;
	}
};

class LLFileUploadAnim : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		const std::string filename = upload_pick((void*)LLFilePicker::FFLOAD_ANIM);
		if (!filename.empty())
		{
			LLFloaterReg::showInstance("upload_anim", LLSD(filename));
		}
		return true;
	}
};

class LLFileUploadBulk : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		if( gAgentCamera.cameraMouselook() )
		{
			gAgentCamera.changeCameraToDefault();
		}

		// TODO:
		// Iterate over all files
		// Check extensions for uploadability, cost
		// Check user balance for entire cost
		// Charge user entire cost
		// Loop, uploading
		// If an upload fails, refund the user for that one
		//
		// Also fix single upload to charge first, then refund

		LLFilePicker& picker = LLFilePicker::instance();
		if (picker.getMultipleOpenFiles())
		{
			const std::string& filename = picker.getFirstFile();
			std::string name = gDirUtilp->getBaseFileName(filename, true);
			
			std::string asset_name = name;
			LLStringUtil::replaceNonstandardASCII( asset_name, '?' );
			LLStringUtil::replaceChar(asset_name, '|', '?');
			LLStringUtil::stripNonprintable(asset_name);
			LLStringUtil::trim(asset_name);
			
			std::string display_name = LLStringUtil::null;
			LLAssetStorage::LLStoreAssetCallback callback = NULL;
			S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
			void *userdata = NULL;

			upload_new_resource(
				filename,
				asset_name,
				asset_name,
				0,
				LLFolderType::FT_NONE,
				LLInventoryType::IT_NONE,
				LLFloaterPerms::getNextOwnerPerms(),
				LLFloaterPerms::getGroupPerms(),
				LLFloaterPerms::getEveryonePerms(),
				display_name,
				callback,
				expected_upload_cost,
				userdata);

			// *NOTE: Ew, we don't iterate over the file list here,
			// we handle the next files in upload_done_callback()
		}
		else
		{
			llinfos << "Couldn't import objects from file" << llendl;
		}
		return true;
	}
};

void upload_error(const std::string& error_message, const std::string& label, const std::string& filename, const LLSD& args) 
{
	llwarns << error_message << llendl;
	LLNotificationsUtil::add(label, args);
	if(LLFile::remove(filename) == -1)
	{
		lldebugs << "unable to remove temp file" << llendl;
	}
	LLFilePicker::instance().reset();						
}

class LLFileEnableCloseWindow : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		bool new_value = NULL != LLFloater::getClosableFloaterFromFocus();
		return new_value;
	}
};

class LLFileCloseWindow : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		LLFloater::closeFocusedFloater();

		return true;
	}
};

class LLFileEnableCloseAllWindows : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		bool open_children = gFloaterView->allChildrenClosed();
		return !open_children;
	}
};

class LLFileCloseAllWindows : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		bool app_quitting = false;
		gFloaterView->closeAllChildren(app_quitting);

		return true;
	}
};

class LLFileTakeSnapshotToDisk : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		LLPointer<LLImageRaw> raw = new LLImageRaw;

		S32 width = gViewerWindow->getWindowWidthRaw();
		S32 height = gViewerWindow->getWindowHeightRaw();

		if (gSavedSettings.getBOOL("HighResSnapshot"))
		{
			width *= 2;
			height *= 2;
		}

		if (gViewerWindow->rawSnapshot(raw,
									   width,
									   height,
									   TRUE,
									   FALSE,
									   gSavedSettings.getBOOL("RenderUIInSnapshot"),
									   FALSE))
		{
			gViewerWindow->playSnapshotAnimAndSound();
			
			LLPointer<LLImageFormatted> formatted;
			switch(LLFloaterSnapshot::ESnapshotFormat(gSavedSettings.getS32("SnapshotFormat")))
			{
			  case LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG:
				formatted = new LLImageJPEG(gSavedSettings.getS32("SnapshotQuality"));
				break;
			  case LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG:
				formatted = new LLImagePNG;
				break;
			  case LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP: 
				formatted = new LLImageBMP;
				break;
			  default: 
				llwarns << "Unknown Local Snapshot format" << llendl;
				return true;
			}

			formatted->enableOverSize() ;
			formatted->encode(raw, 0);
			formatted->disableOverSize() ;
			gViewerWindow->saveImageNumbered(formatted);
		}
		return true;
	}
};

class LLFileQuit : public view_listener_t
{
	bool handleEvent(const LLSD& userdata)
	{
		LLAppViewer::instance()->userQuit();
		return true;
	}
};


void handle_compress_image(void*)
{
	LLFilePicker& picker = LLFilePicker::instance();
	if (picker.getMultipleOpenFiles(LLFilePicker::FFLOAD_IMAGE))
	{
		std::string infile = picker.getFirstFile();
		while (!infile.empty())
		{
			std::string outfile = infile + ".j2c";

			llinfos << "Input:  " << infile << llendl;
			llinfos << "Output: " << outfile << llendl;

			BOOL success;

			success = LLViewerTextureList::createUploadFile(infile, outfile, IMG_CODEC_TGA);

			if (success)
			{
				llinfos << "Compression complete" << llendl;
			}
			else
			{
				llinfos << "Compression failed: " << LLImage::getLastError() << llendl;
			}

			infile = picker.getNextFile();
		}
	}
}

LLUUID upload_new_resource(
	const std::string& src_filename,
	std::string name,
	std::string desc,
	S32 compression_info,
	LLFolderType::EType destination_folder_type,
	LLInventoryType::EType inv_type,
	U32 next_owner_perms,
	U32 group_perms,
	U32 everyone_perms,
	const std::string& display_name,
	LLAssetStorage::LLStoreAssetCallback callback,
	S32 expected_upload_cost,
	void *userdata)
{	
	// Generate the temporary UUID.
	std::string filename = gDirUtilp->getTempFilename();
	LLTransactionID tid;
	LLAssetID uuid;
	
	LLSD args;

	std::string exten = gDirUtilp->getExtension(src_filename);
	LLAssetType::EType asset_type = LLAssetType::AT_NONE;
	std::string error_message;

	BOOL error = FALSE;
	
	if (exten.empty())
	{
		std::string short_name = gDirUtilp->getBaseFileName(filename);
		
		// No extension
		error_message = llformat(
				"No file extension for the file: '%s'\nPlease make sure the file has a correct file extension",
				short_name.c_str());
		args["FILE"] = short_name;
 		upload_error(error_message, "NoFileExtension", filename, args);
		return LLUUID();
	}
	else if( exten == "bmp")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(src_filename,
												 filename,
												 IMG_CODEC_BMP ))
		{
			error_message = llformat( "Problem with file %s:\n\n%s\n",
					src_filename.c_str(), LLImage::getLastError().c_str());
			args["FILE"] = src_filename;
			args["ERROR"] = LLImage::getLastError();
			upload_error(error_message, "ProblemWithFile", filename, args);
			return LLUUID();
		}
	}
	else if( exten == "tga")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(src_filename,
												 filename,
												 IMG_CODEC_TGA ))
		{
			error_message = llformat("Problem with file %s:\n\n%s\n",
					src_filename.c_str(), LLImage::getLastError().c_str());
			args["FILE"] = src_filename;
			args["ERROR"] = LLImage::getLastError();
			upload_error(error_message, "ProblemWithFile", filename, args);
			return LLUUID();
		}
	}
	else if( exten == "jpg" || exten == "jpeg")
	{
		asset_type = LLAssetType::AT_TEXTURE;
		if (!LLViewerTextureList::createUploadFile(src_filename,
												 filename,
												 IMG_CODEC_JPEG ))
		{
			error_message = llformat("Problem with file %s:\n\n%s\n",
					src_filename.c_str(), LLImage::getLastError().c_str());
			args["FILE"] = src_filename;
			args["ERROR"] = LLImage::getLastError();
			upload_error(error_message, "ProblemWithFile", filename, args);
			return LLUUID();
		}
	}
 	else if( exten == "png")
 	{
 		asset_type = LLAssetType::AT_TEXTURE;
 		if (!LLViewerTextureList::createUploadFile(src_filename,
 												 filename,
 												 IMG_CODEC_PNG ))
 		{
 			error_message = llformat("Problem with file %s:\n\n%s\n",
 					src_filename.c_str(), LLImage::getLastError().c_str());
 			args["FILE"] = src_filename;
 			args["ERROR"] = LLImage::getLastError();
 			upload_error(error_message, "ProblemWithFile", filename, args);
 			return LLUUID();
 		}
 	}
	else if(exten == "wav")
	{
		asset_type = LLAssetType::AT_SOUND;  // tag it as audio
		S32 encode_result = 0;

		llinfos << "Attempting to encode wav as an ogg file" << llendl;

		encode_result = encode_vorbis_file(src_filename, filename);
		
		if (LLVORBISENC_NOERR != encode_result)
		{
			switch(encode_result)
			{
				case LLVORBISENC_DEST_OPEN_ERR:
				    error_message = llformat( "Couldn't open temporary compressed sound file for writing: %s\n", filename.c_str());
					args["FILE"] = filename;
					upload_error(error_message, "CannotOpenTemporarySoundFile", filename, args);
					break;

				default:	
				  error_message = llformat("Unknown vorbis encode failure on: %s\n", src_filename.c_str());
					args["FILE"] = src_filename;
					upload_error(error_message, "UnknownVorbisEncodeFailure", filename, args);
					break;	
			}	
			return LLUUID();
		}
	}
	else if(exten == "tmp")	 	
	{	 	
		// This is a generic .lin resource file	 	
         asset_type = LLAssetType::AT_OBJECT;	 	
         LLFILE* in = LLFile::fopen(src_filename, "rb");		/* Flawfinder: ignore */	 	
         if (in)	 	
         {	 	
                 // read in the file header	 	
                 char buf[16384];		/* Flawfinder: ignore */ 	
                 size_t readbytes;
                 S32  version;	 	
                 if (fscanf(in, "LindenResource\nversion %d\n", &version))	 	
                 {	 	
                         if (2 == version)	 	
                         {
								// *NOTE: This buffer size is hard coded into scanf() below.
                                 char label[MAX_STRING];		/* Flawfinder: ignore */	 	
                                 char value[MAX_STRING];		/* Flawfinder: ignore */	 	
                                 S32  tokens_read;	 	
                                 while (fgets(buf, 1024, in))	 	
                                 {	 	
                                         label[0] = '\0';	 	
                                         value[0] = '\0';	 	
                                         tokens_read = sscanf(	/* Flawfinder: ignore */
											 buf,
											 "%254s %254s\n",
											 label, value);	 	

                                         llinfos << "got: " << label << " = " << value	 	
                                                         << llendl;	 	

                                         if (EOF == tokens_read)	 	
                                         {	 	
                                                 fclose(in);	 	
                                                 error_message = llformat("corrupt resource file: %s", src_filename.c_str());
												 args["FILE"] = src_filename;
												 upload_error(error_message, "CorruptResourceFile", filename, args);
                                                 return LLUUID();
                                         }	 	

                                         if (2 == tokens_read)	 	
                                         {	 	
                                                 if (! strcmp("type", label))	 	
                                                 {	 	
                                                         asset_type = (LLAssetType::EType)(atoi(value));	 	
                                                 }	 	
                                         }	 	
                                         else	 	
                                         {	 	
                                                 if (! strcmp("_DATA_", label))	 	
                                                 {	 	
                                                         // below is the data section	 	
                                                         break;	 	
                                                 }	 	
                                         }	 	
                                         // other values are currently discarded	 	
                                 }	 	

                         }	 	
                         else	 	
                         {	 	
                                 fclose(in);	 	
                                 error_message = llformat("unknown linden resource file version in file: %s", src_filename.c_str());
								 args["FILE"] = src_filename;
								 upload_error(error_message, "UnknownResourceFileVersion", filename, args);
                                 return LLUUID();
                         }	 	
                 }	 	
                 else	 	
                 {	 	
                         // this is an original binary formatted .lin file	 	
                         // start over at the beginning of the file	 	
                         fseek(in, 0, SEEK_SET);	 	

                         const S32 MAX_ASSET_DESCRIPTION_LENGTH = 256;	 	
                         const S32 MAX_ASSET_NAME_LENGTH = 64;	 	
                         S32 header_size = 34 + MAX_ASSET_DESCRIPTION_LENGTH + MAX_ASSET_NAME_LENGTH;	 	
                         S16     type_num;	 	

                         // read in and throw out most of the header except for the type	 	
                         if (fread(buf, header_size, 1, in) != 1)
						 {
							 llwarns << "Short read" << llendl;
						 }
                         memcpy(&type_num, buf + 16, sizeof(S16));		/* Flawfinder: ignore */	 	
                         asset_type = (LLAssetType::EType)type_num;	 	
                 }	 	

                 // copy the file's data segment into another file for uploading	 	
                 LLFILE* out = LLFile::fopen(filename, "wb");		/* Flawfinder: ignore */	
                 if (out)	 	
                 {	 	
                         while((readbytes = fread(buf, 1, 16384, in)))		/* Flawfinder: ignore */	 	
                         {	 	
							 if (fwrite(buf, 1, readbytes, out) != readbytes)
							 {
								 llwarns << "Short write" << llendl;
							 }
                         }	 	
                         fclose(out);	 	
                 }	 	
                 else	 	
                 {	 	
                         fclose(in);	 	
                         error_message = llformat( "Unable to create output file: %s", filename.c_str());
						 args["FILE"] = filename;
						 upload_error(error_message, "UnableToCreateOutputFile", filename, args);
                         return LLUUID();
                 }	 	

                 fclose(in);	 	
         }	 	
         else	 	
         {	 	
                 llinfos << "Couldn't open .lin file " << src_filename << llendl;	 	
         }	 	
	}
	else if (exten == "bvh")
	{
		error_message = llformat("We do not currently support bulk upload of animation files\n");
		upload_error(error_message, "DoNotSupportBulkAnimationUpload", filename, args);
		return LLUUID();
	}
	else
	{
		// Unknown extension
		error_message = llformat(LLTrans::getString("UnknownFileExtension").c_str(), exten.c_str());
		error = TRUE;;
	}

	// gen a new transaction ID for this asset
	tid.generate();

	if (!error)
	{
		uuid = tid.makeAssetID(gAgent.getSecureSessionID());
		// copy this file into the vfs for upload
		S32 file_size;
		LLAPRFile infile ;
		infile.open(filename, LL_APR_RB, NULL, &file_size);
		if (infile.getFileHandle())
		{
			LLVFile file(gVFS, uuid, asset_type, LLVFile::WRITE);

			file.setMaxSize(file_size);

			const S32 buf_size = 65536;
			U8 copy_buf[buf_size];
			while ((file_size = infile.read(copy_buf, buf_size)))
			{
				file.write(copy_buf, file_size);
			}
		}
		else
		{
			error_message = llformat( "Unable to access output file: %s", filename.c_str());
			error = TRUE;
		}
	}

	if (!error)
	{
		std::string t_disp_name = display_name;
		if (t_disp_name.empty())
		{
			t_disp_name = src_filename;
		}
		upload_new_resource(
			tid,
			asset_type,
			name,
			desc,
			compression_info, // tid
			destination_folder_type,
			inv_type,
			next_owner_perms,
			group_perms,
			everyone_perms,
			display_name,
			callback,
			expected_upload_cost,
			userdata);
	}
	else
	{
		llwarns << error_message << llendl;
		LLSD args;
		args["ERROR_MESSAGE"] = error_message;
		LLNotificationsUtil::add("ErrorMessage", args);
		if(LLFile::remove(filename) == -1)
		{
			lldebugs << "unable to remove temp file" << llendl;
		}
		LLFilePicker::instance().reset();
	}

	return uuid;
}

void upload_done_callback(
	const LLUUID& uuid,
	void* user_data,
	S32 result,
	LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLResourceData* data = (LLResourceData*)user_data;
	S32 expected_upload_cost = data ? data->mExpectedUploadCost : 0;
	//LLAssetType::EType pref_loc = data->mPreferredLocation;
	BOOL is_balance_sufficient = TRUE;

	if(data)
	{
		if (result >= 0)
		{
			LLFolderType::EType dest_loc = (data->mPreferredLocation == LLFolderType::FT_NONE) ? LLFolderType::assetTypeToFolderType(data->mAssetInfo.mType) : data->mPreferredLocation;
			
			if (LLAssetType::AT_SOUND == data->mAssetInfo.mType ||
			    LLAssetType::AT_TEXTURE == data->mAssetInfo.mType ||
			    LLAssetType::AT_ANIMATION == data->mAssetInfo.mType)
			{
				// Charge the user for the upload.
				LLViewerRegion* region = gAgent.getRegion();
				
				if(!(can_afford_transaction(expected_upload_cost)))
				{
					LLStringUtil::format_map_t args;
					args["NAME"] = data->mAssetInfo.getName();
					args["AMOUNT"] = llformat("%d", expected_upload_cost);
					LLBuyCurrencyHTML::openCurrencyFloater( LLTrans::getString("UploadingCosts", args), expected_upload_cost );
					is_balance_sufficient = FALSE;
				}
				else if(region)
				{
					// Charge user for upload
					gStatusBar->debitBalance(expected_upload_cost);
					
					LLMessageSystem* msg = gMessageSystem;
					msg->newMessageFast(_PREHASH_MoneyTransferRequest);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->nextBlockFast(_PREHASH_MoneyData);
					msg->addUUIDFast(_PREHASH_SourceID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_DestID, LLUUID::null);
					msg->addU8("Flags", 0);
					// we tell the sim how much we were expecting to pay so it
					// can respond to any discrepancy
					msg->addS32Fast(_PREHASH_Amount, expected_upload_cost);
					msg->addU8Fast(_PREHASH_AggregatePermNextOwner, (U8)LLAggregatePermissions::AP_EMPTY);
					msg->addU8Fast(_PREHASH_AggregatePermInventory, (U8)LLAggregatePermissions::AP_EMPTY);
					msg->addS32Fast(_PREHASH_TransactionType, TRANS_UPLOAD_CHARGE);
					msg->addStringFast(_PREHASH_Description, NULL);
					msg->sendReliable(region->getHost());
				}
			}

			if(is_balance_sufficient)
			{
				// Actually add the upload to inventory
				llinfos << "Adding " << uuid << " to inventory." << llendl;
				const LLUUID folder_id = gInventory.findCategoryUUIDForType(dest_loc);
				if(folder_id.notNull())
				{
					U32 next_owner_perms = data->mNextOwnerPerm;
					if(PERM_NONE == next_owner_perms)
					{
						next_owner_perms = PERM_MOVE | PERM_TRANSFER;
					}
					create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							      folder_id, data->mAssetInfo.mTransactionID, data->mAssetInfo.getName(),
							      data->mAssetInfo.getDescription(), data->mAssetInfo.mType,
							      data->mInventoryType, NOT_WEARABLE, next_owner_perms,
							      LLPointer<LLInventoryCallback>(NULL));
				}
				else
				{
					llwarns << "Can't find a folder to put it in" << llendl;
				}
			}
		}
		else // 	if(result >= 0)
		{
			LLSD args;
			args["FILE"] = LLInventoryType::lookupHumanReadable(data->mInventoryType);
			args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
			LLNotificationsUtil::add("CannotUploadReason", args);
		}
	}

	LLUploadDialog::modalUploadFinished();
	delete data;
	data = NULL;

	// *NOTE: This is a pretty big hack. What this does is check the
	// file picker if there are any more pending uploads. If so,
	// upload that file.
	const std::string& next_file = LLFilePicker::instance().getNextFile();
	if(is_balance_sufficient && !next_file.empty())
	{
		std::string asset_name = gDirUtilp->getBaseFileName(next_file, true);
		LLStringUtil::replaceNonstandardASCII( asset_name, '?' );
		LLStringUtil::replaceChar(asset_name, '|', '?');
		LLStringUtil::stripNonprintable(asset_name);
		LLStringUtil::trim(asset_name);

		std::string display_name = LLStringUtil::null;
		LLAssetStorage::LLStoreAssetCallback callback = NULL;
		void *userdata = NULL;
		upload_new_resource(
			next_file,
			asset_name,
			asset_name,	// file
			0,
			LLFolderType::FT_NONE,
			LLInventoryType::IT_NONE,
			PERM_NONE,
			PERM_NONE,
			PERM_NONE,
			display_name,
			callback,
			expected_upload_cost, // assuming next in a group of uploads is of roughly the same type, i.e. same upload cost
			userdata);
	}
}

static LLAssetID upload_new_resource_prep(
	const LLTransactionID& tid,
	LLAssetType::EType asset_type,
	LLInventoryType::EType& inventory_type,
	std::string& name,
	const std::string& display_name,
	std::string& description)
{
	LLAssetID uuid = generate_asset_id_for_new_upload(tid);

	increase_new_upload_stats(asset_type);

	assign_defaults_and_show_upload_message(
		asset_type,
		inventory_type,
		name,
		display_name,
		description);

	return uuid;
}

LLSD generate_new_resource_upload_capability_body(
	LLAssetType::EType asset_type,
	const std::string& name,
	const std::string& desc,
	LLFolderType::EType destination_folder_type,
	LLInventoryType::EType inv_type,
	U32 next_owner_perms,
	U32 group_perms,
	U32 everyone_perms)
{
	LLSD body;

	body["folder_id"] = gInventory.findCategoryUUIDForType(
		(destination_folder_type == LLFolderType::FT_NONE) ?
		(LLFolderType::EType) asset_type :
		destination_folder_type);

	body["asset_type"] = LLAssetType::lookup(asset_type);
	body["inventory_type"] = LLInventoryType::lookup(inv_type);
	body["name"] = name;
	body["description"] = desc;
	body["next_owner_mask"] = LLSD::Integer(next_owner_perms);
	body["group_mask"] = LLSD::Integer(group_perms);
	body["everyone_mask"] = LLSD::Integer(everyone_perms);

	return body;
}

void upload_new_resource(
	const LLTransactionID &tid,
	LLAssetType::EType asset_type,
	std::string name,
	std::string desc,
	S32 compression_info,
	LLFolderType::EType destination_folder_type,
	LLInventoryType::EType inv_type,
	U32 next_owner_perms,
	U32 group_perms,
	U32 everyone_perms,
	const std::string& display_name,
	LLAssetStorage::LLStoreAssetCallback callback,
	S32 expected_upload_cost,
	void *userdata)
{
	if(gDisconnected)
	{
		return ;
	}
	
	LLAssetID uuid = 
		upload_new_resource_prep(
			tid,
			asset_type,
			inv_type,
			name,
			display_name,
			desc);
	
	if( LLAssetType::AT_SOUND == asset_type )
	{
		LLViewerStats::getInstance()->incStat(LLViewerStats::ST_UPLOAD_SOUND_COUNT );
	}
	else
	if( LLAssetType::AT_TEXTURE == asset_type )
	{
		LLViewerStats::getInstance()->incStat(LLViewerStats::ST_UPLOAD_TEXTURE_COUNT );
	}
	else
	if( LLAssetType::AT_ANIMATION == asset_type)
	{
		LLViewerStats::getInstance()->incStat(LLViewerStats::ST_UPLOAD_ANIM_COUNT );
	}

	if(LLInventoryType::IT_NONE == inv_type)
	{
		inv_type = LLInventoryType::defaultForAssetType(asset_type);
	}
	LLStringUtil::stripNonprintable(name);
	LLStringUtil::stripNonprintable(desc);
	if(name.empty())
	{
		name = "(No Name)";
	}
	if(desc.empty())
	{
		desc = "(No Description)";
	}
	
	// At this point, we're ready for the upload.
	std::string upload_message = "Uploading...\n\n";
	upload_message.append(display_name);
	LLUploadDialog::modalUploadDialog(upload_message);

	llinfos << "*** Uploading: " << llendl;
	llinfos << "Type: " << LLAssetType::lookup(asset_type) << llendl;
	llinfos << "UUID: " << uuid << llendl;
	llinfos << "Name: " << name << llendl;
	llinfos << "Desc: " << desc << llendl;
	llinfos << "Expected Upload Cost: " << expected_upload_cost << llendl;
	lldebugs << "Folder: " << gInventory.findCategoryUUIDForType((destination_folder_type == LLFolderType::FT_NONE) ? LLFolderType::assetTypeToFolderType(asset_type) : destination_folder_type) << llendl;
	lldebugs << "Asset Type: " << LLAssetType::lookup(asset_type) << llendl;

	std::string url = gAgent.getRegion()->getCapability(
		"NewFileAgentInventory");

	if ( !url.empty() )
	{
		llinfos << "New Agent Inventory via capability" << llendl;

		LLSD body;
		body = generate_new_resource_upload_capability_body(
			asset_type,
			name,
			desc,
			destination_folder_type,
			inv_type,
			next_owner_perms,
			group_perms,
			everyone_perms);

		LLHTTPClient::post(
			url,
			body,
			new LLNewAgentInventoryResponder(
				body,
				uuid,
				asset_type));
	}
	else
	{
		llinfos << "NewAgentInventory capability not found, new agent inventory via asset system." << llendl;
		// check for adequate funds
		// TODO: do this check on the sim
		if (LLAssetType::AT_SOUND == asset_type ||
			LLAssetType::AT_TEXTURE == asset_type ||
			LLAssetType::AT_ANIMATION == asset_type)
		{
			S32 balance = gStatusBar->getBalance();
			if (balance < expected_upload_cost)
			{
				// insufficient funds, bail on this upload
				LLStringUtil::format_map_t args;
				args["NAME"] = name;
				args["AMOUNT"] = llformat("%d", expected_upload_cost);
				LLBuyCurrencyHTML::openCurrencyFloater( LLTrans::getString("UploadingCosts", args), expected_upload_cost );
				return;
			}
		}

		LLResourceData* data = new LLResourceData;
		data->mAssetInfo.mTransactionID = tid;
		data->mAssetInfo.mUuid = uuid;
		data->mAssetInfo.mType = asset_type;
		data->mAssetInfo.mCreatorID = gAgentID;
		data->mInventoryType = inv_type;
		data->mNextOwnerPerm = next_owner_perms;
		data->mExpectedUploadCost = expected_upload_cost;
		data->mUserData = userdata;
		data->mAssetInfo.setName(name);
		data->mAssetInfo.setDescription(desc);
		data->mPreferredLocation = destination_folder_type;

		LLAssetStorage::LLStoreAssetCallback asset_callback = &upload_done_callback;
		if (callback)
		{
			asset_callback = callback;
		}
		gAssetStorage->storeAssetData(
			data->mAssetInfo.mTransactionID,
			data->mAssetInfo.mType,
			asset_callback,
			(void*)data,
			FALSE);
	}
}

BOOL upload_new_variable_price_resource(
	const LLTransactionID &tid, 
	LLAssetType::EType asset_type,
	std::string name,
	std::string desc, 
	LLFolderType::EType destination_folder_type,
	LLInventoryType::EType inv_type,
	U32 next_owner_perms,
	U32 group_perms,
	U32 everyone_perms,
	const std::string& display_name,
	const LLSD& asset_resources)
{
	LLAssetID uuid = 
		upload_new_resource_prep(
			tid,
			asset_type,
			inv_type,
			name,
			display_name,
			desc);

	llinfos << "*** Uploading: " << llendl;
	llinfos << "Type: " << LLAssetType::lookup(asset_type) << llendl;
	llinfos << "UUID: " << uuid << llendl;
	llinfos << "Name: " << name << llendl;
	llinfos << "Desc: " << desc << llendl;
	lldebugs << "Folder: "
		<< gInventory.findCategoryUUIDForType((destination_folder_type == LLFolderType::FT_NONE) ? (LLFolderType::EType)asset_type : destination_folder_type) << llendl;
	lldebugs << "Asset Type: " << LLAssetType::lookup(asset_type) << llendl;

	std::string url = gAgent.getRegion()->getCapability(
		"NewFileAgentInventoryVariablePrice");

	if ( !url.empty() )
	{
		lldebugs
			<< "New Agent Inventory variable price upload" << llendl;
		
		// Each of the two capabilities has similar data, so
		// let's reuse that code

		LLSD body;

		body = generate_new_resource_upload_capability_body(
			asset_type,
			name,
			desc,
			destination_folder_type,
			inv_type,
			next_owner_perms,
			group_perms,
			everyone_perms);

		body["asset_resources"] = asset_resources;

		LLHTTPClient::post(
			url,
			body,
			new LLNewAgentInventoryVariablePriceResponder(
				uuid,
				asset_type,
				body));

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

LLAssetID generate_asset_id_for_new_upload(const LLTransactionID& tid)
{
	if ( gDisconnected )
	{	
		LLAssetID rv;

		rv.setNull();
		return rv;
	}

	LLAssetID uuid = tid.makeAssetID(gAgent.getSecureSessionID());

	return uuid;
}

void increase_new_upload_stats(LLAssetType::EType asset_type)
{
	if ( LLAssetType::AT_SOUND == asset_type )
	{
		LLViewerStats::getInstance()->incStat(
			LLViewerStats::ST_UPLOAD_SOUND_COUNT );
	}
	else if ( LLAssetType::AT_TEXTURE == asset_type )
	{
		LLViewerStats::getInstance()->incStat(
			LLViewerStats::ST_UPLOAD_TEXTURE_COUNT );
	}
	else if ( LLAssetType::AT_ANIMATION == asset_type )
	{
		LLViewerStats::getInstance()->incStat(
			LLViewerStats::ST_UPLOAD_ANIM_COUNT );
	}
}

void assign_defaults_and_show_upload_message(
	LLAssetType::EType asset_type,
	LLInventoryType::EType& inventory_type,
	std::string& name,
	const std::string& display_name,
	std::string& description)
{
	if ( LLInventoryType::IT_NONE == inventory_type )
	{
		inventory_type = LLInventoryType::defaultForAssetType(asset_type);
	}
	LLStringUtil::stripNonprintable(name);
	LLStringUtil::stripNonprintable(description);

	if ( name.empty() )
	{
		name = "(No Name)";
	}
	if ( description.empty() )
	{
		description = "(No Description)";
	}

	// At this point, we're ready for the upload.
	std::string upload_message = "Uploading...\n\n";
	upload_message.append(display_name);
	LLUploadDialog::modalUploadDialog(upload_message);
}


void init_menu_file()
{
	view_listener_t::addCommit(new LLFileUploadImage(), "File.UploadImage");
	view_listener_t::addCommit(new LLFileUploadSound(), "File.UploadSound");
	view_listener_t::addCommit(new LLFileUploadAnim(), "File.UploadAnim");
	view_listener_t::addCommit(new LLFileUploadModel(), "File.UploadModel");
	view_listener_t::addCommit(new LLFileUploadBulk(), "File.UploadBulk");
	view_listener_t::addCommit(new LLFileCloseWindow(), "File.CloseWindow");
	view_listener_t::addCommit(new LLFileCloseAllWindows(), "File.CloseAllWindows");
	view_listener_t::addEnable(new LLFileEnableCloseWindow(), "File.EnableCloseWindow");
	view_listener_t::addEnable(new LLFileEnableCloseAllWindows(), "File.EnableCloseAllWindows");
	view_listener_t::addCommit(new LLFileTakeSnapshotToDisk(), "File.TakeSnapshotToDisk");
	view_listener_t::addCommit(new LLFileQuit(), "File.Quit");

	view_listener_t::addEnable(new LLFileEnableUpload(), "File.EnableUpload");
	view_listener_t::addEnable(new LLFileEnableUploadModel(), "File.EnableUploadModel");
	view_listener_t::addMenu(new LLMeshEnabled(), "File.MeshEnabled");
	view_listener_t::addMenu(new LLMeshUploadVisible(), "File.VisibleUploadModel");

	// "File.SaveTexture" moved to llpanelmaininventory so that it can be properly handled.
}
