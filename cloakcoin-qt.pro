QT += core gui network
QT += widgets
TEMPLATE = app
TARGET = cloakcoin-qt
VERSION = 2.1.0
INCLUDEPATH += src src/json src/qt src/tor
DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE BOOST_THREAD_PROVIDES_GENERIC_SHARED_MUTEX_ON_WIN __NO_SYSTEM_INCLUDES
#DEFINES += CURL_STATICLIB
CONFIG += no_include_pwd
CONFIG += thread
CONFIG += static

message($$QMAKESPEC)

#PRECOMPILED_HEADER = src/stable.h

#DEFINES += DEBUG_LOCKCONTENTION DEBUG_LOCKORDER

#CONFIG += console
# UNCOMMENT THIS TO GET CONSOLE QDEBUG OUTPUT
# CONFIG += console

greaterThan(QT_MAJOR_VERSION, 4) {
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0
}

#DEFINES += ENABLE_WALLET

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

message(LIBS = $$LIBS)

# setup windows specific libs, using msys mingw x-compile environment
win32 {
    message(*** win32 build ***)
    #LIBS += -L$$PWD/ex_lib/libcurl -lcurldll
    LIBS += C:\deps\curl-7.40.0\lib\.libs\libcurl.dll.a
	LIBS += $$PWD/src/leveldb/lib/win/x86/libleveldb.a $$PWD/src/leveldb/lib/win/x86/libmemenv.a

    INCLUDEPATH+=C:\deps\curl-7.40.0\include
    INCLUDEPATH+=C:\deps\libevent-2.0.21-stable\include
    INCLUDEPATH += $$PWD/ex_lib/qrencode-3.4.1-1-mingw32-dev
    INCLUDEPATH += $$PWD/src/leveldb/include/leveldb
    INCLUDEPATH += $$PWD/build	

    DEFINES += USE_LEVELDB
    INCLUDEPATH += src/leveldb/include src/leveldb/helpers src/leveldb/helpers/memenv
    SOURCES += src/txdb-leveldb.cpp

    BOOST_INCLUDE_PATH += C:/deps/boost_1_57_0
    BOOST_LIB_PATH=C:\deps\boost_1_57_0\stage\lib

    BDB_INCLUDE_PATH=C:\deps\db-4.8.30.NC\build_unix
    BDB_LIB_PATH=C:\deps\db-4.8.30.NC\build_unix
	
    OPENSSL_INCLUDE_PATH = C:\deps\openssl-1.0.2g\include
    OPENSSL_LIB_PATH=C:\deps\openssl-1.0.2g

    QRENCODE_LIB_PATH="C:\deps\qrencode-3.4.4\.libs"
    QRENCODE_INCLUDE_PATH="C:\deps\qrencode-3.4.4"

    BOOST_LIB_SUFFIX = -mgw49-mt-s-1_57

    LIBS+=-LC:\deps\libevent-2.0.21-stable\.libs -LC:\deps\libpng-1.6.16\.libs
    LIBS+=-lcrypto -lws2_32 -lgdi32 -lcrypt32
}

macx {
     message(*** osx build ***)

     QMAKE_MAC_SDK = macosx10.9

     #QMAKE_RPATHDIR += /Users/joe/qt/Qt5.7.1/5.7/clang_64/lib

     #set RPATH (place to look for .dylib & framework by default)
     QMAKE_RPATHDIR += /Users/joe/qt/Qt5.7.1/5.7/clang_64/lib
     #QMAKE_RPATHDIR += @executable_path/../Frameworks
     #QMAKE_RPATHDIR += @executable_path/lib
     #QMAKE_RPATHDIR += @executable_path

     BOOST_INCLUDE_PATH += /Users/joe/Documents/cloak_deps/boost_1_57_0
     BOOST_LIB_PATH=/Users/joe/Documents/cloak_deps/boost_1_57_0/stage/lib

     BOOST_LIB_SUFFIX = -clang-darwin42-mt-s-1_57

     #BDB_INCLUDE_PATH = /opt/local/include/db48
     #BDB_LIB_PATH = /opt/local/lib/db48

     BDB_INCLUDE_PATH = /Users/joe/Documents/cloak_deps/db-4.8.30.NC/build_unix
     BDB_LIB_PATH = /Users/joe/Documents/cloak_deps/db-4.8.30.NC/build_unix
     BDB_LIB_SUFFIX = -4.8

     OPENSSL_INCLUDE_PATH = /usr/local/ssl/include
     OPENSSL_LIB_PATH = /usr/local/ssl/lib

     QRENCODE_LIB_PATH="/opt/local/lib"
     QRENCODE_INCLUDE_PATH="/opt/local/include"

     DEFINES += USE_LEVELDB
     INCLUDEPATH += src/leveldb/include src/leveldb/helpers src/leveldb/helpers/memenv
     SOURCES += src/txdb-leveldb.cpp
     LIBS+=$$PWD/src/leveldb/libleveldb.a $$PWD/src/leveldb/libmemenv.a
     LIBS+=/opt/local/lib/libcurl.a
     LIBS+=-L/usr/local/Cellar/libevent/2.0.22/lib/
     INCLUDEPATH+=/opt/local/include/curl
     INCLUDEPATH+=/usr/local/Cellar/libevent/2.0.22/include
}

