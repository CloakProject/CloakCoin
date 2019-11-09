echo "Building LevelDB ..."; cd leveldb; make; cd ..;

echo "Building cloakcoind ..."; make -f makefile.osx;

echo "Copying dist files ..."
mkdir ../dist/; mkdir ../dist/osx
cp -f /usr/local/opt/berkeley-db@4/lib/libdb_cxx-4.8.dylib ../dist/osx/libdb_cxx-4.8.dylib
cp -f /usr/local/opt/boost@1.57/lib/libboost_system-mt.dylib ../dist/osx/libboost_system-mt.dylib
cp -f /usr/local/opt/boost@1.57/lib/libboost_filesystem-mt.dylib ../dist/osx/libboost_filesystem-mt.dylib
cp -f /usr/local/opt/boost@1.57/lib/libboost_program_options-mt.dylib ../dist/osx/libboost_program_options-mt.dylib
cp -f /usr/local/opt/boost@1.57/lib/libboost_thread-mt.dylib ../dist/osx/libboost_thread-mt.dylib
cp -f /usr/local/opt/openssl/lib/libssl.1.0.0.dylib ../dist/osx/libssl.1.0.0.dylib
cp -f /usr/local/opt/openssl/lib/libcrypto.1.0.0.dylib ../dist/osx/libcrypto.1.0.0.dylib
cp -f /usr/local/opt/miniupnpc/lib/libminiupnpc.17.dylib ../dist/osx/libminiupnpc.17.dylib
cp -f cloakcoind ../dist/osx/cloakcoind

echo "Patching dist files ..."
install_name_tool -change /usr/local/opt/berkeley-db@4/lib/libdb_cxx-4.8.dylib @loader_path/libdb_cxx-4.8.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/boost@1.57/lib/libboost_system-mt.dylib @loader_path/libboost_system-mt.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/boost@1.57/lib/libboost_filesystem-mt.dylib @loader_path/libboost_filesystem-mt.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/boost@1.57/lib/libboost_program_options-mt.dylib @loader_path/libboost_program_options-mt.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/boost@1.57/lib/libboost_thread-mt.dylib @loader_path/libboost_thread-mt.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/openssl/lib/libssl.1.0.0.dylib @loader_path/libssl.1.0.0.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/openssl/lib/libcrypto.1.0.0.dylib @loader_path/libcrypto.1.0.0.dylib ../dist/osx/cloakcoind
install_name_tool -change /usr/local/opt/miniupnpc/lib/libminiupnpc.17.dylib @loader_path/libminiupnpc.17.dylib ../dist/osx/cloakcoind
sudo install_name_tool -change /usr/local/Cellar/openssl/1.0.2t/lib/libcrypto.1.0.0.dylib @loader_path/libcrypto.1.0.0.dylib ../dist/osx/libssl.1.0.0.dylib

echo "Stripping binaries ..."
strip ../dist/osx/cloakcoind

read -p "Done! Press any key."
