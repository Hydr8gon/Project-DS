# Project DS
Project DIVA for the DS!

# Overview
Project DS is an open-source implementation of the Japanese rhythm games *Hatsune Miku: Project DIVA Future Tone* and *Hatsune Miku: Project DIVA Mega Mix* for the Nintendo DS. It reads chart and music files extracted from these games and allows you to play them on the DS. It's currently in early stages, and not all features are implemented.

# Usage
DSC files should be provided in the `/project-ds/dsc` folder on your SD card. These can be dumped from a game, or custom ones can be used, as long as they are compatible with *Future Tone*. Song files in OGG format should be placed in `/project-ds/ogg`. The filenames must correspond; for example, `pv_001_extreme.dsc` corresponds to `pv_001.ogg`. Songs will be converted to PCM format the first time they are played, which takes a while. For song names and other information to be loaded, database files named `pv_db.txt`, `mdata_pv_db.txt`, or similar should be placed in `/project-ds`.

# Converter
Converting songs on the DS takes a long time, so a tool to batch convert them on a computer is also supplied. Once songs in OGG format are placed in `/project-ds/ogg`, run `converter` from the `/project-ds` directory. When everything is finished, you won't have to worry about insane conversion times anymore.

# Compiling
To build Project DS manually, you first need to install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and the `nds-dev` package. After that, you should be able to simply run `make` in the project root directory to compile. To compile the converter, run `make -f Makefile.conv` instead.

### References
* [Open PD Script Editor](https://notabug.org/thatrandomlurker/Open-PD-Script-Editor) - Opcode definitions and parameter counts
* [Project DIVA Arcade atwiki](https://w.atwiki.jp/projectdiva_ac/pages/128.html) - Detailed scoring information