linux {
     message(*** linux build ***)

     QRENCODE_LIB_PATH="/opt/deps/qrencode-3.4.4/.libs"
     QRENCODE_INCLUDE_PATH="/opt/deps/qrencode-3.4.4"

     OPENSSL_INCLUDE_PATH="/opt/deps/openssl-1.0.2g/include"
     OPENSSL_LIB_PATH="/opt/deps/openssl-1.0.2g"

     DEFINES += USE_LEVELDB
     INCLUDEPATH += src/leveldb/include src/leveldb/helpers src/leveldb/helpers/memenv
     SOURCES += src/txdb-leveldb.cpp
     LIBS+=$$PWD/src/leveldb/libleveldb.a $$PWD/src/leveldb/libmemenv.a /opt/deps/openssl-1.0.2g/libssl.a /opt/deps/openssl-1.0.2g/libcrypto.a
}

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.12
# use: qmake "RELEASE=1"
contains(RELEASE, 1) {
    message(*** release ***)
    # Mac: compile for maximum compatibility (10.5, 32-bit)
    macx:QMAKE_CXXFLAGS += -mmacosx-version-min=10.7 #-arch i386 -isysroot /Developer/SDKs/MacOSX10.7.sd
    #macx:QMAKE_LFLAGS += -no_weak_imports

    !windows:!macx {
        # Linux: static link
        LIBS += -Wl,-Bstatic
    }
}

!win32 {
# for extra security against potential buffer overflows: enable GCCs Stack Smashing Protection
QMAKE_CXXFLAGS *= -fstack-protector-all --param ssp-buffer-size=1 -std=c++11
QMAKE_LFLAGS *= -fstack-protector-all --param ssp-buffer-size=1 -std=c++11 -fno-pie -no-pie
# We need to exclude this for Windows cross compile with MinGW 4.2.x, as it will result in a non-working executable!
# This can be enabled for Windows, when we switch to MinGW >= 4.4.x.
}
# for extra security on Windows: enable ASLR and DEP via GCC linker flags
win32:QMAKE_LFLAGS *= -Wl,--dynamicbase -Wl,--nxcompat
# on Windows: enable GCC large address aware linker flag
win32:QMAKE_LFLAGS *= -Wl,--large-address-aware

# use: qmake "USE_QRCODE=1"
# libqrencode (http://fukuchi.org/works/qrencode/index.en.html) must be installed for support
contains(USE_QRCODE, 1) {
    message(Building with QRCode support)
    DEFINES += USE_QRCODE
        !contains(USE_MXE, 1) {
                LIBS += -lqrencode
	}   
}

LIBS += -levent
#DEFINES += CURL_STATICLIB
INCLUDEPATH += ex_lib

