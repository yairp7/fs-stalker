# Filesystem Stalker
### Build 
1. Clone project and open in android studio.
2. Run ndkBuild gradle task to create the shared libraries and executables.
### Setup
1. Push both fs-stalker and libandroid.so(or future libs per platform), to /data/local/tmp.
2. 'chmod +x' them, if needed.
### Run fs-stalker in one of the following ways:
1. fs-stalker <dir_path_1> <dir_path_2> <dir_path_3> - will watch each one of the directorys specified.
2. fs-stalker <root_dir_path> - will watch every subfolder found under the specific root directory.
