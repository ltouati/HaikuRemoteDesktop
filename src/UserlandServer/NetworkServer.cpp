/*
 * NetworkServer.cpp
 * Custom Socket Server
 */
#include "NetworkServer.h"
#include "NetworkUtils.h"
#include "messages.pb.h"
#include "handlers/PacketHandlerFactory.h"
#include "ScreenCapture.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <Path.h>
#include <File.h>
#include <Application.h>
#include <Roster.h>
#include <Entry.h>
#include <Clipboard.h>
#define BUFFER_SIZE 4096

NetworkServer::NetworkServer(port_id inputPort)
    : fServerSocket(-1),
      fInputPort(inputPort),
      fRunning(false),
      fTarget(BMessenger()),
      fWebSocketClientCount(0),
      fCurrentBitrate(2000),
      fLock("NetworkLock"),
      fSSLContext(nullptr),
      fLastX(0),
      fLastY(0),
      fLastCursorTime(0),
      fScreenCapture(nullptr) {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    fSSLContext = SSL_CTX_new(TLS_server_method());
    if (!fSSLContext) {
        fprintf(stderr, "Unable to create SSL context\n");
        ERR_print_errors_fp(stderr);
    } else {
        // Auto-generate Self-Signed Cert if missing
        BEntry certEntry("server.crt");
        BEntry keyEntry("server.key");
        if (!certEntry.Exists() || !keyEntry.Exists()) {
            printf("SSL Certificates not found. Generating self-signed certificate...\n");
            int ret = system(
                "openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes -subj \"/C=US/ST=State/L=City/O=HaikuRemote/CN=localhost\"");
            if (ret != 0) {
                fprintf(stderr, "Failed to generate SSL certificates (openssl command failed)\n");
            }
        }

        // Load Certs
        // Assume server.crt and server.key are in the CWD
        if (SSL_CTX_use_certificate_file(fSSLContext, "server.crt", SSL_FILETYPE_PEM) <= 0) {
            fprintf(stderr, "Failed to load server.crt\n");
            ERR_print_errors_fp(stderr);
        }
        if (SSL_CTX_use_PrivateKey_file(fSSLContext, "server.key", SSL_FILETYPE_PEM) <= 0) {
            fprintf(stderr, "Failed to load server.key\n");
            ERR_print_errors_fp(stderr);
        }
    }
}

NetworkServer::~NetworkServer() {
    Stop();
    if (fSSLContext) SSL_CTX_free(fSSLContext);
    EVP_cleanup();
}

void
NetworkServer::SetTarget(BMessenger target) {
    fTarget = target;
}

void
NetworkServer::SetScreenCapture(ScreenCapture *screenCapture) {
    fScreenCapture = screenCapture;
}

void
NetworkServer::SetCursor(float x, float y) {
    fLastX = x;
    fLastY = y;
    fLastCursorTime = system_time();
}

bool
NetworkServer::GetLastCursor(float &x, float &y, bigtime_t &time) {
    if (fLastCursorTime == 0) return false;
    x = fLastX;
    y = fLastY;
    time = fLastCursorTime;
    return true;
}

status_t
NetworkServer::Start(uint16 port) {
    fServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (fServerSocket < 0) return B_ERROR;

    int opt = 1;
    setsockopt(fServerSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Non-blocking
    int flags = fcntl(fServerSocket, F_GETFL, 0);
    fcntl(fServerSocket, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fServerSocket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(fServerSocket);
        return B_ERROR;
    }

    if (listen(fServerSocket, 5) < 0) {
        close(fServerSocket);
        return B_ERROR;
    }

    fRunning = true;
    return B_OK;
}

void
NetworkServer::Stop() {
    fLock.Lock();
    fRunning = false;
    if (fServerSocket >= 0) {
        close(fServerSocket);
        fServerSocket = -1;
    }

    for (int32 i = 0; i < fClients.CountItems(); i++) {
        ClientState *client = (ClientState *) fClients.ItemAt(i);
        if (client->ssl) SSL_free(client->ssl);
        close(client->socket);
        delete client;
    }
    fClients.MakeEmpty();
    fWebSocketClientCount = 0;
    fLock.Unlock();
}

void
NetworkServer::Broadcast(const struct iovec *vec, int count) {
    if (fClients.CountItems() == 0) return;

    fLock.Lock();
    for (int32 i = 0; i < fClients.CountItems(); i++) {
        ClientState *client = (ClientState *) fClients.ItemAt(i);
        if (client->isWebSocket && client->sslAccepted) {
            // We need to flatten the iovec because SSL_write doesn't support scatter/gather directly
            // (Unless we use BIOs, but that's complex. Flat buffer copy is easier for now)
            // Calculate Total Size
            size_t totalLen = 0;
            for (int k = 0; k < count; k++) totalLen += vec[k].iov_len;

            uint8 *flatBuf = new uint8[totalLen];
            size_t offset = 0;
            for (int k = 0; k < count; k++) {
                memcpy(flatBuf + offset, vec[k].iov_base, vec[k].iov_len);
                offset += vec[k].iov_len;
            }

            size_t sentTotal = 0;
            while (sentTotal < totalLen) {
                int written = SSL_write(client->ssl, flatBuf + sentTotal, totalLen - sentTotal);
                if (written <= 0) {
                    int err = SSL_get_error(client->ssl, written);
                    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                        // Blocking here is bad, but for simplicity we snooze
                        snooze(100);
                        continue;
                    }
                    // Error
                    break;
                }
                sentTotal += written;
            }

            delete[] flatBuf;
        }
    }
    fLock.Unlock();
}

