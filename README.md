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
./_build/Release/src/app/uvgVPCCdec <input-filename.vpcc> 0 0
```
1. First parameter filename
2. Second parameter 1/0 for writing to file yes/no
3. Third parameter 1/0 for fast YUV->RGB color conversion yes/no

Number of threads is set in code in `uvgvpccdec.cpp`.
