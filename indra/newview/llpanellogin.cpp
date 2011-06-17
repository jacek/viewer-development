/** 
 * @file llpanellogin.cpp
 * @brief Login dialog and logo display
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

#include "llpanellogin.h"

#include "indra_constants.h"		// for key and mask constants
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llmd5.h"
#include "llsecondlifeurls.h"
#include "v4color.h"

#include "llappviewer.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcommandhandler.h"		// for secondlife:///app/login/
#include "llcombobox.h"
#include "llcurl.h"
#include "llviewercontrol.h"
#include "llfloaterpreference.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llsecapi.h"
#include "llstartup.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llslurl.h"
#include "llversioninfo.h"
#include "llviewerhelp.h"
#include "llviewertexturelist.h"
#include "llviewermenu.h"			// for handle_preferences()
#include "llviewernetwork.h"
#include "llviewerwindow.h"			// to link into child list
#include "lluictrlfactory.h"
#include "llhttpclient.h"
#include "llweb.h"
#include "llmediactrl.h"
#include "llrootview.h"

#include "llfloatertos.h"
#include "lltrans.h"
#include "llglheaders.h"
#include "llpanelloginlistener.h"

#if LL_WINDOWS
#pragma warning(disable: 4355)      // 'this' used in initializer list
#endif  // LL_WINDOWS

#include "llsdserialize.h"

const S32 BLACK_BORDER_HEIGHT = 160;
const S32 MAX_PASSWORD = 16;

LLPanelLogin *LLPanelLogin::sInstance = NULL;
BOOL LLPanelLogin::sCapslockDidNotification = FALSE;

// Helper for converting a user name into the canonical "Firstname Lastname" form.
// For new accounts without a last name "Resident" is added as a last name.
static std::string canonicalize_username(const std::string& name);

class LLLoginRefreshHandler : public LLCommandHandler
{
public:
	// don't allow from external browsers
	LLLoginRefreshHandler() : LLCommandHandler("login_refresh", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web)
	{	
		if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
		{
			LLPanelLogin::loadLoginPage();
		}	
		return true;
	}
};

//---------------------------------------------------------------------------
// Public methods
//---------------------------------------------------------------------------
LLPanelLogin::LLPanelLogin(const LLRect &rect,
						 BOOL show_server,
						 void (*callback)(S32 option, void* user_data),
						 void *cb_data)
:	LLPanel(),
	mLogoImage(),
	mCallback(callback),
	mCallbackData(cb_data),
	mListener(new LLPanelLoginListener(this))
{
	setBackgroundVisible(FALSE);
	setBackgroundOpaque(TRUE);

	// instance management
	if (LLPanelLogin::sInstance)
	{
		llwarns << "Duplicate instance of login view deleted" << llendl;
		// Don't leave bad pointer in gFocusMgr
		gFocusMgr.setDefaultKeyboardFocus(NULL);

		delete LLPanelLogin::sInstance;
	}

	mPasswordModified = FALSE;
	LLPanelLogin::sInstance = this;

	LLView* login_holder = gViewerWindow->getLoginPanelHolder();
	if (login_holder)
	{
		login_holder->addChild(this);
	}

	// Logo
	mLogoImage = LLUI::getUIImage("startup_logo");

	buildFromFile( "panel_login.xml");
	
	reshape(rect.getWidth(), rect.getHeight());

	getChild<LLLineEditor>("password_edit")->setKeystrokeCallback(onPassKey, this);

	// change z sort of clickable text to be behind buttons
	sendChildToBack(getChildView("forgot_password_text"));

	if(LLStartUp::getStartSLURL().getType() != LLSLURL::LOCATION)
	{
		LLSLURL slurl(gSavedSettings.getString("LoginLocation"));
		LLStartUp::setStartSLURL(slurl);
	}
	updateLocationCombo(false);

	LLUICtrl& mode_combo = getChildRef<LLUICtrl>("mode_combo");
	mode_combo.setValue(gSavedSettings.getString("SessionSettingsFile"));
	mode_combo.setCommitCallback(boost::bind(&LLPanelLogin::onModeChange, this, getChild<LLUICtrl>("mode_combo")->getValue(), _2));

	LLComboBox* server_choice_combo = sInstance->getChild<LLComboBox>("server_combo");
	server_choice_combo->setCommitCallback(onSelectServer, NULL);
	server_choice_combo->setFocusLostCallback(boost::bind(onServerComboLostFocus, _1));
	updateServerCombo();

	childSetAction("connect_btn", onClickConnect, this);

	getChild<LLPanel>("login")->setDefaultBtn("connect_btn");

	std::string channel = LLVersionInfo::getChannel();
	std::string version = llformat("%s (%d)",
								   LLVersionInfo::getShortVersion().c_str(),
								   LLVersionInfo::getBuild());
	//LLTextBox* channel_text = getChild<LLTextBox>("channel_text");
	//channel_text->setTextArg("[CHANNEL]", channel); // though not displayed
	//channel_text->setTextArg("[VERSION]", version);
	//channel_text->setClickedCallback(onClickVersion, this);
	
	LLTextBox* forgot_password_text = getChild<LLTextBox>("forgot_password_text");
	forgot_password_text->setClickedCallback(onClickForgotPassword, NULL);

	LLTextBox* create_new_account_text = getChild<LLTextBox>("create_new_account_text");
	create_new_account_text->setClickedCallback(onClickNewAccount, NULL);

	LLTextBox* need_help_text = getChild<LLTextBox>("login_help");
	need_help_text->setClickedCallback(onClickHelp, NULL);
	
	// get the web browser control
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	web_browser->addObserver(this);
	
	reshapeBrowser();

	loadLoginPage();
			
	// Show last logged in user favorites in "Start at" combo.
	addUsersWithFavoritesToUsername();
	getChild<LLComboBox>("username_combo")->setTextChangedCallback(boost::bind(&LLPanelLogin::addFavoritesToStartLocation, this));

	updateLocationCombo(false);

}

void LLPanelLogin::addUsersWithFavoritesToUsername()
{
	LLComboBox* combo = getChild<LLComboBox>("username_combo");
	if (!combo) return;
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites.xml");
	LLSD fav_llsd;
	llifstream file;
	file.open(filename);
	if (!file.is_open()) return;
	LLSDSerialize::fromXML(fav_llsd, file);
	for (LLSD::map_const_iterator iter = fav_llsd.beginMap();
		iter != fav_llsd.endMap(); ++iter)
	{
		combo->add(iter->first);
	}
}

void LLPanelLogin::addFavoritesToStartLocation()
{
	LLComboBox* combo = getChild<LLComboBox>("start_location_combo");
	if (!combo) return;
	int num_items = combo->getItemCount();
	for (int i = num_items - 1; i > 2; i--)
	{
		combo->remove(i);
	}
	std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites.xml");
	LLSD fav_llsd;
	llifstream file;
	file.open(filename);
	if (!file.is_open()) return;
	LLSDSerialize::fromXML(fav_llsd, file);
	for (LLSD::map_const_iterator iter = fav_llsd.beginMap();
		iter != fav_llsd.endMap(); ++iter)
	{
		std::string user_defined_name = getChild<LLComboBox>("username_combo")->getSimple();

		// The account name in stored_favorites.xml has Resident last name even if user has
		// a single word account name, so it can be compared case-insensitive with the
		// user defined "firstname lastname".
		S32 res = LLStringUtil::compareInsensitive(canonicalize_username(user_defined_name), iter->first);
		if (res != 0) continue;

		combo->addSeparator();
		LLSD user_llsd = iter->second;
		for (LLSD::array_const_iterator iter1 = user_llsd.beginArray();
			iter1 != user_llsd.endArray(); ++iter1)
		{
			std::string label = (*iter1)["name"].asString();
			std::string value = (*iter1)["slurl"].asString();
			if(label != "" && value != "")
			{
				combo->add(label, value);
			}
		}
		break;
	}
}

// force the size to be correct (XML doesn't seem to be sufficient to do this)
// (with some padding so the other login screen doesn't show through)
void LLPanelLogin::reshapeBrowser()
{
	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("login_html");
	LLRect rect = gViewerWindow->getWindowRectScaled();
	LLRect html_rect;
	html_rect.setCenterAndSize(
		rect.getCenterX() - 2, rect.getCenterY() + 40,
		rect.getWidth() + 6, rect.getHeight() - 78 );
	web_browser->setRect( html_rect );
	web_browser->reshape( html_rect.getWidth(), html_rect.getHeight(), TRUE );
	reshape( rect.getWidth(), rect.getHeight(), 1 );
}

LLPanelLogin::~LLPanelLogin()
{
	LLPanelLogin::sInstance = NULL;

	// Controls having keyboard focus by default
	// must reset it on destroy. (EXT-2748)
	gFocusMgr.setDefaultKeyboardFocus(NULL);
}

// virtual
void LLPanelLogin::draw()
{
	glPushMatrix();
	{
		F32 image_aspect = 1.333333f;
		F32 view_aspect = (F32)getRect().getWidth() / (F32)getRect().getHeight();
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			glTranslatef(-0.5f * (image_aspect / view_aspect - 1.f) * getRect().getWidth(), 0.f, 0.f);
			glScalef(image_aspect / view_aspect, 1.f, 1.f);
		}

		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();

		if (getChild<LLView>("login_widgets")->getVisible())
		{
			// draw a background box in black
			gl_rect_2d( 0, height - 264, width, 264, LLColor4::black );
			// draw the bottom part of the background image
			// just the blue background to the native client UI
			mLogoImage->draw(0, -264, width + 8, mLogoImage->getHeight());
		};
	}
	glPopMatrix();

	LLPanel::draw();
}

// virtual
BOOL LLPanelLogin::handleKeyHere(KEY key, MASK mask)
{
	if ( KEY_F1 == key )
	{
		LLViewerHelp* vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->f1HelpTopic());
		return TRUE;
	}

	return LLPanel::handleKeyHere(key, mask);
}

// virtual 
void LLPanelLogin::setFocus(BOOL b)
{
	if(b != hasFocus())
	{
		if(b)
		{
			LLPanelLogin::giveFocus();
		}
		else
		{
			LLPanel::setFocus(b);
		}
	}
}

// static
void LLPanelLogin::giveFocus()
{
	if( sInstance )
	{
		// Grab focus and move cursor to first blank input field
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		std::string pass = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

		BOOL have_username = !username.empty();
		BOOL have_pass = !pass.empty();

		LLLineEditor* edit = NULL;
		LLComboBox* combo = NULL;
		if (have_username && !have_pass)
		{
			// User saved his name but not his password.  Move
			// focus to password field.
			edit = sInstance->getChild<LLLineEditor>("password_edit");
		}
		else
		{
			// User doesn't have a name, so start there.
			combo = sInstance->getChild<LLComboBox>("username_combo");
		}

		if (edit)
		{
			edit->setFocus(TRUE);
			edit->selectAll();
		}
		else if (combo)
		{
			combo->setFocus(TRUE);
		}
	}
}

// static
void LLPanelLogin::showLoginWidgets()
{
	// *NOTE: Mani - This may or may not be obselete code.
	// It seems to be part of the defunct? reg-in-client project.
	sInstance->getChildView("login_widgets")->setVisible( true);
	LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
	sInstance->reshapeBrowser();
	// *TODO: Append all the usual login parameters, like first_login=Y etc.
	std::string splash_screen_url = LLGridManager::getInstance()->getLoginPage();
	web_browser->navigateTo( splash_screen_url, "text/html" );
	LLUICtrl* username_combo = sInstance->getChild<LLUICtrl>("username_combo");
	username_combo->setFocus(TRUE);
}

// static
void LLPanelLogin::show(const LLRect &rect,
						BOOL show_server,
						void (*callback)(S32 option, void* user_data),
						void* callback_data)
{
	new LLPanelLogin(rect, show_server, callback, callback_data);

	if( !gFocusMgr.getKeyboardFocus() )
	{
		// Grab focus and move cursor to first enabled control
		sInstance->setFocus(TRUE);
	}

	// Make sure that focus always goes here (and use the latest sInstance that was just created)
	gFocusMgr.setDefaultKeyboardFocus(sInstance);
}

// static
void LLPanelLogin::setFields(LLPointer<LLCredential> credential,
							 BOOL remember)
{
	if (!sInstance)
	{
		llwarns << "Attempted fillFields with no login view shown" << llendl;
		return;
	}
	LL_INFOS("Credentials") << "Setting login fields to " << *credential << LL_ENDL;

	LLSD identifier = credential->getIdentifier();
	if((std::string)identifier["type"] == "agent") 
	{
		std::string firstname = identifier["first_name"].asString();
		std::string lastname = identifier["last_name"].asString();
	    std::string login_id = firstname;
	    if (!lastname.empty() && lastname != "Resident")
	    {
		    // support traditional First Last name SLURLs
		    login_id += " ";
		    login_id += lastname;
	    }
		sInstance->getChild<LLComboBox>("username_combo")->setLabel(login_id);	
	}
	else if((std::string)identifier["type"] == "account")
	{
		sInstance->getChild<LLComboBox>("username_combo")->setLabel((std::string)identifier["account_name"]);		
	}
	else
	{
	  sInstance->getChild<LLComboBox>("username_combo")->setLabel(std::string());	
	}
	sInstance->addFavoritesToStartLocation();
	// if the password exists in the credential, set the password field with
	// a filler to get some stars
	LLSD authenticator = credential->getAuthenticator();
	LL_INFOS("Credentials") << "Setting authenticator field " << authenticator["type"].asString() << LL_ENDL;
	if(authenticator.isMap() && 
	   authenticator.has("secret") && 
	   (authenticator["secret"].asString().size() > 0))
	{
		
		// This is a MD5 hex digest of a password.
		// We don't actually use the password input field, 
		// fill it with MAX_PASSWORD characters so we get a 
		// nice row of asterixes.
		const std::string filler("123456789!123456");
		sInstance->getChild<LLUICtrl>("password_edit")->setValue(std::string("123456789!123456"));
	}
	else
	{
		sInstance->getChild<LLUICtrl>("password_edit")->setValue(std::string());		
	}
	sInstance->getChild<LLUICtrl>("remember_check")->setValue(remember);
}


// static
void LLPanelLogin::getFields(LLPointer<LLCredential>& credential,
							 BOOL& remember)
{
	if (!sInstance)
	{
		llwarns << "Attempted getFields with no login view shown" << llendl;
		return;
	}
	
	// load the credential so we can pass back the stored password or hash if the user did
	// not modify the password field.
	
	credential = gSecAPIHandler->loadCredential(LLGridManager::getInstance()->getGrid());

	LLSD identifier = LLSD::emptyMap();
	LLSD authenticator = LLSD::emptyMap();
	
	if(credential.notNull())
	{
		authenticator = credential->getAuthenticator();
	}

	std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
	LLStringUtil::trim(username);
	std::string password = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();

	LL_INFOS2("Credentials", "Authentication") << "retrieving username:" << username << LL_ENDL;
	// determine if the username is a first/last form or not.
	size_t separator_index = username.find_first_of(' ');
	if (separator_index == username.npos
		&& !LLGridManager::getInstance()->isSystemGrid())
	{
		LL_INFOS2("Credentials", "Authentication") << "account: " << username << LL_ENDL;
		// single username, so this is a 'clear' identifier
		identifier["type"] = CRED_IDENTIFIER_TYPE_ACCOUNT;
		identifier["account_name"] = username;
		
		if (LLPanelLogin::sInstance->mPasswordModified)
		{
			authenticator = LLSD::emptyMap();
			// password is plaintext
			authenticator["type"] = CRED_AUTHENTICATOR_TYPE_CLEAR;
			authenticator["secret"] = password;
		}
	}
	else
	{
		// Be lenient in terms of what separators we allow for two-word names
		// and allow legacy users to login with firstname.lastname
		separator_index = username.find_first_of(" ._");
		std::string first = username.substr(0, separator_index);
		std::string last;
		if (separator_index != username.npos)
		{
			last = username.substr(separator_index+1, username.npos);
		LLStringUtil::trim(last);
		}
		else
		{
			// ...on Linden grids, single username users as considered to have
			// last name "Resident"
			// *TODO: Make login.cgi support "account_name" like above
			last = "Resident";
		}
		
		if (last.find_first_of(' ') == last.npos)
		{
			LL_INFOS2("Credentials", "Authentication") << "agent: " << username << LL_ENDL;
			// traditional firstname / lastname
			identifier["type"] = CRED_IDENTIFIER_TYPE_AGENT;
			identifier["first_name"] = first;
			identifier["last_name"] = last;
		
			if (LLPanelLogin::sInstance->mPasswordModified)
			{
				authenticator = LLSD::emptyMap();
				authenticator["type"] = CRED_AUTHENTICATOR_TYPE_HASH;
				authenticator["algorithm"] = "md5";
				LLMD5 pass((const U8 *)password.c_str());
				char md5pass[33];               /* Flawfinder: ignore */
				pass.hex_digest(md5pass);
				authenticator["secret"] = md5pass;
			}
		}
	}
	credential = gSecAPIHandler->createCredential(LLGridManager::getInstance()->getGrid(), identifier, authenticator);
	remember = sInstance->getChild<LLUICtrl>("remember_check")->getValue();
}

