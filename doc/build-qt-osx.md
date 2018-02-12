# Instructions to build GUI CloakCoin wallet on Mac OS X

## Introduction

This is a tutorial to build the CloakCoin wallet with a graphical user interface (GUI), intended for end-users.

It is designed to walk you through all the steps required to build the wallet from scratch.

If you are experienced, there are short instructions in the README.

You will need about 3Go of free space, unless you need to install Xcode, in which case you will need somewhere around 10Go, depending on the Xcode version.

It should take less than 1 hour to complete this guide, provided you have a good connection and good computer (just copy-pasting the commands on fiber connection and recent computer could be completed in under 20 minutes).


## Install a package manager

To build the wallet, you first need to install a package manager, which will let you install dependencies easily.

On Mac OS X, there is no pre-installed package manager, but there are several you can install.

The one CloakCoin use is HomeBrew. 

First, **go on [HomeBrew main page](https://brew.sh/) and follow the instructions to install it.**


## Installing the dependencies

To install dependencies, you need to open the Terminal app and run commands.

The command to install dependencies is `brew install` followed by a package name.

When you do that, HomeBrew will download the pre-compiled package and installed it in a pre-determined folder.

*Warning*: if HomeBrew doesn't have the pre-compiled package for your OS version (generally if you have an older version of Mac OS X), it will need to compile it. To do that, it will require that you **install Xcode**. To do that, go over to  [from Apple Developer](http://developer.apple.com/download/more/) and find a version of Xcode compatible with your OS version.

You can install dependencies one-by-one, or all at once.
To install all at once, **run: `brew install qt boost@1.57 leveldb curl openssl libevent berkeley-db@4 miniupnpc qrencode`**

Otherwise, here is the detail:
* Qt is required for the graphical interface: `brew install qt`
* Boost 1.57 is required for various utilities, and that specific version is required (newer won't work): `brew install boost@1.57`
* LevelDB is required to save some data: `brew install leveldb`
* Berkeley DB 4.8 is required to save some other data, and that specific version is required (newer won't work): `brew install berkeley-db@4`
* Curl is required to communicate: `brew install curl`
* OpenSSL is required to secure communications: `brew install openssl`
* Libevent is required to handle events internally : `brew install libevent`
* Miniupnpc can be used to communicate using UPnP protocol: `brew install miniupnpc`
* Qrencode can be used to provide QR codes: `brew install qrencode`


## Expose qmake

To compile the code, you will need your Terminal to have access to the `qmake` command, which was installed with Qt.
To do that, **run the command `echo 'export PATH="/usr/local/opt/qt/bin:$PATH"' >> ~/.bash_profile`**, then restart your terminal.

## Download the code

You can either [download a .zip of the code](https://github.com/CloakProject/Cloak2Public/archive/master.zip) or **clone the repository using `git` (which is pre-installed on all Macs): `git clone https://github.com/CloakProject/Cloak2Public.git`**.

## Compiling the wallet

In the Terminal, go where you downloaded the code using the `cd` command. For example, if you cloned the repository, **run `cd Cloak2Public`.**

Then, **run `qmake && make`**, and wait until it finishes (it will take a few minutes and display a lot of things, be patient).

**Congratulations**, you now have the CloakCoin wallet you built yourself. Be advised that by downloading from the repo, you got a bleeding edge version, that may contain bugs.
It is advised not to use it to store your real CloakCoin. By default, this version will be on TestNet anyway.

## Compiling using Qt Creator

There is an alternate way to build the wallet instead of running `qmake && make`: you can use Qt Creator. Download it, import the `.pro` file, then build it.

## Package dependencies inside the app

To be able to run the app on another Mac which doesn't have all the dependencies installed, you need to package dependencies inside the app.

There is a convenient tool named `macdeployqt` which does exactly that. Just **run `macdeployqt cloakcoin-qt.app`** 

## Creating a .dmg to distribute the app

Users expect a .dmg where they can just drag&drop the app to their Applications folder. There are several ways to do this.
You can use [create-dmg](https://github.com/andreyvit/create-dmg). **Clone it and make create-dmg accessible in your `$PATH`, then run:
```
mkdir cloakcoin-release
mv cloakcoin-qt.app cloakcoin-release/CloakCoin.app
create-dmg --volname "CloakCoin" --volicon "src/qt/res/icons/CloakCoin.icns" --background "contrib/macdeploy/background.png" --window-pos 200 120 --window-size 500 320 --icon-size 100 --icon CloakCoin.app 120 150 --hide-extension CloakCoin.app --app-drop-link 380 150 CloakCoin.dmg cloakcoin-release/ 
```**

## Troubleshooting

If you have an issue with this guide, please open issue with:
* your Mac OS X version
* the result of `brew list`

We'll try to help you as best as we can.

## Known issues

#### qmake: command not found

Ensure qmake is in your $PATH. On Mac, using HomeBrew, `echo 'export PATH="/usr/local/opt/qt/bin:$PATH"' >> ~/.bash_profile` and restart your terminal.

#### Qt Creator

If you see `symbol not found` errors, ensure your DYLD_FRAMEWORK_PATH and DYLD_LIBRARY_PATH are correct in `Projects` -> `Run` -> `Run environment`. 

