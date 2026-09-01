#!/bin/bash

if ! command -v md5sum &> /dev/null
then
    echo "md5sum could not be found"
    exit 1
fi

if ! command -v dd &> /dev/null
then
    echo "dd could not be found"
    exit 1
fi

if ! command -v dd &> /dev/null
then
    echo "gzip could not be found"
    exit 1
fi


ROM_FILENAME=
OUT_FILENAME=
ROM_MD5=

MD5_US="70c525880240c1e838b8b1be35666c3b"


# US
ROM_FILENAME="baserom.u.z64"
OUT_FILENAME="baserom.u.cdata"
if [ -f "${ROM_FILENAME}" ]; then
    ROM_MD5=$(md5sum "${ROM_FILENAME}" | cut -d " " -f1)
    
    if [ "${ROM_MD5}" = "${MD5_US}" ]; then
        echo "extracting US compressed data segment"
        dd bs=1 skip=137616 count=71760 if="${ROM_FILENAME}" of="${OUT_FILENAME}" status=none
        GZ=gzip tools/1172inflate.sh "${OUT_FILENAME}" "${OUT_FILENAME}.bin"
        rm "${OUT_FILENAME}"
    else
        echo "cannot extract compressed data segment from ${ROM_FILENAME}, md5=${ROM_MD5}, expected ${MD5_US}"
    fi
else
    echo "${ROM_FILENAME} not found"
fi
