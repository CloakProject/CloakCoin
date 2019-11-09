echo "Building miniupnpc ..."; cd miniupnpc; mingw32-make -f Makefile.mingw; cd ..;

echo "Building LevelDB ..."; cd leveldb; export TARGET_OS=OS_WINDOWS_CROSSCOMPILE; mingw32-make; cd ..;

echo "Building cloakcoind ..."; mingw32-make -f makefile.mingw;

echo "Copying dist files ..."
mkdir ../dist; mkdir ../dist/win
cp -f cloakcoind.exe ../dist/win/cloakcoind.exe

echo "Stripping binaries ..."
strip ../dist/win/cloakcoind.exe

read -p "Done! Press any key."
