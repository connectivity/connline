#!/bin/sh

rm -f aclocal.m4 compile configure
rm -rf autom4te.cache/
rm -rf config

find ./ -type f -name "config.h.in*" -delete;
find ./ -type f -name "Makefile.in" -delete;
find ./ -type f -name "*.xml" -delete;
find ./ -type f -name "*.gcno" -delete;
find ./ -type f -name "*.gcda" -delete;
find ./ -type f -name "*.gcov" -delete;