// static
BOOL LLPanelLogin::isGridComboDirty()
{
	BOOL user_picked = FALSE;
	if (!sInstance)
	{
		llwarns << "Attempted getServer with no login view shown" << llendl;
	}
	else
	{
		LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
		user_picked = combo->isDirty();
	}
	return user_picked;
}

// static
BOOL LLPanelLogin::areCredentialFieldsDirty()
{
	if (!sInstance)
	{
		llwarns << "Attempted getServer with no login view shown" << llendl;
	}
	else
	{
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();
		LLStringUtil::trim(username);
		std::string password = sInstance->getChild<LLUICtrl>("password_edit")->getValue().asString();
		LLComboBox* combo = sInstance->getChild<LLComboBox>("username_combo");
		if(combo && combo->isDirty())
		{
			return true;
		}
		LLLineEditor* ctrl = sInstance->getChild<LLLineEditor>("password_edit");
		if(ctrl && ctrl->isDirty()) 
		{
			return true;
		}
	}
	return false;	
}


// static
void LLPanelLogin::updateLocationCombo( bool force_visible )
{
	if (!sInstance) 
	{
		return;
	}	
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("start_location_combo");
	
	switch(LLStartUp::getStartSLURL().getType())
	{
		case LLSLURL::LOCATION:
		{
			
			combo->setCurrentByIndex( 2 );	
			combo->setTextEntry(LLStartUp::getStartSLURL().getLocationString());	
			break;
		}
		case LLSLURL::HOME_LOCATION:
			combo->setCurrentByIndex(1);
			break;
		default:
			combo->setCurrentByIndex(0);
			break;
	}
	
	BOOL show_start = TRUE;
	
	if ( ! force_visible )
		show_start = gSavedSettings.getBOOL("ShowStartLocation");
	
	sInstance->getChildView("start_location_combo")->setVisible( show_start);
	sInstance->getChildView("start_location_text")->setVisible( show_start);
	
	BOOL show_server = gSavedSettings.getBOOL("ForceShowGrid");
	sInstance->getChildView("server_combo_text")->setVisible( show_server);	
	sInstance->getChildView("server_combo")->setVisible( show_server);
}

