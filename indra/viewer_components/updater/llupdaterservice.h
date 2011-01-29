/** 
 * @file llupdaterservice.h
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

#ifndef LL_UPDATERSERVICE_H
#define LL_UPDATERSERVICE_H

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

class LLUpdaterServiceImpl;

class LLUpdaterService
{
public:
	class UsageError: public std::runtime_error
	{
	public:
		UsageError(const std::string& msg) : std::runtime_error(msg) {}
	};
	
	// Name of the event pump through which update events will be delivered.
	static std::string const & pumpName(void);
	
	// Returns true if an update has been completely downloaded and is now ready to install.
	static bool updateReadyToInstall(void);
	
	// Type codes for events posted by this service.  Stored the event's 'type' element.
	enum eUpdaterEvent {
		INVALID,
		DOWNLOAD_COMPLETE,
		DOWNLOAD_ERROR,
		INSTALL_ERROR,
		PROGRESS,
		STATE_CHANGE
	};
	
	enum eUpdaterState {
		INITIAL,
		CHECKING_FOR_UPDATE,
		TEMPORARY_ERROR,
		DOWNLOADING,
		INSTALLING,
		UP_TO_DATE,
		TERMINAL,
		FAILURE
	};

	LLUpdaterService();
	~LLUpdaterService();

	void initialize(const std::string& protocol_version,
				    const std::string& url, 
				    const std::string& path,
				    const std::string& channel,
				    const std::string& version);

	void setCheckPeriod(unsigned int seconds);
	void setBandwidthLimit(U64 bytesPerSecond);
	
	void startChecking(bool install_if_ready = false);
	void stopChecking();
	bool isChecking();
	eUpdaterState getState();

	typedef boost::function<void (void)> app_exit_callback_t;
	template <typename F>
	void setAppExitCallback(F const &callable) 
	{ 
		app_exit_callback_t aecb = callable;
		setImplAppExitCallback(aecb);
	}
	
	// If an update is or has been downloaded, this method will return the
	// version string for that update.  An empty string will be returned
	// otherwise.
	std::string updatedVersion(void);

private:
	boost::shared_ptr<LLUpdaterServiceImpl> mImpl;
	void setImplAppExitCallback(app_exit_callback_t aecb);
};

// Returns the full version as a string.
std::string const & ll_get_version(void);

#endif // LL_UPDATERSERVICE_H
