#!/bin/bash

set -e

aclocal
autoheader
automake -c --add-missing --force-missing
autoconf