void
NetworkServer::Broadcast(const void *header, size_t headerLen, const void *data, size_t dataLen) {
    struct iovec vec[2];
    vec[0].iov_base = (void *) header;
    vec[0].iov_len = headerLen;
    vec[1].iov_base = (void *) data;
    vec[1].iov_len = dataLen;
    Broadcast(vec, 2);
}

void
NetworkServer::ProcessEvents() {
    if (!fRunning) return;

    fLock.Lock(); // Protect fClients

    _CheckClipboard();

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(fServerSocket, &readSet);

    int maxFd = fServerSocket;

    for (int32 i = 0; i < fClients.CountItems(); i++) {
        ClientState *client = (ClientState *) fClients.ItemAt(i);
        FD_SET(client->socket, &readSet);
        if (client->socket > maxFd) maxFd = client->socket;
    }

    fLock.Unlock(); // Unlock for Select (Blocking-ish)

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000; // 10ms timeout

    if (select(maxFd + 1, &readSet, nullptr, nullptr, &tv) > 0) {
        if (FD_ISSET(fServerSocket, &readSet)) {
            _HandleNewConnection(); // Should Lock internally or here? _HandleNewConnection modifies fClients.
        }

        fLock.Lock(); // Lock again to iterate/modify clients
        for (int32 i = fClients.CountItems() - 1; i >= 0; i--) {
            ClientState *client = (ClientState *) fClients.ItemAt(i);

            // Handle SSL Handshake
            if (!client->sslAccepted) {
                int ret = SSL_accept(client->ssl);
                if (ret <= 0) {
                    int err = SSL_get_error(client->ssl, ret);
                    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                        // Need more data/write, keep socket open
                    } else {
                        printf("SSL Handshake failed: %d\n", err);
                        ERR_print_errors_fp(stdout);
                        SSL_free(client->ssl);
                        close(client->socket);
                        fClients.RemoveItem(i);
                        delete client;
                    }
                } else {
                    printf("SSL Handshake Success: %d\n", client->socket);
                    client->sslAccepted = true;
                }
                continue; // Retry next loop
            }

            if (FD_ISSET(client->socket, &readSet)) {
                // Reading from client (SSL)
                char buffer[BUFFER_SIZE];
                int bytesRead = SSL_read(client->ssl, buffer, BUFFER_SIZE - 1);

                if (bytesRead <= 0) {
                    int err = SSL_get_error(client->ssl, bytesRead);
                    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                        // Needs more, continue
                        continue;
                    }

                    // Disconnect
                    SSL_free(client->ssl);
                    close(client->socket);

                    if (client->isWebSocket) {
                        fWebSocketClientCount--;
                        if (fWebSocketClientCount == 0 && fTarget.IsValid()) {
                            fTarget.SendMessage(MSG_NO_CLIENTS);
                        }
                    }

                    fClients.RemoveItem(i);
                    delete client;
                } else {
                    // Success
                    buffer[bytesRead] = 0;
                    // client->buffer.Append(buffer, bytesRead);
                    client->buffer.insert(client->buffer.end(), (uint8 *) buffer, (uint8 *) buffer + bytesRead);

                    bool closeConn = false;
                    if (!client->isWebSocket) {
                        // Wait for full HTTP header (\r\n\r\n = 0x0D 0x0A 0x0D 0x0A)
                        bool headerFound = false;
                        if (client->buffer.size() >= 4) {
                            for (size_t k = 0; k <= client->buffer.size() - 4; k++) {
                                if (client->buffer[k] == 0x0D && client->buffer[k + 1] == 0x0A &&
                                    client->buffer[k + 2] == 0x0D && client->buffer[k + 3] == 0x0A) {
                                    headerFound = true;
                                    break;
                                }
                            }
                        }

                        if (headerFound) {
                            closeConn = _ParseHTTP(client, (const char *) client->buffer.data(), client->buffer.size());

                            if (!closeConn) {
                                client->buffer.clear();
                            }
                        }
                    } else {
                        // WS Data Parse
                        while (true) {
                            size_t consumed = _ParseWebSocketFrame(client);
                            if (consumed == 0) break; // Need more data
                            if (consumed <= client->buffer.size()) {
                                client->buffer.erase(client->buffer.begin(), client->buffer.begin() + consumed);
                            }
                            if (client->buffer.empty()) break;
                        }
                    }

                    if (closeConn) {
                        SSL_shutdown(client->ssl);
                        SSL_free(client->ssl);
                        close(client->socket);
                        fClients.RemoveItem(i);
                        delete client;
                    }
                }
            }
        }
        fLock.Unlock();
    }
}

