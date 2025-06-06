#!/bin/bash
uncrustify -c utilities/format.cfg --no-backup examples/C/*.c
uncrustify -c utilities/format.cfg --no-backup examples/C/*.h
uncrustify -c utilities/format.cfg --no-backup examples/Cpp/*.cpp
uncrustify -c utilities/format.cfg --no-backup examples/Cpp/*.hpp
uncrustify -c utilities/format.cfg --no-backup lib/C/*.c
uncrustify -c utilities/format.cfg --no-backup lib/C/*.h
uncrustify -c utilities/format.cfg --no-backup lib/Cpp/*.cpp
uncrustify -c utilities/format.cfg --no-backup lib/Cpp/*.hpp