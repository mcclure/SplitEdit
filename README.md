This is a GUI editor app for the data in LiveSplit (.LSS) files. It is not quite finished.

You will find downloadable versions for Mac and Windows under the "Releases" button in the GitHub header.

# Using

Use the menu to open a .LSS file. It will list the basic file information, the times recorded for your PB and "best splits", and then all of your runs. Edit any field then save. **This is an early beta so I recommend backing up your .LSS before saving**.

If any of your runs did not finish, the table for that run will be missing rows at the end and there will be no final time listed. If any of your runs are missing split data (this happens if you renamed or reordered splits after recording the run) the missing splits will be labeled as "-----" and certain editing features will be disabled.

## TODO for 1.0

* In the final version there's gonna be an "Automatic" checkbox next to the PB and Best Splits listing for continuously recalculating your PB and best splits from the other data
* Open/save starts at system root every time :/

## Known problems

* No undo
* Can't remove a run
* If a run has fewer splits than it should you can't fix this
* If a run has splits which are "missing", rather than skipped (this happens when you rename/reorder splits when you already have runs) it can't usefully edit tht run
* Rounds to microsecond, this is what you want for LiveSplit One but for LiveSplit classic millisecond would be better
* If your split names are very long the times will get clipped on the right side of the window
* Changing the "offset" field doesn't change times (should it??)
* Open/save doesn't filter for .lss files
* File-modified tracking might be wrong

# Building

First, run qmake:

    qmake

(Or, if you want to run in the debugger:)

    qmake CONFIG+=debug

Then run:

    make

(Or, on Windows:)

    nmake

# License

The Qt libraries have a LGPL license. The Qt example code this is based on has a BSD license. All new code in this repo is by <<andi.m.mcclure@gmail.com>> and is MIT licensed:

> Copyright 2019-2020 Andi McClure
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

