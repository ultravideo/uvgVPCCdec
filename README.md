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
