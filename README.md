# Filesystem Stalker
### Build 
1. Clone project and open in android studio.
2. Run assemble/build gradle task to create the shared libraries and executables.
### Setup
1. Push both fs-stalker and libstalker.so(or future libs per platform), to /data/local/tmp(or any where you want).
2. 'chmod +x' them, if needed.
* Both tasks can be done using a 3rd party app calling the StalkerManager class.
### Run fs-stalker in one of the following ways:
1. fs-stalker [OPTIONS] <dir_path_1> <dir_path_2> <dir_path_3> - will watch each one of the directorys specified.
2. fs-stalker [OPTIONS] <root_dir_path> - will watch every subfolder found under the specific root directory.
* OPTIONS: -o <output_file>