// static
void LLPanelLogin::updateStartSLURL()
{
	if (!sInstance) return;
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("start_location_combo");
	S32 index = combo->getCurrentIndex();
	
	switch (index)
	{
		case 0:
		{
			LLStartUp::setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_LAST));
			break;
		}			
		case 1:
		{
			LLStartUp::setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_HOME));
			break;
		}
		default:
		{
			LLSLURL slurl = LLSLURL(combo->getValue().asString());
			if(slurl.getType() == LLSLURL::LOCATION)
			{
				// we've changed the grid, so update the grid selection
				LLStartUp::setStartSLURL(slurl);
			}
			break;
		}			
	}
}


void LLPanelLogin::setLocation(const LLSLURL& slurl)
{
	LLStartUp::setStartSLURL(slurl);
	updateServer();
}

// static
void LLPanelLogin::closePanel()
{
	if (sInstance)
	{
		LLPanelLogin::sInstance->getParent()->removeChild( LLPanelLogin::sInstance );

		delete sInstance;
		sInstance = NULL;
	}
}

// static
void LLPanelLogin::setAlwaysRefresh(bool refresh)
{
	if (LLStartUp::getStartupState() >= STATE_LOGIN_CLEANUP) return;

	LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");

	if (web_browser)
	{
		web_browser->setAlwaysRefresh(refresh);
	}
}



