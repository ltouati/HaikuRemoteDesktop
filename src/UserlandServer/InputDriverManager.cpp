/*
 * InputDriverManager.cpp
 */
#include "InputDriverManager.h"
#include <Entry.h>
#include <Path.h>
#include <File.h>
#include <Directory.h>
#include <FindDirectory.h>
#include <Roster.h>
#include <Application.h>
#include <stdio.h>
#include <stdlib.h> // for system()

#define DRIVER_NAME "VirtualMouse"
#define DRIVER_FILENAME "virtual_input_server_addon.so"


InputDriverManager::InputDriverManager()
{
}

InputDriverManager::~InputDriverManager()
{
}

status_t 
InputDriverManager::InstallDriver() 
{
	// 1. Find Source Driver (adjacent to executable)
	app_info info;
	be_app->GetAppInfo(&info);
	BPath srcPath(&info.ref);
	srcPath.GetParent(&srcPath);
	srcPath.Append(DRIVER_FILENAME);
	
	BEntry srcEntry(srcPath.Path());
	if (!srcEntry.Exists()) {
		fprintf(stderr, "Driver file not found at: %s\n", srcPath.Path());
		return B_ENTRY_NOT_FOUND;
	}

	// 2. Find Destination Directory
	BPath destPath;
	if (find_directory(B_USER_NONPACKAGED_ADDONS_DIRECTORY, &destPath) != B_OK)
		return B_ERROR;
		
	destPath.Append("input_server/devices");
	create_directory(destPath.Path(), 0755); // Ensure exists
	
	destPath.Append(DRIVER_NAME);
	
	printf("Installing driver to: %s\n", destPath.Path());

	// 3. Copy File
	// Simple copy using BFile
	BFile srcFile(&srcEntry, B_READ_ONLY);
	if (srcFile.InitCheck() != B_OK) return srcFile.InitCheck();
	
	BFile destFile(destPath.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
	if (destFile.InitCheck() != B_OK) return destFile.InitCheck();
	
	off_t size;
	srcFile.GetSize(&size);
	void* buffer = malloc(size);
	srcFile.Read(buffer, size);
	destFile.Write(buffer, size);
	free(buffer);
	
	// 4. Restart Input Server
	return _RestartInputServer();
}

status_t 
InputDriverManager::UninstallDriver() 
{
	BPath destPath;
	if (find_directory(B_USER_NONPACKAGED_ADDONS_DIRECTORY, &destPath) != B_OK)
		return B_ERROR;
		
	destPath.Append("input_server/devices");
	destPath.Append(DRIVER_NAME);
	
	BEntry destEntry(destPath.Path());
	if (destEntry.Exists()) {
		printf("Removing driver: %s\n", destPath.Path());
		destEntry.Remove();
	} else {
		printf("Driver not found, skipping removal.\n");
		return B_OK;
	}
	
	return _RestartInputServer();
}

status_t 
InputDriverManager::_RestartInputServer() 
{
	printf("Restarting Input Server...\n");
	// Using system() is simplest way to invoke the server with flags
	// "/system/servers/input_server -q" quits it.
	// The system (Launch Daemon) should restart it automatically?
	// Or we might need to launch it.
	
	int ret = system("/system/servers/input_server -q");
	
	// Wait a moment
	snooze(1000000); // 1s
	
	if (!be_roster->IsRunning("application/x-vnd.Be-input_server")) {
		printf("Input Server not running, launching...\n");
		be_roster->Launch("application/x-vnd.Be-input_server");
	}
	
	return (ret == 0) ? B_OK : B_ERROR;
}
