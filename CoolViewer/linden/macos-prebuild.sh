#!/bin/bash

export PATH="/Applications/CMake.app/Contents/bin:$PATH"

if ! which cmake &>/dev/null; then
	echo
	echo "You need to have 'cmake' installed and in your PATH !".
	exit 1
fi

top=`pwd`

if ! [ -x "./scripts/develop.py" ] ; then
	echo
	echo "This script must be ran from the linden/ Cool VL Viewer sources directory !".
	exit 1
fi

./scripts/develop.py -t Release configure -DNO_FATAL_WARNINGS:BOOL=TRUE

if ! [ -d "$top/build-darwin-x86_64/CoolVLViewer.xcodeproj" ] ; then
	echo
	echo "Failed to generate the project files, sorry !"
	exit 1
fi

# Create a dummy (empty) directory to prevent warnings at link time since cmake
# apparently adds a spurious "Release" sub-directory search path for libraries...
mkdir -p "$top/lib/release/Release"

echo
echo "Your may now:"
echo " - Launch Xcode and open the project:"
echo "   $top/build-darwin-x86_64/CoolVLViewer.xcodeproj"
echo " - In the Xcode window that opened on the CoolVLViewer project, look on"
echo "   the left of the title bar and check that the active scheme is reading"
echo "   'ALL_BUILD > My Mac' or 'viewer > My Mac' (if not, click on it and"
echo "   change it for one of those)."
echo " - Build the viewer from the menu bar 'Product' -> 'Build For' -> 'Running'."
