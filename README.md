# CloakCoin (CLOAK)

CloakCoin is a cool new crypto currency that will feature a uniquely implemented anonymization feature that uses exchanges on the back end and a decoupled transaction flow architecture.

CloakCoin uses the Enigma system in order to facilitate private/secure transactions. See the [explanation](ENIGMA.md) or the [Whitepaper](https://www.cloakcoin.com/resources/CloakCoin_ENIGMA_Whitepaper_v1.0.pdf) for more details.

## Branches explanation

- `2.1.0` branch currently contains legacy dev code [beta 1 - testnet only
- `anorak_master` branch contains windows targeted source
- `anorak_linux` branch contains (yes, you guessed it!) linux targeted source
- `master branch` is currently empty as we want to save it for a future unified source branch, to cover win/linux/mac platforms in one codebase


## Compiling GUI CloakCoin wallet from source

#### Step-by-step instructions

If you are new, please follow the detailed instructions
* for Windows (soon)
* for Linux (soon)
* [for Mac OS X](doc/build-qt-osx.md)

### Dependencies

To install the dependencies, you need to use your system package manager:
- Windows: ???
- Debian/Ubuntu: use `apt-get install *package*`
- Mac OS X: install [HomeBrew](https://brew.sh/), and use `brew install *package*`. To install all at once: `brew install qt boost@1.57 leveldb curl openssl libevent berkeley-db@4 miniupnpc`

**TODO: all Debian/Ubuntu packages must be checked.**

| Dep            | Min. version   | Debian/Ubuntu pkg  | HomeBrew pkg    | Optional | Purpose        |
| -------------- | -------------- | ------------------ | --------------- | -------- | -------------  |
| Qt             | 5.5.1          | ???                | `qt`            | NO       | GUI            |
| Boost          | 1.57*          | `libboost-all-dev` | `boost@1.57`    | NO       | C++ libraries  |
| OpenSSL        | 1.0.2g         | `libssl-dev`       | `openssl`       | NO       | ha256 sum      |
| Curl           | any            | `curl`             | `curl`          | NO       | Requests       |
| libevent       | 2.0.21         | `libevent`         | `libevent`      | NO       | Events         |
| LevelDB        | 1.2            | `leveldb`          | `leveldb`       | NO       | Database       |
| Berkeley DB    | 4.8*           | `berkeley-db@4`    | `berkeley-db@4` | NO       | Database       |
| qrencode       | 3.4.4          | `qrencode`         | `qrencode`      | YES      | QR Codes       |
| libminiupnpc   | 1.9.20140911** | `libminiupnpc-dev` | `miniupnpc`     | YES      | NAT punching   |
| Doxygen        | any            | `doxygen`          | `doxygen`       | YES      | Documentation  |

\* You need this specific version. Newer version won't work
\*\* This version is very specific, as 1.9 seems to have some non-compatible changes. 2.0 probably won't work (test needed)

### Compiling

You can either use the command line or import the .pro file in Qt Creator.

Using the command line, run:
```qmake && make```

### Troubleshooting

Please see the step-by-step instructions for your OS. Every one contains a **Troubleshooting** section explaining what you should do if you encounter a problem, and a **Known issues** section.

## Configuration

This wallet supports the `staking=0` option in the `CloakCoin.conf` file to disable the stake miner thread for pool and exchange operators.