void LLPanelLogin::loadLoginPage()
{
	if (!sInstance) return;
	
	std::ostringstream oStr;

	std::string login_page = LLGridManager::getInstance()->getLoginPage();

	oStr << login_page;
	
	// Use the right delimeter depending on how LLURI parses the URL
	LLURI login_page_uri = LLURI(login_page);
	
	std::string first_query_delimiter = "&";
	if (login_page_uri.queryMap().size() == 0)
	{
		first_query_delimiter = "?";
	}

	// Language
	std::string language = LLUI::getLanguage();
	oStr << first_query_delimiter<<"lang=" << language;
	
	// First Login?
	if (gSavedSettings.getBOOL("FirstLoginThisInstall"))
	{
		oStr << "&firstlogin=TRUE";
	}

	// Channel and Version
	std::string version = llformat("%s (%d)",
								   LLVersionInfo::getShortVersion().c_str(),
								   LLVersionInfo::getBuild());

	char* curl_channel = curl_escape(LLVersionInfo::getChannel().c_str(), 0);
	char* curl_version = curl_escape(version.c_str(), 0);

	oStr << "&channel=" << curl_channel;
	oStr << "&version=" << curl_version;

	curl_free(curl_channel);
	curl_free(curl_version);

	// Grid
	char* curl_grid = curl_escape(LLGridManager::getInstance()->getGridLabel().c_str(), 0);
	oStr << "&grid=" << curl_grid;
	curl_free(curl_grid);
	
	// add OS info
	char * os_info = curl_escape(LLAppViewer::instance()->getOSInfo().getOSStringSimple().c_str(), 0);
	oStr << "&os=" << os_info;
	curl_free(os_info);
	
	gViewerWindow->setMenuBackgroundColor(false, !LLGridManager::getInstance()->isInProductionGrid());
	
	LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
	if (web_browser->getCurrentNavUrl() != oStr.str())
	{
		web_browser->navigateTo( oStr.str(), "text/html" );
	}
}

