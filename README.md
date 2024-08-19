# uvgVPCC decoder

## Compilation

Install libavcodec
```
sudo apt install libavcodec-dev
```
Build with
```
cmake -B _build/Release -S . -DBUILD_SHARED_LIBS=ON -DBUILD_TYPE=RelWithDebInfo && cmake --build ./_build/Release --parallel
```
Run
```
./_build/Release/src/app/uvgVPCCdec <input-filename.vpcc> 0
```
Second parameter states whether or not you want the outputted frames written to the disk (1) or not (0).

Number of threads is set in code in `uvgvpccdec.cpp`.
