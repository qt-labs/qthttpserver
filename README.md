# qthttpserver
Qt HTTP Server installation steps

## Installation on Windows
Here are the requirements to install The Qt HTTP server on Windows

### Requirements
1. Install Perl (https://strawberryperl.com/)
2. Visual Studio

### Steps to obtain `dll` files
1. Open Command prompt
2. `cd path/to/qthttpserver`
3. `vcvars64`
4. Initialize submodules `git submodule update --init`
5. `qmake`
6. `nmake`
7. `nmake install`