void
NetworkServer::_HandleNewConnection() {
    sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    int clientSocket = accept(fServerSocket, (struct sockaddr *) &clientAddr, &addrLen);

    if (clientSocket >= 0) {
        // Set non-blocking before SSL
        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

        fLock.Lock();
        ClientState *client = new ClientState();
        client->socket = clientSocket;
        client->isWebSocket = false;

        // Create SSL
        client->ssl = SSL_new(fSSLContext);
        SSL_set_fd(client->ssl, clientSocket);
        client->sslAccepted = false; // Waiting for handshake

        fClients.AddItem(client);
        fLock.Unlock();

        printf("New connection: %d (SSL Pending)\n", clientSocket);
    }
}

bool
NetworkServer::_ParseHTTP(ClientState *client, const char *data, ssize_t len) {
    int clientSocket = client->socket;
    // ... logic uses client->ssl for writes ... (Need to refactor inside)
    BString request(data);
    printf("HTTP Request (Length %ld):\n%s\n", len, request.String());

    // Split into lines
    int32 pos = 0;
    BString line;
    int32 lineEnd = request.FindFirst("\n", pos);

    BString path;
    bool isUpgrade = false;
    BString wsKey = "";

    int lineCount = 0;
    while (lineEnd != B_ERROR) {
        line.SetTo(request.String() + pos, lineEnd - pos);
        line.RemoveAll("\r"); // Remove CR

        if (lineCount == 0) {
            // Request Line: GET /path HTTP/1.1
            if (line.FindFirst("GET") == 0) {
                int32 firstSpace = line.FindFirst(" ");
                int32 secondSpace = line.FindFirst(" ", firstSpace + 1);
                if (firstSpace != B_ERROR && secondSpace != B_ERROR) {
                    line.CopyInto(path, firstSpace + 1, secondSpace - firstSpace - 1);
                }
            }
        } else {
            // Headers
            BString lowerLine = line;
            lowerLine.ToLower();

            if (lowerLine.FindFirst("upgrade:") == 0) {
                if (lowerLine.FindFirst("websocket") != B_ERROR) {
                    isUpgrade = true;
                }
            } else if (lowerLine.FindFirst("sec-websocket-key:") == 0) {
                // Extract Value (Case Sensitive from original line)
                int32 colonPos = line.FindFirst(":");
                if (colonPos != B_ERROR) {
                    wsKey.SetTo(line.String() + colonPos + 1);
                    wsKey.Trim(); // Remove surrounding spaces
                }
            }
        }

        pos = lineEnd + 1;
        lineEnd = request.FindFirst("\n", pos);
        lineCount++;
    }

    printf("Parsed Path: '%s', Upgrade: %d, Key: '%s'\n", path.String(), isUpgrade, wsKey.String());

    if (isUpgrade && wsKey.Length() > 0) {
        printf("Performing WebSocket Handshake...\n");
        BString response = _MakeWebSocketResponse(wsKey.String());

        // SSL Write
        SSL_write(client->ssl, response.String(), response.Length());

        // Mark as upgraded
        client->isWebSocket = true;
        fWebSocketClientCount++;

        // Send Welcome Message (Init Config) immediately
        if (fWelcomeMessage.Length() > 0) {
            // Need to wrap in WS Frame?
            // The fWelcomeMessage stored by SetWelcomeMessage is likely just the JSON string.
            // Broadcast wraps it. We need to wrap it here too.

            uint8 headerBuf[16];
            size_t headerLen = NetworkUtils::MakeWebSocketHeader(fWelcomeMessage.Length(), headerBuf, 0x01); // Text

            SSL_write(client->ssl, headerBuf, headerLen);
            SSL_write(client->ssl, fWelcomeMessage.String(), fWelcomeMessage.Length());
        }

        if (fTarget.IsValid()) {
            fTarget.SendMessage(MSG_CLIENTS_CONNECTED);
        }
        return false; // Keep open
    } else {
        // Static File Serving

        // 1. Sanitize Path
        // Remove Query Params
        int32 qPos = path.FindFirst("?");
        if (qPos != B_ERROR) path.Truncate(qPos);

        // Security: Prevent Directory Traversal
        if (path.FindFirst("..") != B_ERROR) {
            const char *msg = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
            SSL_write(client->ssl, msg, strlen(msg));
            return true;
        }

        // ... (Path Check) ...

        if (path.Length() > 1 && path[1] == '/') {
            const char *msg = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
            SSL_write(client->ssl, msg, strlen(msg));
            return true;
        }

        // Default to index.html
        if (path == "/" || path.Length() == 0) path = "/index.html";

        // Resolve Path relative to executable
        app_info info;
        be_app->GetAppInfo(&info);
        BPath filePath(&info.ref);
        filePath.GetParent(&filePath);
        filePath.Append(path.String() + 1); // Skip leading '/'

        BFile file(filePath.Path(), B_READ_ONLY);
        if (file.InitCheck() == B_OK) {
            off_t size;
            file.GetSize(&size);

            // Determine MIME Type
            const char *mime = "application/octet-stream";
            if (path.EndsWith(".html")) mime = "text/html";
            else if (path.EndsWith(".js")) mime = "application/javascript";
            else if (path.EndsWith(".css")) mime = "text/css";
            else if (path.EndsWith(".wasm")) mime = "application/wasm";

            printf("Serving '%s' (%lld bytes, %s)\n", filePath.Path(), size, mime);

            BString header;
            header << "HTTP/1.1 200 OK\r\n"
                    << "Content-Type: " << mime << "\r\n"
                    << "Cache-Control: no-cache\r\n"
                    << "Content-Length: " << size << "\r\n\r\n";

            SSL_write(client->ssl, header.String(), header.Length());

            // Send File Content
            char *buf = new char[4096];
            while (size > 0) {
                ssize_t read = file.Read(buf, 4096);
                if (read <= 0) break;

                size_t sentTotal = 0;
                while (sentTotal < (size_t) read) {
                    int sent = SSL_write(client->ssl, buf + sentTotal, read - sentTotal);
                    if (sent <= 0) {
                        int err = SSL_get_error(client->ssl, sent);
                        // Blocking handling bad, but skip for simplicity
                        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                            snooze(100);
                            continue;
                        }
                        break;
                    }
                    sentTotal += sent;
                }
                size -= read;
            }
            delete[] buf;
            return true; // Close after response (HTTP/1.0 style)
        } else {
            printf("File not found: %s\n", filePath.Path());
            const char *msg = "HTTP/1.1 404 Not Found\r\n\r\n404 Not Found";
            SSL_write(client->ssl, msg, strlen(msg));
            return true;
        }
    }
}