# use: qmake "USE_UPNP=1" ( enabled by default; default)
#  or: qmake "USE_UPNP=0" (disabled by default)
#  or: qmake "USE_UPNP=-" (not supported)
# miniupnpc (http://miniupnp.free.fr/files/) must be installed for support
contains(USE_UPNP, -) {
    message(Building without UPNP support)
} else {
    message(Building with UPNP support)
    count(USE_UPNP, 0) {
        USE_UPNP=1
    }
    INCLUDEPATH += $$PWD/src
    win32:INCLUDEPATH += C:/deps/miniupnpc
    INCLUDEPATH += /opt/deps/miniupnpc
    DEFINES += USE_UPNP=$$USE_UPNP MINIUPNP_STATICLIB
    win32:LIBS += C:/deps/miniupnpc/libminiupnpc.a
    macx:LIBS += /opt/local/lib/libminiupnpc.a
    !windows:!macx{
        LIBS += -lminiupnpc
    }
    win32:LIBS += -liphlpapi
}


# use: qmake "USE_DBUS=1"
contains(USE_DBUS, 1) {
    message(Building with DBUS (Freedesktop notifications) support)
    DEFINES += USE_DBUS
    QT += dbus
}

# use: qmake "USE_IPV6=1" ( enabled by default; default)
#  or: qmake "USE_IPV6=0" (disabled by default)
#  or: qmake "USE_IPV6=-" (not supported)
contains(USE_IPV6, -) {
    message(Building without IPv6 support)
} else {
    message(Building with IPv6 support)
    count(USE_IPV6, 0) {
        USE_IPV6=1
    }
    DEFINES += USE_IPV6=$$USE_IPV6
}

contains(BITCOIN_NEED_QT_PLUGINS, 1) {
    DEFINES += BITCOIN_NEED_QT_PLUGINS
    QTPLUGIN += qcncodecs qjpcodecs qtwcodecs qkrcodecs qtaccessiblewidgets
}

# regenerate src/build.h
#!windows|contains(USE_BUILD_INFO, 1) {
#    genbuild.depends = FORCE
#    genbuild.commands = cd $$PWD; /bin/sh share/genbuild.sh $$OUT_PWD/build/build.h
#    genbuild.target = $$OUT_PWD/build/build.h
#    PRE_TARGETDEPS += $$OUT_PWD/build/build.h
#    QMAKE_EXTRA_TARGETS += genbuild
#    DEFINES += HAVE_BUILD_INFO
#}

QMAKE_CXXFLAGS += -msse2
QMAKE_CFLAGS += -msse2
QMAKE_CXXFLAGS_WARN_ON = -fdiagnostics-show-option -Wall -Wextra -Wformat -Wformat-security -Wno-unused-parameter -Wstack-protector

