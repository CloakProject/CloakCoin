
# CloakCoin

![CLOAK LOGO](/src/qt/res/icons/CloakCoin.png)

This is the official CloakCoin Repository. If you want more info about Cloakcoin, please visit the [official website](https://www.cloakcoin.com)

## Coin Specification

| Specification |     Value     |
| ------------- | ------------- |
| Consensus Algorithm | Proof-of-Stake |
|    Rewards    |    6% p.a.    |
|  Block Time   |    60 sec     |
| Circulating Supply |   5.5m   |

## Social Networks & channels

To contact us, or talk about CloakCoin, use your preferred social media:

| Site				|     Link		|
| ------------- | ------------ |
| Twitter			| [CloakCoin](https://www.twitter.com/CloakCoin) |
| Facebook			| [CloakCoinOfficial](https://www.facebook.com/CloakCoinOfficial/) |
| Telegram			|	[cloakproject](https://t.me/cloakproject) |
| Rocket Chat			| [Official Chat](https://chat.cloakcoin.com) |

## Installation
All the installation instructions are for Windows / Linux / MacOS / RaspberryPi an android on the Cloak Wiki : [Install Cloakcoin Wallet](https://www.cloakcoin.com/knowledge-base/get_started/compilation)

## Compiling GUI CloakCoin wallet from source

#### Disclaimer

**We advise you that compiling from source gives you a bleeding edge version of the CloakCoin wallet. This means there might be bugs, crashes, and you could lose your Cloaks. Therefore, to avoid any issue, you should never run a development wallet on the same computer as the stable wallet which holds your Cloaks.**

By default, development wallet runs on TestNet, but you can never be too safe.

### Dependencies

To install the dependencies, you need to use your system package manager:
- Windows: you will need to download/compile the proper DLLs needed, depending on your system gen & version; this is very hard to get right (libcurl...), not recommended for beginners. 
- Ubuntu: use `sudo apt-get install *package*`. To install all at once, including build tools (not listed in the chart): `sudo apt-get install git make build-essential g++ qttools5-dev-tools qt5-default libboost-all-dev libleveldb-dev libcurl4-openssl-dev libssl-dev libevent-dev libminiupnpc libqrencode-dev`
- Mac OS X: install [HomeBrew](https://brew.sh/), and use `brew install *package*`. To install all at once: `brew install qt boost@1.57 leveldb curl openssl libevent berkeley-db@4 miniupnpc`


| Dep            | Min. version   | Ubuntu pkg             | HomeBrew pkg    | Optional | Purpose        |
| -------------- | -------------- | ---------------------- | --------------- | -------- | -------------  |
| Qt             | 5.5.1          | `qt5-default`          | `qt`            | NO       | GUI            |
| Boost          | 1.57/1.58*     | `libboost-all-dev`     | `boost@1.57`    | NO       | C++ libraries  |
| OpenSSL        | 1.0.2g         | `libssl-dev`           | `openssl`       | NO       | ha256 sum      |
| Curl           | 7.16**         | `libcurl4-openssl-dev` | `curl`          | NO       | Requests       |
| libevent       | 2.0.21         | `libevent-dev `        | `libevent`      | NO       | Events         |
| LevelDB        | 1.2            | `libleveldb-dev`       | `leveldb`       | NO       | Database       |
| Berkeley DB    | 4.8*           |  install from source   | `berkeley-db@4` | NO       | Database       |
| qrencode       | 3.4.4          | `libqrencode-dev`      | `qrencode`      | YES      | QR Codes       |
| libminiupnpc   | 1.9.20140911***| `libminiupnpc`         | `miniupnpc`     | YES      | NAT punching   |
| Doxygen        | any            | `doxygen`              | `doxygen`       | YES      | Documentation  |

_\* Those specific version are known to work. Latest versions are known not to work._

_\*\* Debian and Ubuntu refer to version< <= 7.15 as libcurl3, and versions >= 7.16 as libcurl4, because of an API change. You need libcurl4._

_\*\*\* This version is very specific, as 1.9 seems to have some non-compatible changes. 2.0 probably won't work (test needed). **WARNING** This is included for historic reasons, but you should not use it, as it is a known security hole. It's disabled by default._

### Compiling

You can either use the command line or import the .pro file in Qt Creator.

Using the command line, run:
```qmake && make```

Alternative Linux build tutorial can be found [here](https://gist.github.com/brannondorsey/1153ec2f50d1c88c9f028a3c9ced7b8d). Thanks [Brannon](https://github.com/brannondorsey)

## License

Distributed under the [MIT software license](http://www.opensource.org/licenses/mit-license.php).

