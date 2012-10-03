#!/bin/sh

./autogen.sh && \
    ./configure --enable-maintainer-mode \
		--enable-debug \
		--enable-connman \
		--enable-nm \
		--enable-wicd \
		--enable-test
