#!/bin/sh
echo Copying $SRC1 as $DST1
cp "$SRC1" "$DST1" || sleep 4
echo Copying $SRC2 as $DST2
cp "$SRC2" "$DST2" || sleep 4
sleep 1
