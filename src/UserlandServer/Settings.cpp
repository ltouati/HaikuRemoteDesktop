/*
 * Settings.cpp
 * Persistent Settings Manager
 */
#include "Settings.h"
#include <File.h>
#include <FindDirectory.h>
#include <Path.h>
#include <Directory.h>
#include <stdio.h>

#define SETTINGS_DIR_NAME "HaikuRemoteDesktop"
#define SETTINGS_FILENAME "settings"

#include <stdlib.h>

Settings::Settings()
    : fPort(8443) {
    
    // Set default paths to be inside the settings directory
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {
        path.Append(SETTINGS_DIR_NAME);
        
        BPath certPath = path;
        certPath.Append("server.crt");
        fSSLCertPath = certPath.Path();
        
        BPath keyPath = path;
        keyPath.Append("server.key");
        fSSLKeyPath = keyPath.Path();
    } else {
        fSSLCertPath = "server.crt";
        fSSLKeyPath = "server.key";
    }
}
// ... (Desctructor/Load/Save remain same, appending new methods at end)

status_t Settings::GenerateCertificates() {
    BString command;
    // Ensure directory exists for the paths
    BPath certPath(fSSLCertPath);
    BPath parent;
    if (certPath.GetParent(&parent) == B_OK) {
        create_directory(parent.Path(), 0755);
    }
    
    command.SetToFormat("openssl req -x509 -newkey rsa:4096 -keyout \"%s\" -out \"%s\" -days 365 -nodes -subj \"/C=US/ST=State/L=City/O=HaikuRemote/CN=localhost\"", fSSLKeyPath.String(), fSSLCertPath.String());
    
    int ret = system(command.String());
    if (ret != 0) {
        fprintf(stderr, "Failed to generate SSL certificates. Code: %d\n", ret);
        return B_ERROR;
    }
    return B_OK;
}

BString Settings::ReadContent(const char* filePath) {
    BFile file(filePath, B_READ_ONLY);
    if (file.InitCheck() != B_OK) return "";
    
    off_t size;
    file.GetSize(&size);
    
    BString content;
    char* buffer = content.LockBuffer(size);
    file.Read(buffer, size);
    content.UnlockBuffer(size);
    
    return content;
}

status_t Settings::WriteContent(const char* filePath, const char* content) {
    BFile file(filePath, B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK) return B_ERROR;
    
    ssize_t len = strlen(content);
    if (file.Write(content, len) != len) return B_ERROR;
    
    return B_OK;
}

Settings::~Settings() {
}

status_t Settings::Load() {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK) {
        return B_ERROR;
    }
    path.Append(SETTINGS_DIR_NAME);
    
    // Create directory if it doesn't exist
    create_directory(path.Path(), 0755);
    
    path.Append(SETTINGS_FILENAME);

    BFile file(path.Path(), B_READ_ONLY);
    if (file.InitCheck() != B_OK) {
        // No settings file yet, use defaults
        return B_OK;
    }

    BMessage msg;
    if (msg.Unflatten(&file) != B_OK) {
        return B_ERROR;
    }

    if (msg.FindUInt16("port", &fPort) != B_OK) fPort = 8443;
    
    const char* str;
    if (msg.FindString("cert_path", &str) == B_OK) fSSLCertPath = str;
    // Keep default if not found (which is already set in constructor)

    if (msg.FindString("key_path", &str) == B_OK) fSSLKeyPath = str;
    // Keep default if not found
    
    return B_OK;
}

status_t Settings::Save() {
    BPath path;
    if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK) {
        return B_ERROR;
    }
    path.Append(SETTINGS_DIR_NAME);
    
    // Ensure directory exists
    create_directory(path.Path(), 0755);

    path.Append(SETTINGS_FILENAME);

    BFile file(path.Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (file.InitCheck() != B_OK) {
        return B_ERROR;
    }

    BMessage msg;
    msg.AddUInt16("port", fPort);
    msg.AddString("cert_path", fSSLCertPath);
    msg.AddString("key_path", fSSLKeyPath);

    return msg.Flatten(&file);
}
