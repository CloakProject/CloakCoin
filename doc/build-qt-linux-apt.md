# Instructions to build GUI CloakCoin wallet on Linux

## Introduction

This is a tutorial to build the CloakCoin wallet with a graphical user interface (GUI), intended for end-users.

It is designed to walk you through all the steps required to build the wallet from scratch.

We assume you are using apt-get to install dependencies. If you use another package manager, you'll need to ensure you have the proper versions of every package (especially Boost)

If you are experienced, there are short instructions in the README.

It should take less than 1 hour to complete this guide, provided you have a good connection and good computer (just copy-pasting the commands on fiber connection and recent computer could be completed in under 20 minutes).


## Install Berkeley DB 4.8 from source

Unfortunately, Berkeley DB 4.8 isn't available from apt-get, and newer versions are not compatible.

Open a terminal and copy the commands to compile and install it from source.

* download it: `wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz`
* decompress it: `tar -xzvf db-4.8.30.NC.tar.gz`
* go in the build directory: `cd db-4.8.30.NC/build_unix`
* configure the build: `../dist/configure --enable-cxx`
* compile it: `make`
* install it in the libs: `sudo make install`
* here, check where it is installed. If it's not _/usr/local/BerkeleyDB.4.8/lib_, adapt the next command
* add that path to your libs: `echo "/usr/local/BerkeleyDB.4.8/lib" | sudo tee -a /etc/ld.so.conf.d/BerkeleyDB.4.8.conf`
* update your system to use those libs: `sudo ldconfig`


## Installing the dependencies

Again, we assume you have apt-get. If you use a different package manager, you will need to find the proper dependencies.

You can install dependencies one-by-one, or all at once.
To install all at once, **run: `sudo apt-get install git make build-essential g++ qttools5-dev-tools qt5-default libboost-all-dev libleveldb-dev libcurl4-openssl-dev libssl-dev libevent-dev libminiupnpc libqrencode-dev`**

Otherwise, here is the detail:
* git is useful to clone afterwards, but not an actual dependency: `sudo apt-get install git`
* development tools to compile: `sudo apt-get install make build-essential g++ qttools5-dev-tools`
* Qt is required for the graphical interface: `sudo apt-get install qt5-default`
* Boost is required for various utilities. Ensure it still installs Boost 1.58, newer versions are known not to work: `libboost-all-dev`
* LevelDB is required to save some data: `sudo apt-get install libleveldb-dev`
* Curl is required to communicate: `sudo apt-get install libcurl4-openssl-dev`
* OpenSSL is required to secure communications: `sudo apt-get install libssl-dev`
* Libevent is required to handle events internally : `sudo apt-get install libevent-dev`
* Miniupnpc can be used to communicate using UPnP protocol: `sudo apt-get install libminiupnpc`
* Qrencode can be used to provide QR codes: `sudo apt-get install libqrencode-dev`


## Download the code

You can either [download a .zip of the code](https://github.com/CloakProject/Cloak2Public/archive/master.zip) or **clone the repository using `git` (which is pre-installed on all Macs): `git clone https://github.com/CloakProject/Cloak2Public.git`**.


## Compiling the wallet

In the Terminal, go where you downloaded the code using the `cd` command. For example, if you cloned the repository, **run `cd Cloak2Public`.**

Then, **run `qmake && make`**, and wait until it finishes (it will take a few minutes and display a lot of things, be patient).

**Congratulations**, you now have the CloakCoin wallet you built yourself. Be advised that by downloading from the repo, you got a bleeding edge version, that may contain bugs.
It is advised not to use it to store your real CloakCoin. By default, this version will be on TestNet anyway.

## Troubleshooting

If you have an issue with this guide, please open issue with:
* your distribution and distribution version
* the result of `apt list --installed`

We'll try to help you as best as we can.

## Known issues

Nothing yet.