# Input
DEPENDPATH += src src/json src/qt
HEADERS += src/qt/mainwindow.h \
    src/qt/httpsocket.h \
    src/qt/cloaksend.h \
    src/qt/exchange.h \
    src/qt/QJsonArray.h \
    src/qt/QJsonDocument.h \
    src/qt/QJsonObject.h \
    src/qt/QJsonParseError.h \
    src/qt/QJsonParser.h \
    src/qt/QJsonRoot.h \
    src/qt/QJsonValue.h \
    src/qt/QJsonValueRef.h \
    src/qt/tickertimer.h \
    src/qt/jsonsaverloader.h \
    src/qt/bitcoingui.h \
    src/qt/transactiontablemodel.h \
    src/qt/addresstablemodel.h \
    src/qt/optionsdialog.h \
    src/qt/coincontroldialog.h \
    src/qt/coincontroltreewidget.h \
    src/qt/sendcoinsdialog.h \
    src/qt/addressbookpage.h \
    src/qt/signverifymessagedialog.h \
    src/qt/aboutdialog.h \
    src/qt/editaddressdialog.h \
    src/qt/bitcoinaddressvalidator.h \
    src/alert.h \
    src/addrman.h \
    src/base58.h \
    src/bignum.h \
    src/checkpoints.h \
    src/compat.h \
    src/coincontrol.h \
    src/sync.h \
    src/util.h \
    src/uint256.h \
    src/kernel.h \
    src/scrypt_mine.h \
    src/pbkdf2.h \
    src/serialize.h \
    src/strlcpy.h \
    src/main.h \
    src/net.h \
    src/key.h \
    src/db.h \
    src/walletdb.h \
    src/script.h \
    src/init.h \
    src/irc.h \
    src/mruset.h \
    src/json/json_spirit_writer_template.h \
    src/json/json_spirit_writer.h \
    src/json/json_spirit_value.h \
    src/json/json_spirit_utils.h \
    src/json/json_spirit_stream_reader.h \
    src/json/json_spirit_reader_template.h \
    src/json/json_spirit_reader.h \
    src/json/json_spirit_error_position.h \
    src/json/json_spirit.h \
    src/qt/clientmodel.h \
    src/qt/guiutil.h \
    src/qt/transactionrecord.h \
    src/qt/guiconstants.h \
    src/qt/optionsmodel.h \
    src/qt/monitoreddatamapper.h \
    src/qt/transactiondesc.h \
    src/qt/transactiondescdialog.h \
    src/qt/bitcoinamountfield.h \
    src/wallet.h \
    src/keystore.h \
    src/qt/transactionfilterproxy.h \
    src/qt/transactionview.h \
    src/qt/walletmodel.h \
    src/bitcoinrpc.h \
    src/qt/overviewpage.h \
    src/qt/csvmodelwriter.h \
    src/crypter.h \
    src/qt/sendcoinsentry.h \
    src/qt/qvalidatedlineedit.h \
    src/qt/bitcoinunits.h \
    src/qt/qvaluecombobox.h \
    src/qt/askpassphrasedialog.h \
    src/protocol.h \
    src/qt/notificator.h \
    src/qt/qtipcserver.h \
    src/allocators.h \
    src/ui_interface.h \
    src/qt/rpcconsole.h \
    src/version.h \
    src/netbase.h \
    src/clientversion.h \
    src/hashblock.h \
    src/sph_blake.h \
    src/sph_bmw.h \
    src/sph_cubehash.h \
    src/sph_echo.h \
    src/sph_groestl.h \
    src/sph_jh.h \
    src/sph_keccak.h \
    src/sph_luffa.h \
    src/sph_shavite.h \
    src/sph_simd.h \
    src/sph_skein.h \
    src/sph_fugue.h \
    src/sph_hamsi.h \
    src/sph_types.h \
    src/stealth.h \
    src/qt/enigmastatuspage.h \
    src/qt/enigmatablemodel.h \
    src/lrucache.h \
    src/ecies/ecies.h \
    src/enigma/enigma.h \
    src/enigma/cloakshield.h \
    src/enigma/enigmaann.h \
    src/qt/splashscreen.h \
    src/bitcoin.h \
    src/qt/utilitydialog.h \
    src/qt/winshutdownmonitor.h \
    src/stable.h \
    src/txdb.h \
    src/txdb-leveldb.h \
    src/txdb-bdb.h \
    src/cloakhelpers.h \
    src/enigma/enigmaann.h \
    src/filedownloader.h \
    src/enigma/cloakingdata.h \
    src/enigma/cloakingrequest.h \
    src/miniupnpc/bsdqueue.h \
    src/miniupnpc/codelength.h \
    src/miniupnpc/connecthostport.h \
    src/miniupnpc/declspec.h \
    src/miniupnpc/igd_desc_parse.h \
    src/miniupnpc/minisoap.h \
    src/miniupnpc/minissdpc.h \
    src/miniupnpc/miniupnpc.h \
    src/miniupnpc/miniupnpcstrings.h \
    src/miniupnpc/miniupnpctypes.h \
    src/miniupnpc/miniwget.h \
    src/miniupnpc/minixml.h \
    src/miniupnpc/portlistingparse.h \
    src/miniupnpc/receivedata.h \
    src/miniupnpc/upnpcommands.h \
    src/miniupnpc/upnperrors.h \
    src/miniupnpc/upnpreplyparse.h \
    src/tor/address.h \
    src/tor/addressmap.h \
    src/tor/aes.h \
    src/tor/backtrace.h \
    src/tor/buffers.h \
    src/tor/channel.h \
    src/tor/channeltls.h \
    src/tor/circpathbias.h \
    src/tor/circuitbuild.h \
    src/tor/circuitlist.h \
    src/tor/circuitmux.h \
    src/tor/circuitmux_ewma.h \
    src/tor/circuitstats.h \
    src/tor/circuituse.h \
    src/tor/cloak.h \
    src/tor/command.h \
    src/tor/compat_libevent.h \
    src/tor/config.h \
    src/tor/confparse.h \
    src/tor/connection.h \
    src/tor/connection_edge.h \
    src/tor/connection_or.h \
    src/tor/container.h \
    src/tor/control.h \
    src/tor/cpuworker.h \
    src/tor/crypto.h \
    src/tor/crypto_curve25519.h \
    src/tor/di_ops.h \
    src/tor/directory.h \
    src/tor/dirserv.h \
    src/tor/dirvote.h \
    src/tor/dns.h \
    src/tor/dnsserv.h \
    src/tor/entrynodes.h \
    src/tor/eventdns.h \
    src/tor/eventdns_tor.h \
    src/tor/ext_orport.h \
    src/tor/fp_pair.h \
    src/tor/geoip.h \
    src/tor/hibernate.h \
    src/tor/ht.h \
    src/tor/memarea.h \
    src/tor/mempool.h \
    src/tor/microdesc.h \
    src/tor/networkstatus.h \
    src/tor/nodelist.h \
    src/tor/ntmain.h \
    src/tor/onion.h \
    src/tor/onion_fast.h \
    src/tor/onion_main.h \
    src/tor/onion_ntor.h \
    src/tor/onion_tap.h \
    src/tor/or.h \
    src/tor/orconfig.h \
    src/tor/orconfig_apple.h \
    src/tor/orconfig_linux.h \
    src/tor/orconfig_win32.h \
    src/tor/policies.h \
    src/tor/procmon.h \
    src/tor/reasons.h \
    src/tor/relay.h \
    src/tor/rendclient.h \
    src/tor/rendcommon.h \
    src/tor/rendmid.h \
    src/tor/rendservice.h \
    src/tor/rephist.h \
    src/tor/replaycache.h \
    src/tor/router.h \
    src/tor/routerlist.h \
    src/tor/routerparse.h \
    src/tor/routerset.h \
    src/tor/sandbox.h \
    src/tor/statefile.h \
    src/tor/status.h \
    src/tor/testsupport.h \
    src/tor/tinytest.h \
    src/tor/tinytest_macros.h \
    src/tor/tor_compat.h \
    src/tor/tor_queue.h \
    src/tor/tor_util.h \
    src/tor/torgzip.h \
    src/tor/torint.h \
    src/tor/torlog.h \
    src/tor/tortls.h \
    src/tor/transports.h \
    src/scrypt.h \
    src/enigma/enigmapeer.h \
    src/hdkeys.h \
    src/hdkey.h \
    src/cloakexception.h \
    src/enigma/encryption.h \
    src/enigma/pow.h

