
import os,time,sys
import subprocess,glob
from concurrent.futures import ThreadPoolExecutor, as_completed

def comparePly(index, plyTMC2, plyUVGdec):
    cmdA = f"/home/lfreneau/Desktop/workspace/pccsum/build/src/app/appPccsum {plyTMC2[index]}"
    cmdB = f"/home/lfreneau/Desktop/workspace/pccsum/build/src/app/appPccsum {plyUVGdec[index]}"

    md5A = subprocess.run(cmdA, capture_output=True, text=True, shell=True).stdout.split(" ")[-1][:-1]
    md5B = subprocess.run(cmdB, capture_output=True, text=True, shell=True).stdout.split(" ")[-1][:-1]
    
    if md5A == md5B:
        return f"Frame {index} validated."
    else:
        return f"!!! WRONG MD5 => Frame {index} !!!"


# TMC2_DIR="/home/lfreneau/Desktop/workspace/runctctmc2/mpeg-pcc-tmc2"
TMC2_DIR="/home/lfreneau/Desktop/workspace/dev_workspace/uvgVPCC/_utils/tmc2"
TMC2_DECODER_EXE=f"{TMC2_DIR}/bin/PccAppDecoder"
# TMC2_COMMON=f"\
#   --colorSpaceConversionPath=YUV420toYUV444uvgVPCCSimple \
#   --inverseColorSpaceConversionConfig={TMC2_DIR}/cfg/hdrconvert/yuv420toyuv444_16bit.cfg \
#   --nbThread=20 \
#   --colorTransform=0 \
#   --computeMetrics=0 \
#   --keepIntermediateFiles=1 \
#   --computeChecksum=0 "

TMC2_COMMON=f"\
  --colorSpaceConversionPath= \
  --inverseColorSpaceConversionConfig={TMC2_DIR}/cfg/hdrconvert/yuv420toyuv444_16bit.cfg \
  --nbThread=20 \
  --colorTransform=0 \
  --computeMetrics=0 \
  --keepIntermediateFiles=1 \
  --computeChecksum=0 "


def build():
    print("### Command lines : build ###")

    cmd = f"cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=1 --preset=default && cmake --build --preset=default"
    if os.system(cmd): exit()
    os.system("cp -f _build/compile_commands.json .")




def decodeTMC2(bitstream):
    print("### Command lines : decode TMC2 ###")

    if not os.path.exists(bitstream):
        print("This bitstream file does not exist:",bitstream)
        exit()

    baseName = bitstream.split("/")[-1].replace(".vpcc","").replace(".bin","")
    outputFile = f"./_out/TMC2/TMC2_{baseName}_%04d.ply"
    logFile = outputFile.replace(r"_%04d.ply",".log")
    cmd = f"{TMC2_DECODER_EXE} {TMC2_COMMON} "
    cmd += f"--compressedStreamPath={bitstream} "
    cmd += f"--reconstructedDataPath={outputFile} "
    cmd += f"--resolution=1023 > {logFile} "  # Resolution ight change in the future
    
    os.system("rm -f _out/TMC2/*  |:")

    if os.system(cmd): exit()

def visualize():
    print("### Command lines : visualize uvgVPCCdec point-clouds ###")

    if(len(glob.glob("./_out/uvgVPCCdec/*.ply"))==0):
        print("No ply to visualize.")
        return

    cmd = r"bash -c \""
    cmd = f"/home/lfreneau/Desktop/workspace/dev_workspace/uvgVPCC/_utils/mpeg-pcc-renderer/bin/linux/Release/PccAppRenderer"
    cmd += " -d ./_out/uvgVPCCdec/ -n 0 --backgroundColor='20 20 85' --rotate=2 --play=1 " # > /dev/null
    cmd += r"\""
    
    os.system(f"{cmd} &")
    
def comparePC():
    print("### Command lines : compare pc ###")

    plyTMC2 = sorted(glob.glob("./_out/TMC2/*.ply"))
    plyUVGdec = sorted(glob.glob("./_out/uvgVPCCdec/*.ply"))
    with ThreadPoolExecutor() as executor:
        futures = [executor.submit(comparePly, i, plyTMC2, plyUVGdec) for i in range(len(plyUVGdec))]
        for future in as_completed(futures):
            print(future.result())





def main():
    if os.getcwd() != "/home/lfreneau/uvgvpccdec":
        print("You are not in '/home/lfreneau/uvgvpccdec'. This is dangerous.")
        exit()

    if len(sys.argv)==1:
        print("WARNING !!! This python script is dangerous and it should be used with so much cautious that you should never use it without the approval of Louis Fréneau.")

    bitstream = sys.argv[-1]
    if "b" in sys.argv[1:]: build()
    if "r" in sys.argv[1:]: run(bitstream)
    if "d" in sys.argv[1:]: decodeTMC2(bitstream)
    if "v" in sys.argv[1:]: visualize()
    if "m" in sys.argv[1:]: comparePC()














def run(bitstream):
    print("### Command lines : run ###")

    if not os.path.exists(bitstream):
        print("This bitstream file does not exist:",bitstream)
        exit()

    baseName = bitstream.split("/")[-1].replace(".vpcc","").replace(".bin","")
    outputFile = f"./_out/uvgVPCCdec/uvgVPCCdec_{baseName}_%04d.ply"
    logFile = outputFile.replace(r"_%04d.ply",".log")

    os.system("rm -f _out/uvgVPCCdec/*  |:")

    ####################################
    ######## uvgVPCCdec command ########
    ####################################
    
    
    cmd = f"_build/src/app/uvgVPCCdec -i {bitstream} -o {outputFile} --TMC2Upscalling=true -t 1 > {logFile}"


    ####################################
    ####################################
    ####################################


    if os.system(cmd): exit()







if __name__ == "__main__": main()