void LLPanelLogin::handleMediaEvent(LLPluginClassMedia* /*self*/, EMediaEvent event)
{
	if(event == MEDIA_EVENT_NAVIGATE_COMPLETE)
	{
		LLMediaCtrl* web_browser = sInstance->getChild<LLMediaCtrl>("login_html");
		if (web_browser)
		{
			// *HACK HACK HACK HACK!
			/* Stuff a Tab key into the browser now so that the first field will
			** get the focus!  The embedded javascript on the page that properly
			** sets the initial focus in a real web browser is not working inside
			** the viewer, so this is an UGLY HACK WORKAROUND for now.
			*/
			// Commented out as it's not reliable
			//web_browser->handleKey(KEY_TAB, MASK_NONE, false);
		}
	}
}

//---------------------------------------------------------------------------
// Protected methods
//---------------------------------------------------------------------------

// static
void LLPanelLogin::onClickConnect(void *)
{
	if (sInstance && sInstance->mCallback)
	{
		// JC - Make sure the fields all get committed.
		sInstance->setFocus(FALSE);

		LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
		LLSD combo_val = combo->getSelectedValue();
		if (combo_val.isUndefined())
		{
			combo_val = combo->getValue();
		}
		if(combo_val.isUndefined())
		{
			LLNotificationsUtil::add("StartRegionEmpty");
			return;
		}		
		try
		{
			LLGridManager::getInstance()->setGridChoice(combo_val.asString());
		}
		catch (LLInvalidGridName ex)
		{
			LLSD args;
			args["GRID"] = combo_val.asString();
			LLNotificationsUtil::add("InvalidGrid", args);
			return;
		}
		updateStartSLURL();
		std::string username = sInstance->getChild<LLUICtrl>("username_combo")->getValue().asString();

		
		if(username.empty())
		{
			// user must type in something into the username field
			LLNotificationsUtil::add("MustHaveAccountToLogIn");
		}
		else
		{
			LLPointer<LLCredential> cred;
			BOOL remember;
			getFields(cred, remember);
			std::string identifier_type;
			cred->identifierType(identifier_type);
			LLSD allowed_credential_types;
			LLGridManager::getInstance()->getLoginIdentifierTypes(allowed_credential_types);
			
			// check the typed in credential type against the credential types expected by the server.
			for(LLSD::array_iterator i = allowed_credential_types.beginArray();
				i != allowed_credential_types.endArray();
				i++)
			{
				
				if(i->asString() == identifier_type)
				{
					// yay correct credential type
					sInstance->mCallback(0, sInstance->mCallbackData);
					return;
				}
			}
			
			// Right now, maingrid is the only thing that is picky about
			// credential format, as it doesn't yet allow account (single username)
			// format creds.  - Rox.  James, we wanna fix the message when we change
			// this.
			LLNotificationsUtil::add("InvalidCredentialFormat");			
		}
	}
}