SOURCES += \
    src/tor/address.c \
    src/tor/addressmap.c \
    src/tor/aes.c \
    src/tor/backtrace.c \
    src/tor/buffers.c \
    src/tor/channel.c \
    src/tor/channeltls.c \
    src/tor/circpathbias.c \
    src/tor/circuitbuild.c \
    src/tor/circuitlist.c \
    src/tor/circuitmux.c \
    src/tor/circuitmux_ewma.c \
    src/tor/circuitstats.c \
    src/tor/circuituse.c \
    src/tor/command.c \
    src/tor/compat.c \
    src/tor/compat_libevent.c \
    src/tor/config.c \
    src/tor/config_codedigest.c \
    src/tor/confparse.c \
    src/tor/connection.c \
    src/tor/connection_edge.c \
    src/tor/connection_or.c \
    src/tor/container.c \
    src/tor/control.c \
    src/tor/cpuworker.c \
    src/tor/crypto.c \
    src/tor/crypto_curve25519.c \
    src/tor/crypto_format.c \
    src/tor/curve25519-donna.c \
    src/tor/di_ops.c \
    src/tor/directory.c \
    src/tor/dirserv.c \
    src/tor/dirvote.c \
    src/tor/dns.c \
    src/tor/dnsserv.c \
    src/tor/entrynodes.c \
    src/tor/ext_orport.c \
    src/tor/fp_pair.c \
    src/tor/geoip.c \
    src/tor/hibernate.c \
    src/tor/log.c \
    src/tor/memarea.c \
    src/tor/mempool.c \
    src/tor/microdesc.c \
    src/tor/networkstatus.c \
    src/tor/nodelist.c \
    src/tor/onion.c \
    src/tor/onion_fast.c \
    src/tor/onion_main.c \
    src/tor/onion_ntor.c \
    src/tor/onion_tap.c \
    src/tor/policies.c \
    src/tor/cloak.cpp \
    src/tor/procmon.c \
    src/tor/reasons.c \
    src/tor/relay.c \
    src/tor/rendclient.c \
    src/tor/rendcommon.c \
    src/tor/rendmid.c \
    src/tor/rendservice.c \
    src/tor/rephist.c \
    src/tor/replaycache.c \
    src/tor/router.c \
    src/tor/routerlist.c \
    src/tor/routerparse.c \
    src/tor/routerset.c \
    src/tor/sandbox.c \
    src/tor/statefile.c \
    src/tor/status.c \
    src/tor/strlcat.c \
    src/tor/strlcpy.c \
    src/tor/tor_util.c \
    src/tor/torgzip.c \
    src/tor/tortls.c \
    src/tor/transports.c \
    src/tor/util_codedigest.c \
    src/alert.cpp \
    src/version.cpp \
    src/sync.cpp \
    src/util.cpp \
    src/netbase.cpp \
    src/key.cpp \
    src/script.cpp \
    src/main.cpp \
    src/init.cpp \
    src/net.cpp \
    src/irc.cpp \
    src/checkpoints.cpp \
    src/addrman.cpp \
    src/db.cpp \
    src/walletdb.cpp \
    src/qt/hmac_sha512.cpp \
    src/qt/exchange.cpp \
    src/qt/QJsonArray.cpp \
    src/qt/QJsonDocument.cpp \
    src/qt/QJsonObject.cpp \
    src/qt/QJsonParseError.cpp \
    src/qt/QJsonParser.cpp \
    src/qt/QJsonValue.cpp \
    src/qt/QJsonValueRef.cpp \
    src/qt/tickertimer.cpp \
    src/qt/jsonsaverloader.cpp \
    src/qt/bitcoin.cpp src/qt/bitcoingui.cpp \
    src/qt/transactiontablemodel.cpp \
    src/qt/addresstablemodel.cpp \
    src/qt/optionsdialog.cpp \
    src/qt/sendcoinsdialog.cpp \
    src/qt/coincontroldialog.cpp \
    src/qt/coincontroltreewidget.cpp \
    src/qt/addressbookpage.cpp \
    src/qt/signverifymessagedialog.cpp \
    src/qt/aboutdialog.cpp \
    src/qt/editaddressdialog.cpp \
    src/qt/bitcoinaddressvalidator.cpp \
    src/qt/clientmodel.cpp \
    src/qt/guiutil.cpp \
    src/qt/transactionrecord.cpp \
    src/qt/optionsmodel.cpp \
    src/qt/monitoreddatamapper.cpp \
    src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp \
    src/qt/bitcoinstrings.cpp \
    src/qt/bitcoinamountfield.cpp \
    src/wallet.cpp \
    src/keystore.cpp \
    src/qt/transactionfilterproxy.cpp \
    src/qt/transactionview.cpp \
    src/qt/walletmodel.cpp \
    src/bitcoinrpc.cpp \
    src/rpcdump.cpp \
    src/rpcnet.cpp \
    src/rpcmining.cpp \
    src/rpcwallet.cpp \
    src/rpcblockchain.cpp \
    src/rpcrawtransaction.cpp \
    src/qt/overviewpage.cpp \
    src/qt/csvmodelwriter.cpp \
    src/crypter.cpp \
    src/qt/sendcoinsentry.cpp \
    src/qt/qvalidatedlineedit.cpp \
    src/qt/bitcoinunits.cpp \
    src/qt/qvaluecombobox.cpp \
    src/qt/askpassphrasedialog.cpp \
    src/protocol.cpp \
    src/qt/notificator.cpp \
    src/qt/qtipcserver.cpp \
    src/qt/rpcconsole.cpp \
    src/noui.cpp \
    src/kernel.cpp \
    src/scrypt-x86.S \
    src/scrypt-x86_64.S \
    src/scrypt_mine.cpp \
    src/pbkdf2.cpp \
    src/aes_helper.c \
    src/blake.c \
    src/bmw.c \
    src/cubehash.c \
    src/echo.c \
    src/groestl.c \
    src/jh.c \
    src/keccak.c \
    src/luffa.c \
    src/shavite.c \
    src/simd.c \
    src/skein.c \
    src/fugue.c \
    src/hamsi.c \
    src/scrypt.cpp \
    src/qt/httpsocket.cpp \
    src/stealth.cpp \
    src/qt/enigmastatuspage.cpp \
    src/qt/enigmatablemodel.cpp \
    src/ecies/ecies.c \
    src/ecies/keys.c \
    src/enigma/cloakshield.cpp \
    src/enigma/enigmaann.cpp \
    src/enigma/enigma.cpp \
    src/qt/mainwindow.cpp \
    src/qt/splashscreen.cpp \
    src/qt/utilitydialog.cpp \
    src/qt/winshutdownmonitor.cpp \
    src/cloakhelpers.cpp \
    src/filedownloader.cpp \
    src/enigma/cloakingdata.cpp \
    src/enigma/cloakingrequest.cpp \
    src/json/json_spirit_reader.cpp \
    src/json/json_spirit_value.cpp \
    src/json/json_spirit_writer.cpp \
    src/hamsi_helper.c \
    src/enigma/enigmapeer.cpp \
    src/cloakexception.cpp \
    src/enigma/encryption.cpp \
    src/enigma/pow.cpp

