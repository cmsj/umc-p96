#!/bin/bash

set -eux

ARCHIVE="umc.lha"
CURDIR="$(basename $(pwd))"

cd ..
rm -f "${ARCHIVE}"
cp "${CURDIR}/README" umc.readme
cp "${CURDIR}/.folder_info" umc.info

rm -f umc.lha

lha -ao5 ${ARCHIVE} \
	"${CURDIR}/umc" \
	"${CURDIR}/umc.c" \
	"${CURDIR}/README" \
	"${CURDIR}/README.info" \
	"${CURDIR}/COPYING" \
	"${CURDIR}/Makefile" \
	umc.info

echo ""
echo ""

lha l ${ARCHIVE}
