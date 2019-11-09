echo "Building miniupnpc ..."; cd miniupnpc; make; cd ..;

echo "Building LevelDB ..."; cd leveldb; make; cd ..;

echo "Building cloakcoind ..."; make -f makefile.unix;

echo "Copying dist files ..."
mkdir ../dist/; mkdir ../dist/unix
cp -f cloakcoind ../dist/unix/cloakcoind

echo "Striping binaries ..."
strip ../dist/unix/cloakcoind

read -p "Done! Press any key."