RESOURCES += \
    src/qt/bitcoin.qrc

FORMS += \
    src/qt/forms/mainwindow.ui \
    src/qt/forms/coincontroldialog.ui \
    src/qt/forms/sendcoinsdialog.ui \
    src/qt/forms/addressbookpage.ui \
    src/qt/forms/signverifymessagedialog.ui \
    src/qt/forms/aboutdialog.ui \
    src/qt/forms/editaddressdialog.ui \
    src/qt/forms/transactiondescdialog.ui \
    src/qt/forms/overviewpage.ui \
    src/qt/forms/sendcoinsentry.ui \
    src/qt/forms/askpassphrasedialog.ui \
    src/qt/forms/rpcconsole.ui \
    src/qt/forms/optionsdialog.ui \
    src/qt/forms/enigmastatuspage.ui \
    src/qt/forms/helpmessagedialog.ui

contains(USE_QRCODE, 1) {
HEADERS += src/qt/qrcodedialog.h
SOURCES += src/qt/qrcodedialog.cpp
FORMS += src/qt/forms/qrcodedialog.ui
}

contains(BITCOIN_QT_TEST, 1) {
SOURCES += src/qt/test/test_main.cpp \
    src/qt/test/uritests.cpp
HEADERS += src/qt/test/uritests.h
DEPENDPATH += src/qt/test
QT += testlib
TARGET = cloakcoin-qt_test
DEFINES += BITCOIN_QT_TEST
}

