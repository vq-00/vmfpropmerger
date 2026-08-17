Need this tool in the same directory as .exe for model decompilation: https://github.com/UltraTechX/Crowbar-Command-Line
```
====================================================
 Reactive Drop VMF Prop Merger
====================================================

Usage:
 vmfpropmerger.exe [options]

Input:
 --vmf <path> VMF file
 --game <path> Reactive Drop game directory

Model filtering:
 --exclude-model <pattern> Exclude model (repeatable)
 --exclude-file <path> File containing excluded models/patterns
 --include-model <pattern> Only process matching models (repeatable)

Transforms:
 --coord-map <map> Coordinate conversion, default: -y,x,z
 Examples: -y,x,z x,y,z y,x,z
 --rotation Apply prop angles (default)
 --no-rotation Ignore prop angles
 --transform-normals Transform normals (default)
 --no-transform-normals Leave normals in source orientation
 --global-scale <number> Additional global scale
 --offset <x> <y> <z> Global XYZ offset

Output:
 --output-dir <path> Output model directory
 --model-name <name> Final model filename/path
 --targetname <name> Targetname for merged VMF entity
 --output-vmf <path> Output VMF path
 --max-triangles <number> Triangles per generated SMD body
 --max-convex-pieces <number> Maximum collision convex pieces
 --surfaceprop <name> QC surfaceprop
 --sequence-fps <number> QC sequence FPS

Tools:
 --crowbar <path> Crowbar executable
 --studiomdl <path> studiomdl.exe
 --no-vpk Do not search VPK archives

Work files:
 --work-dir <path> Work directory
 --keep-work Do not delete work directory

VMF behavior:
 --no-vmf Do not create a modified VMF
 --keep-original-props Keep successfully merged props
 --allow-failures Compile even when some props fail

Diagnostics:
 --dry-run Parse/filter only; no external tools
 --quiet Minimal output
 --verbose More diagnostic output
 --help Show this help
 --version Show version

Examples:
 vmfpropmerger.exe --vmf map.vmf --game "D:\\Games\\Reactive Drop\\reactivedrop"

 vmfpropmerger.exe --vmf map.vmf --game "D:\\Games\\Reactive Drop\\reactivedrop" \
 --exclude-model "props/*" \
 --exclude-model "models/characters/*" \
 --max-triangles 10000 \
 --keep-work
```
