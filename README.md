# Haiku Remote Desktop Server

A high-performance, web-based remote desktop server for Haiku OS. This application allows you to control your Haiku system remotely via any modern web browser using WebRTC for low-latency video streaming and WebSocket for input control.

## Features

-   **Web-Based Client**: No client software required. Works in Chrome, Firefox, Safari, etc.
-   **Low Latency**: Uses efficient video codecs (VP8/VP9) and WebRTC streaming.
-   **Input Injection**: Full mouse and keyboard control via a custom Input Server Add-on.
-   **Clipboard Sync**: Bidirectional text clipboard synchronization between client and server.
-   **Secure**: Uses SSL/TLS on port **8443**.
-   **Native Integration**: Installs as a native Haiku package with Deskbar integration.

## Architecture

The system consists of two main components:

1.  **Screen Server (`screen_server`)**: The main userland application. It captures the screen, encodes the video stream, serves the web interface, and handles network communication.
2.  **Virtual Input Add-on (`VirtualMouse`)**: A system add-on (`input_server` device) that allows the server to inject mouse and keyboard events directly into the OS input stream.

*Note: The server automatically installs the input add-on to `/boot/home/config/non-packaged/add-ons/input_server/devices/` upon first launch if it's not present.*

## Requirements

-   **Haiku OS** (x86_64)
  
## Build Instructions

1. **Install Dependencies**
    ```bash
    pkgman install cmake gcc make nodejs20 rsync protobuf_devel x264_devel npm
    ```

2.  **Clone the repository**:
    ```bash
    git clone https://github.com/your-repo/HaikuRemoteDesktop.git
    cd HaikuRemoteDesktop
    ```

3.  **Create a build directory**:
    ```bash
    mkdir build
    cd build
    ```

4.  **Configure and Build**:
    ```bash
    cmake ..
    make
    ```

5.  **Create Haiku Package (.hpkg)**:
    To generate an installable package:
    ```bash
    make package_haiku
    ```
    This will create `HaikuRemoteDesktop-1.0.1-1-x86_64.hpkg` in your build directory.

## Installation & Usage

### Method 1: Install Package (Recommended)

1.  Double-click `HaikuRemoteDesktop-1.0.1-1-x86_64.hpkg` to install it.
2.  Open the **Deskbar**, navigate to **Applications**, and launch **HaikuRemoteDesktop**.
3.  The server will start in the background.

### Method 2: Run Manually

From the build directory:
```bash
dist/screen_server
```

### Accessing the Client

1.  Open a web browser on another machine on the same network.
2.  Navigate to: `https://<haiku-ip-address>:8443`
3.  Accept the self-signed certificate warning (first time only).
4.  You should now see your Haiku desktop!

## Cross-Compilation (Linux/Docker)

You can build this project from Linux using the provided Dockerfile.

## Cross-Compilation (Linux/Docker)

### 1. Build the Docker Image
Run this command in your terminal where the Dockerfile is located:

```bash
docker build -t my-haiku-app .
```

### 2. Run the Compilation
You can now run the build. The Dockerfile already contains all necessary dependencies.
We mount the current directory to `/work` so that the compiled binary appears in your local folder.

```bash
docker run --rm -v $(pwd):/work my-haiku-app
```

**Note:** The build uses a "staged" target (`stage_haiku`) because the `package create` tool has limitations in the cross-compilation environment.
The compiled binaries and assets will be available in:
`build/package_staging/`

You can copy this folder to your Haiku system and run `package create` manually if you need a `.hpkg`, or just run the executable directly.

## Development

-   **Web Assets**: Located in `src/UserlandServer/index.html`.
-   **Port Configuration**: Default port is **8443**.
-   **Logs**: Server logs to stdout/stderr. Input driver logs to syslog.

## Notes
- This application was mostly vibe-coded using Antigravity and Gemini 3.0

## License

MIT License