CODECFORTR = UTF-8

# for lrelease/lupdate
# also add new translations to src/qt/bitcoin.qrc under translations/
TRANSLATIONS = src/qt/locale/bitcoin_en.ts src/qt/locale/bitcoin_ru.ts src/qt/locale/bitcoin_fr.ts

isEmpty(QMAKE_LRELEASE) {
    #win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease.exe
win32:QMAKE_LRELEASE = $$[QT_INSTALL_BINS]\\lrelease
    else:QMAKE_LRELEASE = /usr/local/Qt-5.5.1/bin/lrelease
}
isEmpty(QM_DIR):QM_DIR = $$PWD/src/qt/locale

# automatically build translations, so they can be included in resource file
TSQM.name = lrelease ${QMAKE_FILE_IN}
TSQM.input = TRANSLATIONS
TSQM.output = $$QM_DIR/${QMAKE_FILE_BASE}.qm
TSQM.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
TSQM.CONFIG = no_link
QMAKE_EXTRA_COMPILERS += TSQM

message($$QMAKE_LRELEASE)

# "Other files" to show in Qt Creator
OTHER_FILES += \
    doc/*.rst doc/*.txt doc/README README.md res/bitcoin-qt.rc src/test/*.cpp src/test/*.h src/qt/test/*.cpp src/qt/test/*.h \
	src/qt/locale/*.ts

# platform specific defaults, if not overridden on command line

isEmpty(BOOST_LIB_SUFFIX) {
    macx:BOOST_LIB_SUFFIX = -mt
    #windows:BOOST_LIB_SUFFIX = -mgw44-mt-s-1_50
}

isEmpty(BOOST_THREAD_LIB_SUFFIX) {
    BOOST_THREAD_LIB_SUFFIX = $$BOOST_LIB_SUFFIX
}

isEmpty(BDB_LIB_SUFFIX) {
    macx:BDB_LIB_SUFFIX = -4.8
}

windows:DEFINES += WIN32
windows:RC_FILE = src/qt/res/bitcoin-qt.rc

windows:!contains(MINGW_THREAD_BUGFIX, 0) {
    # At least qmake's win32-g++-cross profile is missing the -lmingwthrd
    # thread-safety flag. GCC has -mthreads to enable this, but it doesn't
    # work with static linking. -lmingwthrd must come BEFORE -lmingw, so
    # it is prepended to QMAKE_LIBS_QT_ENTRY.
    # It can be turned off with MINGW_THREAD_BUGFIX=0, just in case it causes
    # any problems on some untested qmake profile now or in the future.
    DEFINES += _MT
    QMAKE_LIBS_QT_ENTRY = -lmingwthrd $$QMAKE_LIBS_QT_ENTRY
}

!windows:!macx {
    DEFINES += LINUX
    LIBS += -lrt
}

macx:HEADERS += src/qt/macdockiconhandler.h
macx:OBJECTIVE_SOURCES += src/qt/macdockiconhandler.mm
macx:LIBS += -framework Foundation -framework ApplicationServices -framework AppKit
macx:DEFINES += MAC_OSX MSG_NOSIGNAL=0
macx:ICON = src/qt/res/icons/bitcoin.icns
macx:TARGET = "cloakcoin-qt"
macx:QMAKE_CFLAGS_THREAD += -pthread
macx:QMAKE_LFLAGS_THREAD += -pthread
macx:QMAKE_CXXFLAGS_THREAD += -pthread

# libsecp256k1
#INCLUDEPATH += $$PWD/src/secp256k1/include
#LIBS += $$PWD/src/secp256k1/.libs/libsecp256k1.a

# Set libraries and includes at end, to use platform-defined defaults if not overridden
INCLUDEPATH += $$BOOST_INCLUDE_PATH $$BDB_INCLUDE_PATH $$OPENSSL_INCLUDE_PATH $$QRENCODE_INCLUDE_PATH
# libdb_cxx-4.8.a
macx:LIBS += /Users/joe/Documents/cloak_deps/db-4.8.30.NC/build_unix/libdb_cxx-4.8.a
LIBS += $$join(BOOST_LIB_PATH,,-L,) $$join(OPENSSL_LIB_PATH,,-L,) $$join(QRENCODE_LIB_PATH,,-L,)
#LIBS += -lssl -lcrypto
!macx:LIBS += $$join(BDB_LIB_PATH,,-L,) -ldb_cxx$$BDB_LIB_SUFFIX
!win32:LIBS += -levent -lz
#LIBS += -lz
# -lgdi32 has to happen after -lcrypto (see  #681)
windows:LIBS += -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32 
LIBS += -L/usr/local/lib/ -lboost_system$$BOOST_LIB_SUFFIX -lboost_filesystem$$BOOST_LIB_SUFFIX -lboost_program_options$$BOOST_LIB_SUFFIX -lboost_thread$$BOOST_THREAD_LIB_SUFFIX
windows:LIBS += -lboost_chrono$$BOOST_LIB_SUFFIX

contains(RELEASE, 1) {
    !windows:!macx {
        # Linux: turn dynamic linking back on for c/c++ runtime libraries
        LIBS += -Wl,-Bdynamic
    }
}

message(LIBS = $$LIBS)

!windows:!macx {
    LIBS += -ldl -lcurl
}

system($$QMAKE_LRELEASE -silent $$_PRO_FILE_)

DISTFILES += \
    src/notes.txt

message(LIBS = $$LIBS)

