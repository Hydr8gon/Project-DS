# Project DS
Project DIVA for the DS!

### Overview
Project DS is an open-source implementation of the Japanese rhythm games *Hatsune Miku: Project DIVA Future Tone* and
*Hatsune Miku: Project DIVA Mega Mix* for the Nintendo DS. It reads chart and music files extracted from these games and
allows you to play them on the DS. The UI is still in early stages, but gameplay should be more or less complete.

### Downloads
The latest build of Project DS is automatically provided via GitHub Actions, and can be downloaded from the
[releases page](https://github.com/Hydr8gon/Project-DS/releases).

### Usage
DSC and OGG files dumped from a compatible game should be placed in the `project-ds/dsc` and `project-ds/ogg` folders on
the root of your SD card. To load song names and other information, database files ending in `_db.txt` should be placed
in `project-ds/db`. A video guide with more detailed instructions can be found
[here](https://www.youtube.com/watch?v=ZQ4uYyCW7aA).

### Converter
OGG files need to be converted to PCM format the first time they're played, which takes a long time on the DS. To avoid
this, a tool is provided to convert them all at once on a computer. Once the OGG files are in place, you can run
`converter` in the `project-ds` directory to start the process.

### Contributing
This is a personal project, and I've decided to not review or accept pull requests for it. If you want to help, you can
test things and report issues or provide feedback. If you can afford it, you can also donate to motivate me and allow me
to spend more time on things like this. Nothing is mandatory, and I appreciate any interest in my projects, even if
you're just a user!

### Building
To build Project DS, you need to install [devkitPro](https://devkitpro.org/wiki/Getting_Started) and its `nds-dev`
package. With that set up, run `make -j$(nproc)` in the project root directory to start building. To build the
converter, run `make -f Makefile.conv -j$(nproc)` instead.

### Documentation
The `notes.txt` file in this repo documents my findings on the format of game files, as well as various mechanics. It
also has links to other resources that are useful for a project like this. Feel free to reference it if you're working
on something DIVA-related!

### Other Links
* [Hydra's Lair](https://hydr8gon.github.io) - Blog where I may or may not write about things
* [Discord Server](https://discord.gg/JbNz7y4) - A place to chat about my projects and stuff