// static
void LLPanelLogin::onClickNewAccount(void*)
{
	LLWeb::loadURLExternal(sInstance->getString("create_account_url"));
}


// static
void LLPanelLogin::onClickVersion(void*)
{
	LLFloaterReg::showInstance("sl_about"); 
}

//static
void LLPanelLogin::onClickForgotPassword(void*)
{
	if (sInstance )
	{
		LLWeb::loadURLExternal(sInstance->getString( "forgot_password_url" ));
	}
}

//static
void LLPanelLogin::onClickHelp(void*)
{
	if (sInstance)
	{
		LLViewerHelp* vhelp = LLViewerHelp::getInstance();
		vhelp->showTopic(vhelp->preLoginTopic());
	}
}

// static
void LLPanelLogin::onPassKey(LLLineEditor* caller, void* user_data)
{
	LLPanelLogin *This = (LLPanelLogin *) user_data;
	This->mPasswordModified = TRUE;
	if (gKeyboard->getKeyDown(KEY_CAPSLOCK) && sCapslockDidNotification == FALSE)
	{
// *TODO: use another way to notify user about enabled caps lock, see EXT-6858
		sCapslockDidNotification = TRUE;
	}
}


void LLPanelLogin::updateServer()
{
	try 
	{

		updateServerCombo();	
		// if they've selected another grid, we should load the credentials
		// for that grid and set them to the UI.
		if(sInstance && !sInstance->areCredentialFieldsDirty())
		{
			LLPointer<LLCredential> credential = gSecAPIHandler->loadCredential(LLGridManager::getInstance()->getGrid());	
			bool remember = sInstance->getChild<LLUICtrl>("remember_check")->getValue();
			sInstance->setFields(credential, remember);
		}
		// grid changed so show new splash screen (possibly)
		loadLoginPage();
		updateLocationCombo(LLStartUp::getStartSLURL().getType() == LLSLURL::LOCATION);
	}
	catch (LLInvalidGridName ex)
	{
		// do nothing
	}
}

