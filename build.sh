#!/usr/bin/env bash

juniper -s ListExt.jun SignalExt.jun NeoPixel.jun TEA.jun -o main.cpp && cat main.cpp
