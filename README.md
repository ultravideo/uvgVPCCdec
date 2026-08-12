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
---

Currently Underconstruction:
  Sample cmd: uvgvpccdec/_build/Release/src/app/uvgVPCCdec -i /home/nhan/nhan/uvgvpccdec_WORKSPACE/bitstreams/longdress_vox10_tmc2_geo_smoothing.vpcc -o decoded_plys/decoded_%03d_%04d.ply -t 0 --TMC2Upscaling=true --fastColorConversion=false

## Using uvgVPCCdec

Run
```
./_build/Release/src/app/uvgVPCCdec -i <input-filename.vpcc>
```

Supported parameters:

```
Required:
  -i, --input <filename>                 : Input bitstream file

Optional:
  -o, --output <filename>                : Output ply file path (file name in the form of
                                           e.g. 'name_%04d.ply')
  -t, --threads <num>                    : Maximum number of threads to be used (default: 20)
      --TMC2Upscaling <bool>             : Enable TMC2 upscaling (default: false)
      --fastColorConversion <bool>       : Enable fast color conversion from YUV to RGB (default: false)
      --keepIntermediateFiles <bool>     : Keep intermediate files (default: false)
      --uvgvpccParametersString <string> : uvgVPCC decoder parameters string

Other options:
      --help                             : Print this help message
      --version                          : Print version information
```


## Dependencies

The following steps can help resolve build issues (especially with Venctester):

```
sudo apt install libzmq3-dev
```

Somewhere on the computer:
```
git clone https://github.com/zeromq/cppzmq.git
cd cppzmq
cmake -B build -DCPPZMQ_BUILD_TESTS=OFF
sudo cmake --build build --target install
```
