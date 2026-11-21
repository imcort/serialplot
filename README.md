# SerialPlot

Small and simple software for plotting data from serial port in realtime.

![SerialPlot Screenshot](./misc/screenshot.png)

## Features

* Reading data from serial port
* Binary data formats (u)int8, (u)int16, (u)int32, float
* User defined frame format for robust operation
* ASCII input (Comma Separated Values)
* Synchronized multi channel plotting
* Define and send commands to the device in ASCII or binary format
* Auxiliary marker input: every character received on a second serial port is
  drawn as a vertical line and recorded in the CSV (a marker flag plus the
  received character)
* Every setting (port, data format, plot, commands, record) can be saved to a
  configuration file and loaded back, from the menu, the command line or by
  dropping the file onto the window
* Take snapshots of the current waveform and save to CSV file

See
[hackaday.io](https://hackaday.io/project/5334-serialplot-realtime-plotting-software)
page for [screenshots](https://hackaday.io/project/5334/gallery).

## Installation

Check the releases page.

## Dependencies

- Qt 6, including SerialPort module
- Qwt 6.3

## Building

### Obtain Dependencies

- Qt6 development packages
- Qt SerialPort module
- CMake
- git

Under Ubuntu/Debian:
```apt install qt6-base-dev qt6-serialport-dev qt6-svg-dev git cmake build-essential```

### Download and Install Qwt [You can skip this step]

[Qwt](http://qwt.sourceforge.net) is the library that provides
plotting widgets for SerialPlot. You have 3 different options for Qwt.

* Leave it to serialplot build scripts. Qwt will be downloaded and built for you.

* Download Qwt 6 [here](http://sourceforge.net/projects/qwt/files/)
  and build it yourself per these
  [instructions](http://qwt.sourceforge.net/qwtinstall.html#qwtinstall-unix)
  and install. Make sure you use Qt6 to build. When calling cmake for
  serialplot, `BUILD_QWT` cmake option to `false`. If you didn't
  install qwt after building point `QWT_ROOT` variable to `qwt/`
  directory that you have just built.

### Download and Build SerialPlot

You can use git to download SerialPlot source code. 

    git clone https://github.com/hyOzd/serialplot
    cd serialplot
    mkdir build && cd build
    cmake ..
    make -j

### macOS compatibility

The macOS build targets macOS 14.0 by default, so it does not require the
version of macOS that was used to build it. Override the minimum version when
configuring:

    cmake -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 ..

For a universal binary that also runs on Intel Macs (requires a universal Qt
installation):

    cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" ..

The effective minimum is also limited by the Qt libraries. Use a Qt build whose
deployment target is at least as old as the desired macOS version (for example
the official Qt 6.8 builds support macOS 12 and newer). Homebrew add-on modules
may be built for the host version only; you can check with `otool -l <framework>`.

### macOS disk image (DMG)

`scripts/make-macos-dmg.sh` builds a Release bundle, copies in the required Qt,
Qwt and plugin binaries with `macdeployqt`, signs it and wraps it in a
drag-to-Applications disk image:

    scripts/make-macos-dmg.sh

The result is written to `dist/SerialPlot-<version>-macos<min>-<arch>.dmg` and
contains the app, an `Applications` link, a short Read Me and the license.
The script raises the minimum macOS version automatically when the installed
Qt modules were built for a newer system, and the file name reflects the real
minimum.

By default the app is signed ad-hoc, which is enough to run after the
recipient allows it in System Settings, but it still triggers Gatekeeper's
"unidentified developer" warning. For a warning-free download, sign with a
Developer ID certificate and notarize:

    CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)" \
    NOTARY_PROFILE="notarytool-keychain-profile" \
    scripts/make-macos-dmg.sh

`NOTARY_PROFILE` is the keychain profile created with
`xcrun notarytool store-credentials`.

## License

This software is licensed under GPLv3 excluding some third party
files. See file COPYING for details.
