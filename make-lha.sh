#!/bin/bash

set -eux

ARCHIVE="umc.lha"

cd ..
rm -f "${ARCHIVE}"
cp umc/Readme umc.readme
cp umc/.folder_info umc.info

rm -f umc.lha

lha -ao5 ${ARCHIVE} \
	umc/umc \
	umc/umc.c \
	umc/README \
	umc/README.info \
	umc/COPYING \
	umc/Makefile \
	umc.info

echo ""
echo ""

lha l ${ARCHIVE}
