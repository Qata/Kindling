#!/usr/bin/env bash

juniper -s ListExt.jun NeoPixel.jun TEA.jun -o main.cpp && cat main.cpp
