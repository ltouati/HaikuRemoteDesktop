/*
 * Settings.h
 * Persistent Settings Manager
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <SupportDefs.h>
#include <String.h>
#include <Message.h>

class Settings {
public:
    Settings();
    ~Settings();

    status_t Load();
    status_t Save();

    uint16 Port() const { return fPort; }
    void SetPort(uint16 port) { fPort = port; }

    const char* SSLCertPath() const { return fSSLCertPath.String(); }
    void SetSSLCertPath(const char* path) { fSSLCertPath = path; }

    const char* SSLKeyPath() const { return fSSLKeyPath.String(); }
    void SetSSLKeyPath(const char* path) { fSSLKeyPath = path; }
    
    // New Methods for UI
    status_t GenerateCertificates();
    BString ReadContent(const char* path);
    status_t WriteContent(const char* path, const char* content);

private:
    uint16 fPort;
    BString fSSLCertPath;
    BString fSSLKeyPath;
};

#endif // SETTINGS_H