void LLPanelLogin::updateServerCombo()
{
	if (!sInstance) 
	{
		return;	
	}
	// We add all of the possible values, sorted, and then add a bar and the current value at the top
	LLComboBox* server_choice_combo = sInstance->getChild<LLComboBox>("server_combo");	
	server_choice_combo->removeall();

	std::map<std::string, std::string> known_grids = LLGridManager::getInstance()->getKnownGrids(!gSavedSettings.getBOOL("ShowBetaGrids"));

	for (std::map<std::string, std::string>::iterator grid_choice = known_grids.begin();
		 grid_choice != known_grids.end();
		 grid_choice++)
	{
		if (!grid_choice->first.empty())
		{
			server_choice_combo->add(grid_choice->second, grid_choice->first);
		}
	}
	server_choice_combo->sortByName();
	
	server_choice_combo->addSeparator(ADD_TOP);
	
	server_choice_combo->add(LLGridManager::getInstance()->getGridLabel(), 
		LLGridManager::getInstance()->getGrid(), ADD_TOP);	
	
	server_choice_combo->selectFirstItem();	
}

// static
void LLPanelLogin::onSelectServer(LLUICtrl*, void*)
{
	// *NOTE: The paramters for this method are ignored. 
	// LLPanelLogin::onServerComboLostFocus(LLFocusableElement* fe, void*)
	// calls this method.
	LL_INFOS("AppInit") << "onSelectServer" << LL_ENDL;
	// The user twiddled with the grid choice ui.
	// apply the selection to the grid setting.
	LLPointer<LLCredential> credential;
	
	LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
	LLSD combo_val = combo->getSelectedValue();
	if (combo_val.isUndefined())
	{
		combo_val = combo->getValue();
	}
	
	combo = sInstance->getChild<LLComboBox>("start_location_combo");	
	combo->setCurrentByIndex(1);
	LLStartUp::setStartSLURL(LLSLURL(gSavedSettings.getString("LoginLocation")));
	LLGridManager::getInstance()->setGridChoice(combo_val.asString());
	// This new selection will override preset uris
	// from the command line.
	updateServer();
	updateLocationCombo(false);
	updateLoginPanelLinks();
}

