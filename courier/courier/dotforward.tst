#!/bin/sh

HOME=.
export HOME
DTLINE="Delivered-To: sam@example.com"
export DTLINE
rm -f .forward
./dotforward
echo "Exit code: $?"
echo "john, john@another.example.com" >.forward
echo "| /bin/true" >>.forward
./dotforward <<EOF
Subject: test

test
EOF
echo "Exit code: $?"
./dotforward <<EOF
Delivered-To: john@example.com
Subject: test

test
EOF
echo "Exit code: $?"
echo "sam, john, john@another.example.com" >.forward
echo "| /bin/true" >>.forward
./dotforward <<EOF
Subject: test

test
EOF
echo "Exit code: $?"
echo "\sam, \john, john@another.example.com" >.forward
echo "| /bin/true" >>.forward
./dotforward <<EOF
Subject: test

test
EOF
echo "Exit code: $?"

echo '\john, "|/usr/bin/foo"' >.forward
./dotforward <<EOF
Subject: test

test
EOF
echo "Exit code: $?"
rm -f .forward
