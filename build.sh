#!/usr/bin/env bash

juniper -s MaybeExt.jun ListExt.jun SignalExt.jun NeoPixel.jun TEA.jun -o main.cpp && cat main.cpp
