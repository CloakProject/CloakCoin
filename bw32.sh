#!/bin/bash
MXE_PATH="/root/Desktop/mxe"

echo "Building CGB QT Wallet for Win32 x86 2..."
echo "MXEPATH = $MXE_PATH"

export PATH=$MXE_PATH/usr/bin:$PATH

$MXE_PATH/usr/i686-pc-mingw32.static/qt/bin/qmake USE_MXE=1 USE_LEVELDB=1 USE_QRCODE=1 && make
read -p "Press any key to exit... " -n1 -s