BString
NetworkServer::_MakeWebSocketResponse(const char *key) {
    BString magic = key;
    magic += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    uint8 hash[20]; // SHA1 20 bytes
    NetworkUtils::SHA1((uint8 *) magic.String(), magic.Length(), hash);

    BString acceptKey = NetworkUtils::Base64Encode(hash, 20);

    BString resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Accept: " << acceptKey << "\r\n\r\n";

    return resp;
}

size_t
NetworkServer::_ParseWebSocketFrame(ClientState *client) {
    if (client->buffer.empty()) return 0;
    const uint8 *buffer = client->buffer.data();
    size_t bufferLen = client->buffer.size();

    if (bufferLen < 2) return 0; // Incomplete Header

    // Byte 1: FIN(1) rsv(3) opcode(4)
    uint8 b1 = buffer[0];
    bool fin = (b1 & 0x80) != 0;
    uint8 opcode = b1 & 0x0F;

    // Byte 2: Mask(1) PayloadLen(7)
    uint8 b2 = buffer[1];
    bool masked = (b2 & 0x80) != 0;
    uint64 payloadLen = b2 & 0x7F;

    size_t headerSize = 2;

    if (payloadLen == 126) {
        if (bufferLen < 4) return 0;
        uint16 smallLen;
        memcpy(&smallLen, buffer + 2, 2);
        payloadLen = ntohs(smallLen);
        headerSize += 2;
    } else if (payloadLen == 127) {
        if (bufferLen < 10) return 0;
        // Manual Big Endian Unpack for 64-bit
        payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            payloadLen = (payloadLen << 8) | buffer[2 + i];
        }
        headerSize += 8;
    }

    uint8 maskKey[4];
    if (masked) {
        if (bufferLen < headerSize + 4) return 0; // Wait for mask key
        memcpy(maskKey, buffer + headerSize, 4);
        headerSize += 4;
    }

    // Check if full payload is available
    if (bufferLen < headerSize + payloadLen) return 0;

    // Extract payload
    uint8 *payload = new uint8[payloadLen];
    memcpy(payload, buffer + headerSize, payloadLen);

    // Unmask
    if (masked) {
        for (size_t i = 0; i < payloadLen; i++) {
            payload[i] ^= maskKey[i % 4];
        }
    }

    // Handle Packet
    if (fin) {
        _HandleInputPacket(client, opcode, payload, payloadLen);
    } else {
        // printf("Warning: Fragmented frames not fully supported yet.\n");
    }

    delete[] payload;
    return headerSize + payloadLen;
}

