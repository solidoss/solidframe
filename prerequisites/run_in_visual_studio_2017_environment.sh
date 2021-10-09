#!/usr/bin/env bash
echo "*******************************************"
echo ""
echo "Usage:"
echo "$0 amd64|x86 bash"
echo ""
echo "*******************************************"

VSCMDPATH="`find c\:/Program\ Files\ \(x86\)/ -name vcvarsall.bat 2>/dev/null | grep -v -F "Permission" | grep -F "2017"`"
VSSETUP="`cygpath -w "$VSCMDPATH"`"
CMDPATH="`pwd`"
CMDSETUP="`cygpath -w "$CMDPATH"`"
CMDARG="$1"
shift
cmd.exe "/C "$VSSETUP" $CMDARG & cd $CMDSETUP & $@"
