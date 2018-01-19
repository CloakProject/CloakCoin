# CloakCoin (CLOAK)

CloakCoin is a cool new crypto currency that will feature a uniquely implemented anonymization feature that uses exchanges on the back end and a decoupled transaction flow architecture.

## Branches explanation

- `2.1.0` branch currently contains legacy dev code [beta 1 - testnet only
- `anorak_master` branch contains windows targeted source
- `anorak_linux` branch contains (yes, you guessed it!) linux targeted source
- `master branch` is currently empty as we want to save it for a future unified source branch, to cover win/linux/mac platforms in one codebase


## Compiling CloakCoin from source

### Dependencies

To install the depencies, you need to use your system package manager:
- Windows: ???
- Debian/Ubuntu: use `apt-get install *package*`
- Mac OS X: install [HomeBrew](https://brew.sh/), and use `brew install *package*`

**TODO: all Debian/Ubuntu packages must be checked.**

| Dep            | Min. version  | Debian/Ubuntu pkg  | HomeBrew pkg    | Optional | Purpose        |
| -------------- | ------------- | ------------------ | --------------- | -------- | -------------  |
| Qt             | 5.5.1         | ???                | `qt`            | NO       | GUI            |
| Boost          | 1.57          | `libboost-all-dev` | `boost`         | NO       | C++ libraries  |
| OpenSSL        | 1.0.2g        | `libssl-dev`       | `openssl`       | NO       | ha256 sum      |
| Curl           | any           | `curl`             | `curl`          | NO       | Requests       |
| libpng         | 1.6.16        | `libpng`           | `libpng`        | NO       | Images (PNG)   |
| libevent       | 2.0.21        | `libevent`         | `libevent`      | NO       | Requests       |
| LevelDB        | 1.2           | `leveldb`          | `leveldb`       | NO       | Database       |
| Berkeley DB    | 1.2           | `berkeley-db@4`    | `berkeley-db@4` | YES*     | Database       |
| qrencode       | 3.4.4         | `qrencode`         | `qrencode`      | YES      | QR Codes       |
| libminiupnpc   | 1.9           | `libminiupnpc-dev` | `miniupnpc`     | YES      | NAT punching   |
| Doxygen        | any           | `doxygen`          | `doxygen`       | YES      | Documentation  |

\* You need either LevelDB or Berkeley DB. Berkeley DB support might be removed in the future.

### Compiling

You can either use the command line or import the .pro file in Qt Creator.

Using the command line, run:
```qmake && make```

### Troubleshooting

#### Qt Creator

* If you see `symbol not found` errors, ensure your DYLD_FRAMEWORK_PATH and DYLD_LIBRARY_PATH are correct in `Projects` -> `Run` -> `Run environment`. 


## Configuration

This wallet supports the `staking=0` option in the `CloakCoin.conf` file to disable the stake miner thread for pool and exchange operators.