void LLPanelLogin::onServerComboLostFocus(LLFocusableElement* fe)
{
	if (!sInstance)
	{
		return;
	}

	LLComboBox* combo = sInstance->getChild<LLComboBox>("server_combo");
	if(fe == combo)
	{
		onSelectServer(combo, NULL);	
	}
}

void LLPanelLogin::updateLoginPanelLinks()
{
	LLSD grid_data;
	LLGridManager::getInstance()->getGridInfo(grid_data);
	bool system_grid = grid_data.has(GRID_IS_SYSTEM_GRID_VALUE);
	
	// need to call through sInstance, as it's called from onSelectServer, which
	// is static.
	sInstance->getChildView("create_new_account_text")->setVisible( system_grid);
	sInstance->getChildView("forgot_password_text")->setVisible( system_grid);
}

void LLPanelLogin::onModeChange(const LLSD& original_value, const LLSD& new_value)
{
	if (original_value.asString() != new_value.asString())
	{
		LLNotificationsUtil::add("ModeChange", LLSD(), LLSD(), boost::bind(&LLPanelLogin::onModeChangeConfirm, this, original_value, new_value, _1, _2));
	}
}

void LLPanelLogin::onModeChangeConfirm(const LLSD& original_value, const LLSD& new_value, const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch (option)
	{
	case 0:
		gSavedSettings.getControl("SessionSettingsFile")->set(new_value);
		LLAppViewer::instance()->forceQuit();
		break;
	case 1:
		// revert to original value
		getChild<LLUICtrl>("mode_combo")->setValue(original_value);
		break;
	default:
		break;
	}
}

std::string canonicalize_username(const std::string& name)
{
	std::string cname = name;
	LLStringUtil::trim(cname);

	// determine if the username is a first/last form or not.
	size_t separator_index = cname.find_first_of(" ._");
	std::string first = cname.substr(0, separator_index);
	std::string last;
	if (separator_index != cname.npos)
	{
		last = cname.substr(separator_index+1, cname.npos);
		LLStringUtil::trim(last);
	}
	else
	{
		// ...on Linden grids, single username users as considered to have
		// last name "Resident"
		last = "Resident";
	}

	// Username in traditional "firstname lastname" form.
	return first + ' ' + last;
}
