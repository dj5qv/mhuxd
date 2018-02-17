#!/bin/bash

aclocal
autoheader
automake -c --add-missing
autoconf

