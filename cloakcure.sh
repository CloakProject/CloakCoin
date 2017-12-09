#!/bin/bash
chmod -R +w CloakCoin-Qt.app/Contents/Frameworks

install_name_tool -id @executable_path/../Frameworks/libcurl.4.dylib CloakCoin-Qt.app/Contents/Frameworks/libcurl.4.dylib
install_name_tool -id @executable_path/../Frameworks/libevent-2.0.5.dylib CloakCoin-Qt.app/Contents/Frameworks/libevent-2.0.5.dylib

install_name_tool -id @executable_path/../Frameworks/libminiupnpc.10.dylib CloakCoin-Qt.app/Contents/Frameworks/libminiupnpc.10.dylib
install_name_tool -id @executable_path/../Frameworks/libssl.1.0.0.dylib CloakCoin-Qt.app/Contents/Frameworks/libssl.1.0.0.dylib
install_name_tool -id @executable_path/../Frameworks/libcrypto.1.0.0.dylib CloakCoin-Qt.app/Contents/Frameworks/libcrypto.1.0.0.dylib
install_name_tool -id @executable_path/../Frameworks/libdb_cxx-4.8.dylib CloakCoin-Qt.app/Contents/Frameworks/libdb_cxx-4.8.dylib
install_name_tool -id @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/Frameworks/libboost_system-mt.dylib
install_name_tool -id @executable_path/../Frameworks/libboost_filesystem-mt.dylib CloakCoin-Qt.app/Contents/Frameworks/libboost_filesystem-mt.dylib
install_name_tool -id @executable_path/../Frameworks/libboost_program_options-mt.dylib CloakCoin-Qt.app/Contents/Frameworks/libboost_program_options-mt.dylib
install_name_tool -id @executable_path/../Frameworks/libboost_thread-mt.dylib CloakCoin-Qt.app/Contents/Frameworks/libboost_thread-mt.dylib

install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change QtGui.framework/Versions/4/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/4/QtGui CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change QtCore.framework/Versions/4/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/4/QtCore CloakCoin-Qt.app/Contents/Frameworks/QtGui.framework/Versions/4/QtGui
install_name_tool -change QtNetwork.framework/Versions/4/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/4/QtNetwork CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change QtWebKit.framework/Versions/4/QtWebKit @executable_path/../Frameworks/QtWebKit.framework/Versions/4/QtWebKit CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt

install_name_tool -change /opt/local/lib/libminiupnpc.10.dylib @executable_path/../Frameworks/libminiupnpc.10.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libssl.1.0.0.dylib @executable_path/../Frameworks/libssl.1.0.0.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libcrypto.1.0.0.dylib @executable_path/../Frameworks/libcrypto.1.0.0.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libcrypto.1.0.0.dylib @executable_path/../Frameworks/libcrypto.1.0.0.dylib CloakCoin-Qt.app/Contents/Frameworks/libssl.1.0.0.dylib

install_name_tool -change /opt/local/lib/db48/libdb_cxx-4.8.dylib @executable_path/../Frameworks/libdb_cxx-4.8.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_system-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_filesystem-mt.dylib @executable_path/../Frameworks/libboost_filesystem-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_program_options-mt.dylib @executable_path/../Frameworks/libboost_program_options-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_thread-mt.dylib @executable_path/../Frameworks/libboost_thread-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_chrono-mt.dylib @executable_path/../Frameworks/libboost_thread-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt

install_name_tool -change /opt/local/lib/libboost_atomic-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_chrono-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_context-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_coroutine-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_date_time-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_filesystem-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_graph-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_iostreams-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_locale-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_log-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_log_setup-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_math_c99-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_math_c99f-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_math_c99l-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_math_tr1-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_math_tr1f-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_math_tr1l-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_prg_exec_monitor-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_program_options-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_python-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_random-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_regex-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_serialization-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_signals-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_system-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_thread-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_timer-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_unit_test_framework-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_wave-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
install_name_tool -change /opt/local/lib/libboost_wserialization-mt.dylib @executable_path/../Frameworks/libboost_system-mt.dylib CloakCoin-Qt.app/Contents/MacOs/CloakCoin-Qt
