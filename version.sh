#!/usr/bin/env sh

git describe --always | sed 's|^v||;s|\([^-]*-g\)|r\1|;s|-|.|g'
