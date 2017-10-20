#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/CloakCoin.ico

convert ../../src/qt/res/icons/CloakCoin-16.png ../../src/qt/res/icons/CloakCoin-32.png ../../src/qt/res/icons/CloakCoin-48.png ${ICON_DST}
