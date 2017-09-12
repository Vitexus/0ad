/* Copyright (C) 2017 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "JSInterface_Debug.h"

#include "i18n/L10n.h"
#include "lib/svn_revision.h"
#include "ps/CConsole.h"
#include "ps/CLogger.h"
#include "ps/Profile.h"
#include "ps/ProfileViewer.h"
#include "ps/UserReport.h"

/**
 * Microseconds since the epoch.
 */
double JSI_Debug::GetMicroseconds(ScriptInterface::CxPrivate* UNUSED(pCxPrivate))
{
	return JS_Now();
}

// Deliberately cause the game to crash.
// Currently implemented via access violation (read of address 0).
// Useful for testing the crashlog/stack trace code.
int JSI_Debug::Crash(ScriptInterface::CxPrivate* UNUSED(pCxPrivate))
{
	debug_printf("Crashing at user's request.\n");
	return *(volatile int*)0;
}

void JSI_Debug::DebugWarn(ScriptInterface::CxPrivate* UNUSED(pCxPrivate))
{
	debug_warn(L"Warning at user's request.");
}

void JSI_Debug::DisplayErrorDialog(ScriptInterface::CxPrivate* UNUSED(pCxPrivate), const std::wstring& msg)
{
	debug_DisplayError(msg.c_str(), DE_NO_DEBUG_INFO, NULL, NULL, NULL, 0, NULL, NULL);
}

JS::Value JSI_Debug::GetProfilerState(ScriptInterface::CxPrivate* pCxPrivate)
{
	return g_ProfileViewer.SaveToJS(*(pCxPrivate->pScriptInterface));
}

// Return the date/time at which the current executable was compiled.
// params: mode OR an integer specifying
//   what to display: -1 for "date time (svn revision)", 0 for date, 1 for time, 2 for svn revision
// returns: string with the requested timestamp info
// notes:
// - Displayed on main menu screen; tells non-programmers which auto-build
//   they are running. Could also be determined via .EXE file properties,
//   but that's a bit more trouble.
// - To be exact, the date/time returned is when scriptglue.cpp was
//   last compiled, but the auto-build does full rebuilds.
// - svn revision is generated by calling svnversion and cached in
//   lib/svn_revision.cpp. it is useful to know when attempting to
//   reproduce bugs (the main EXE and PDB should be temporarily reverted to
//   that revision so that they match user-submitted crashdumps).
std::wstring JSI_Debug::GetBuildTimestamp(ScriptInterface::CxPrivate* UNUSED(pCxPrivate), int mode)
{
	char buf[200];
	if (mode == -1) // Date, time and revision.
	{
		UDate dateTime = g_L10n.ParseDateTime(__DATE__ " " __TIME__, "MMM d yyyy HH:mm:ss", Locale::getUS());
		std::string dateTimeString = g_L10n.LocalizeDateTime(dateTime, L10n::DateTime, SimpleDateFormat::DATE_TIME);
		char svnRevision[32];
		sprintf_s(svnRevision, ARRAY_SIZE(svnRevision), "%ls", svn_revision);
		if (strcmp(svnRevision, "custom build") == 0)
		{
			// Translation: First item is a date and time, item between parenthesis is the Subversion revision number of the current build.
			sprintf_s(buf, ARRAY_SIZE(buf), g_L10n.Translate("%s (custom build)").c_str(), dateTimeString.c_str());
		}
		else
		{
			// Translation: First item is a date and time, item between parenthesis is the Subversion revision number of the current build.
			// dennis-ignore: *
			sprintf_s(buf, ARRAY_SIZE(buf), g_L10n.Translate("%s (%ls)").c_str(), dateTimeString.c_str(), svn_revision);
		}
	}
	else if (mode == 0) // Date.
	{
		UDate dateTime = g_L10n.ParseDateTime(__DATE__, "MMM d yyyy", Locale::getUS());
		std::string dateTimeString = g_L10n.LocalizeDateTime(dateTime, L10n::Date, SimpleDateFormat::MEDIUM);
		sprintf_s(buf, ARRAY_SIZE(buf), "%s", dateTimeString.c_str());
	}
	else if (mode == 1) // Time.
	{
		UDate dateTime = g_L10n.ParseDateTime(__TIME__, "HH:mm:ss", Locale::getUS());
		std::string dateTimeString = g_L10n.LocalizeDateTime(dateTime, L10n::Time, SimpleDateFormat::MEDIUM);
		sprintf_s(buf, ARRAY_SIZE(buf), "%s", dateTimeString.c_str());
	}
	else if (mode == 2) // Revision.
	{
		char svnRevision[32];
		sprintf_s(svnRevision, ARRAY_SIZE(svnRevision), "%ls", svn_revision);
		if (strcmp(svnRevision, "custom build") == 0)
		{
			sprintf_s(buf, ARRAY_SIZE(buf), "%s", g_L10n.Translate("custom build").c_str());
		}
		else
			sprintf_s(buf, ARRAY_SIZE(buf), "%ls", svn_revision);
	}

	return wstring_from_utf8(buf);
}

// Force a JS garbage collection cycle to take place immediately.
// Writes an indication of how long this took to the console.
void JSI_Debug::ForceGC(ScriptInterface::CxPrivate* pCxPrivate)
{
	double time = timer_Time();
	JS_GC(pCxPrivate->pScriptInterface->GetJSRuntime());
	time = timer_Time() - time;
	g_Console->InsertMessage(fmt::sprintf("Garbage collection completed in: %f", time));
}

bool JSI_Debug::IsUserReportEnabled(ScriptInterface::CxPrivate* UNUSED(pCxPrivate))
{
	return g_UserReporter.IsReportingEnabled();
}

void JSI_Debug::SetUserReportEnabled(ScriptInterface::CxPrivate* UNUSED(pCxPrivate), bool enabled)
{
	g_UserReporter.SetReportingEnabled(enabled);
}

std::string JSI_Debug::GetUserReportStatus(ScriptInterface::CxPrivate* UNUSED(pCxPrivate))
{
	return g_UserReporter.GetStatus();
}

void JSI_Debug::SubmitUserReport(ScriptInterface::CxPrivate* UNUSED(pCxPrivate), const std::string& type, int version, const std::wstring& data)
{
	g_UserReporter.SubmitReport(type.c_str(), version, utf8_from_wstring(data));
}

void JSI_Debug::RegisterScriptFunctions(const ScriptInterface& scriptInterface)
{
	scriptInterface.RegisterFunction<double, &GetMicroseconds>("GetMicroseconds");
	scriptInterface.RegisterFunction<int, &Crash>("Crash");
	scriptInterface.RegisterFunction<void, &DebugWarn>("DebugWarn");
	scriptInterface.RegisterFunction<void, std::wstring, &DisplayErrorDialog>("DisplayErrorDialog");
	scriptInterface.RegisterFunction<JS::Value, &GetProfilerState>("GetProfilerState");
	scriptInterface.RegisterFunction<std::wstring, int, &GetBuildTimestamp>("GetBuildTimestamp");
	scriptInterface.RegisterFunction<void, &ForceGC>("ForceGC");

	// User report functions
	scriptInterface.RegisterFunction<bool, &IsUserReportEnabled>("IsUserReportEnabled");
	scriptInterface.RegisterFunction<void, bool, &SetUserReportEnabled>("SetUserReportEnabled");
	scriptInterface.RegisterFunction<std::string, &GetUserReportStatus>("GetUserReportStatus");
	scriptInterface.RegisterFunction<void, std::string, int, std::wstring, &SubmitUserReport>("SubmitUserReport");
}
