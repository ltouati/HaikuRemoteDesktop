/*
 * InputDriverManager.h
 * Manages installation and removal of the VirtualMouse driver
 */
#ifndef INPUT_DRIVER_MANAGER_H
#define INPUT_DRIVER_MANAGER_H

#include <SupportDefs.h>

class InputDriverManager {
public:
	InputDriverManager();
	~InputDriverManager();

	// Installs the driver (.so) and restarts input_server
	status_t InstallDriver();

	// Removes the driver (.so) and restarts input_server
	status_t UninstallDriver();

private:
	status_t _RestartInputServer();
};

#endif // INPUT_DRIVER_MANAGER_H