void
NetworkServer::WakeCapture() {
    if (fTarget.IsValid()) {
        fTarget.SendMessage(MSG_WAKE_CAPTURE);
    }
}

void
NetworkServer::SendMessageToTarget(BMessage *msg) {
    if (fTarget.IsValid()) {
        fTarget.SendMessage(msg);
    }
}

void
NetworkServer::SendToClient(ClientState *client, const void *data, size_t len) {
    if (!client || !client->sslAccepted) return;
    SSL_write(client->ssl, data, len); // Simple wrapper, assume non-blocking handles minor buffers
}

void
NetworkServer::_HandleInputPacket(ClientState *client, uint8 opcode, const uint8 *data, size_t len) {
    if (opcode != 0x02) return; // Only Binary

    // Parse Protobuf
    haiku::remote::InputEvent inputEvent;
    if (!inputEvent.ParseFromArray(data, len)) {
        fprintf(stderr, "NetworkServer: Failed to parse InputEvent proto. Len: %lu\n", len);
        return;
    }

    PacketHandler *handler = PacketHandlerFactory::GetHandler(inputEvent.type());
    if (handler) {
        handler->Handle(this, client, inputEvent);
    } else {
        fprintf(stderr, "NetworkServer: Unknown InputEvent type: %d\n", inputEvent.type());
    }
}

void
NetworkServer::_CheckClipboard() {
    static bigtime_t lastCheck = 0;
    if (system_time() - lastCheck < 1000000) return; // 1s interval
    lastCheck = system_time();

    if (be_clipboard->Lock()) {
        BMessage *clip = be_clipboard->Data();
        const char *text = nullptr;
        ssize_t textLen = 0;
        if (clip->FindData("text/plain", B_MIME_TYPE, (const void **) &text, &textLen) == B_OK) {
            BString current(text, textLen);
            if (current != fLastClipboardData) {
                fLastClipboardData = current;

                // Create Proto Message
                haiku::remote::InputEvent event;
                event.set_type(haiku::remote::InputEvent::CLIPBOARD);
                event.mutable_clipboard()->set_text(current.String());

                std::string serialized;
                event.SerializeToString(&serialized);

                // Create WebSocket Header (Binary 0x02)
                uint8 headerBuf[16];
                size_t headerLen = NetworkUtils::MakeWebSocketHeader(serialized.size(), headerBuf, 0x02);

                for (int32 i = 0; i < fClients.CountItems(); i++) {
                    ClientState *client = (ClientState *) fClients.ItemAt(i);
                    if (client->isWebSocket && client->sslAccepted) {
                        // Flatten for SSL
                        size_t totalLen = headerLen + serialized.size();
                        uint8 *flatBuf = new uint8[totalLen];
                        memcpy(flatBuf, headerBuf, headerLen);
                        memcpy(flatBuf + headerLen, serialized.data(), serialized.size());

                        size_t sentTotal = 0;
                        while (sentTotal < totalLen) {
                            int written = SSL_write(client->ssl, flatBuf + sentTotal, totalLen - sentTotal);
                            if (written <= 0) break;
                            sentTotal += written;
                        }
                        delete[] flatBuf;
                    }
                }
            }
        }
        be_clipboard->Unlock();
    }
}