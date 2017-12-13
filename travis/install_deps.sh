#!/usr/bin/env bash

set -e

REPO_NAME=${TRAVIS_REPO_SLUG#*/}
ARCHIVE_NAME=deps_${PLATFORM}_${REPO_NAME}.tar.gz

# Packages.
case ${PLATFORM} in
     "native_static")
         PACKAGES="gcc cmake libbz2-dev ccache zlib1g-dev uuid-dev libctpp2-dev ctpp2-utils"
         ;;
     "native_dyn")
         PACKAGES="gcc cmake libbz2-dev ccache zlib1g-dev uuid-dev libctpp2-dev ctpp2-utils libmicrohttpd-dev"
         ;;
     "win32_static")
         PACKAGES="g++-mingw-w64-i686 gcc-mingw-w64-i686 gcc-mingw-w64-base mingw-w64-tools ccache ctpp2-utils"
         ;;
     "win32_dyn")
         PACKAGES="g++-mingw-w64-i686 gcc-mingw-w64-i686 gcc-mingw-w64-base mingw-w64-tools ccache ctpp2-utils"
         ;;
     "android_arm")
         PACKAGES="gcc cmake ccache ctpp2-utils"
         ;;
     "android_arm64")
         PACKAGES="gcc cmake ccache ctpp2-utils"
         ;;
esac

sudo apt-get update -qq
sudo apt-get install -qq python3-pip ${PACKAGES}
sudo pip3 install meson==0.43.0

# Ninja
cd $HOME
git clone git://github.com/ninja-build/ninja.git
cd ninja
git checkout release
./configure.py --bootstrap
sudo cp ninja /bin

# Dependencies comming from kiwix-build.
cd ${HOME}
wget http://tmp.kiwix.org/ci/${ARCHIVE_NAME}
mkdir -p BUILD_${PLATFORM}
cd BUILD_${PLATFORM}
tar xf ${HOME}/${ARCHIVE_NAME}
