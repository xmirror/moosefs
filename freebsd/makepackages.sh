#!/usr/bin/env bash

PORTNAMES="master chunkserver client metalogger cgi cgiserv cli netdump"

cd /usr/ports/sysutils/moosefs-common
make ; make package

for portname in ${PORTNAMES}; do
	cd /usr/ports/sysutils/moosefs-ce-${portname}
	make ; make package
done

PORTNAMESREV=`echo $PORTNAMES | awk '{ for (i=NF; i>0 ; i--) { printf "%s%s",$i,(i>1)?" ":""; }}'`

for portname in ${PORTNAMESREV}; do
	cd /usr/ports/sysutils/moosefs-ce-${portname}
	make deinstall
done

cd /usr/ports/sysutils/moosefs-common
make deinstall
