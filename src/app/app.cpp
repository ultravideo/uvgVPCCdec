#include "uvgvpccdec/uvgvpccdec.hpp"
#include <uvgrtp/lib.hh>

#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>

void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream);
bool write_to_file_ = false;
bool fast_yuv_to_rgb_ = false;

constexpr char LOCAL_IP[] = "127.0.0.1";

// This example runs for 10 seconds
constexpr auto RECEIVE_TIME_S = std::chrono::seconds(10);

// Hooks for the media streams
void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame); // VPS only included for simplicity of demonstration
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame);

/* These values specify the amount of NAL units inside each type of V3C unit. These need to be known to be able to reconstruct the 
 * GOFs after receiving. These default values correspond to the provided test sequence, and may be different for other sequences.
 * The sending example has prints that show how many NAL units each V3C unit contain. For other sequences, these values can be
 * modified accordingly. */
constexpr int VPS_NALS = 1; // VPS only included for simplicity of demonstration
constexpr int AD_NALS = 35;
constexpr int OVD_NALS = 35;
constexpr int GVD_NALS = 131;
constexpr int AVD_NALS = 131;

struct nal_info {
    size_t location   = 0; // Start position of the NAL unit
    size_t size       = 0; // Size of the NAL unit
    char* buf = nullptr;     // Used on receiving end for temporary storage of the received NAL unit

};

struct v3c_streams {
    uvgrtp::media_stream* vps = nullptr;
    uvgrtp::media_stream* ad = nullptr;
    uvgrtp::media_stream* ovd = nullptr;
    uvgrtp::media_stream* gvd = nullptr;
    uvgrtp::media_stream* avd = nullptr;
};
struct v3c_unit_header {
    uint8_t vuh_unit_type = 0;
};

/* A v3c_unit_info contains all the required information of a V3C unit
 - nal_info struct holds the format(Atlas, H264, H265, H266), start position and size of the NAL unit
 - With this info you can send the data via different uvgRTP media streams. */
struct v3c_unit_info {
    v3c_unit_header header;
    std::vector<nal_info> nal_infos = {};
    //char* buf; // (used on the receiving end)
    uint64_t ptr = 0; // (used on the receiving end) total size of the received NAL units in a V3C unit
    bool ready = false; // (used on the receiving end)
};

struct v3c_file_map {
    std::vector<v3c_unit_info> vps_units = {};
    std::vector<v3c_unit_info> ad_units = {};
    std::vector<v3c_unit_info> ovd_units = {};
    std::vector<v3c_unit_info> gvd_units = {};
    std::vector<v3c_unit_info> avd_units = {};
    std::vector<v3c_unit_info> pvd_units = {};
    std::vector<v3c_unit_info> cad_units = {};
};

v3c_streams init_v3c_streams(uvgrtp::session* sess, uint16_t src_port, uint16_t dst_port, int flags, bool rec);
v3c_file_map init_mmap();
void copy_rtp_payload(std::vector<v3c_unit_info>* units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame);


/* NOTE: In case where the last GOF has fewer NAL units than specified above, the receiver does not know how many to expect
   and cannot reconstruct that specific GOF. s*/

/* ------------------------ ripped from tmc2------------------------ */
bool write( const std::string fileName, point_cloud_frame* frame, const bool asAscii = true );
void exportPointCloud(const std::string fileName, point_cloud_frame* frame);

enum PCCEndianness { PCC_BIG_ENDIAN = 0, PCC_LITTLE_ENDIAN = 1 };
static inline PCCEndianness PCCSystemEndianness() {
  uint32_t num = 1;
  return ( *( reinterpret_cast<char*>( &num ) ) == 1 ) ? PCC_LITTLE_ENDIAN : PCC_BIG_ENDIAN;
}

size_t read_value(const uint8_t* src, size_t len) {
    size_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value |= static_cast<size_t>(src[i]) << (8 * (len - 1 - i));
    }
    return value;
}

bool output_func(uvgvpcc_dec::API::decoded_output* output);

int main(int argc, char* argv[]) {

    if (argc != 4) {
        std::cout << "invalid number of arguments, enter .vpcc filename, 1/0 (file writing), 1/0 (fast color conversion) " << std::endl;
    }
    //std::string input_file = "longdress-f1.vpcc";
    std::string input_file = argv[1];
    if (*argv[2] == '1') {
        write_to_file_  = true;
    }

    /* Initialize uvgRTP context and session*/
    uvgrtp::context ctx;
    std::pair<std::string, std::string> addresses_receiver(LOCAL_IP, LOCAL_IP);
    uvgrtp::session* sess = ctx.create_session(addresses_receiver);
    int flags = 0;

    // Create the uvgRTP media streams with the correct RTP format
    v3c_streams streams = init_v3c_streams(sess, 8890, 8892, flags, true);

    // Initialize memory map
    v3c_file_map mmap = init_mmap();

    streams.vps->install_receive_hook(&mmap.vps_units, vps_receive_hook);
    streams.ad->install_receive_hook(&mmap.ad_units, ad_receive_hook);
    streams.ovd->install_receive_hook(&mmap.ovd_units, ovd_receive_hook);
    streams.gvd->install_receive_hook(&mmap.gvd_units, gvd_receive_hook);
    streams.avd->install_receive_hook(&mmap.avd_units, avd_receive_hook);
    streams.avd->configure_ctx(RCC_RING_BUFFER_SIZE, 40 * 1000 * 1000);

    uvgvpcc_dec::Logger::setLogLevel(uvgvpcc_dec::LogLevel::INFO);
    uvgvpcc_dec::Parameters param;

    if (*argv[3] == '1') {
        param.fast_color_conversion  = true;
    }
    param.keep_intermediate_files = false;

    //param.max_points = 738590;
    uvgvpcc_dec::API::initializeDecoder(param);

    std::cout << "Waiting incoming packets for " << RECEIVE_TIME_S.count() << " s" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    uvgvpcc_dec::API::v3c_unit_stream unit_stream;
    //readFile(input_file, unit_stream);
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Application", "Number of V3C chunks " + std::to_string(unit_stream.v3c_chunks.size()) + " \n");
    
    uvgvpcc_dec::API::decoded_output output; // Each point cloud frame gets appended to the output as they are decoded
    std::thread file_writer_thread;
    file_writer_thread = std::thread(output_func, &output);
    
    for (size_t i = 0; i < unit_stream.v3c_chunks.size(); ++i) {
        /*const*/ auto& chunk = unit_stream.v3c_chunks.front();
        uvgvpcc_dec::API::decodeV3CChunk(chunk, &output);
        unit_stream.v3c_chunks.pop();
    }
    uvgvpcc_dec::API::emptyFrameQueue();
    output.io_mutex.lock();
    std::shared_ptr<point_cloud_frame> empty = std::make_shared<point_cloud_frame>();
    output.frames.push(empty); // Push empty point cloud frame to signal end of data
    output.io_mutex.unlock();
    output.available_frames.release();
    file_writer_thread.join();
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Done \n");
    return EXIT_SUCCESS;
}

void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "Opening file " + filename + " \n");
    std::ifstream input_file (filename);

    if(!input_file.is_open()) {
        throw std::runtime_error("File reading : Could not open input file " + filename);
    }
    uint8_t header_byte = 0;
    input_file.read(reinterpret_cast<char*> (&header_byte), sizeof(uint8_t));
    size_t v3c_unit_size_precision = (header_byte >> 5) + 1;

    size_t data_read = 1; // hdr byte already read
    unit_stream.v3c_unit_size_precision_bytes = v3c_unit_size_precision;
    
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "V3C unit size precision " + std::to_string(v3c_unit_size_precision) + " \n");
    uvgvpcc_dec::v3c_chunk latest_chunk;
    while (input_file.peek() != EOF) {
        
        uint8_t v3c_size_array[v3c_unit_size_precision];
        input_file.read(reinterpret_cast<char*>(v3c_size_array), v3c_unit_size_precision);
        data_read += input_file.gcount();

        size_t v3c_unit_size = read_value(v3c_size_array, v3c_unit_size_precision);;
        latest_chunk.v3c_unit_sizes.push_back(v3c_unit_size);
        size_t old_size = latest_chunk.data.size();
        latest_chunk.data.resize(old_size + v3c_unit_size);
        input_file.read(reinterpret_cast<char*>(&latest_chunk.data[old_size]), v3c_unit_size);
        data_read += input_file.gcount();
        
    }
    unit_stream.v3c_chunks.push(std::make_shared<uvgvpcc_dec::v3c_chunk>(latest_chunk));
    input_file.close();
}

bool output_func(uvgvpcc_dec::API::decoded_output* output)
{
    while (true) {
        output->available_frames.acquire();
        output->io_mutex.lock();
        std::shared_ptr<point_cloud_frame> frame = output->frames.front();

        if(frame->getPointCount() == 0) {
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "APPLICATION", "Empty frame: All frames written.\n");
            break;
        }
        if(write_to_file_ && frame->getPointCount() != 0) {
            std::string out_name = "output-test-gof" + std::to_string(frame->gof_index) + "-f" + std::to_string(frame->frame_index_in_gof) + ".ply";
            write(out_name, frame.get(), true);
        }

        output->frames.pop();   
        output->io_mutex.unlock();
    }
    return true;
}

void exportPointCloud(const std::string fileName, point_cloud_frame* frame) {

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Adaptation", "Write to file " + fileName + " \n");

    std::ofstream fout(fileName, std::ofstream::out | std::ofstream::trunc);
    if (!fout.is_open()) {
        throw std::runtime_error("Error : can't create a stream from : " + fileName);
    }
    fout << "ply";
    fout << "\nformat ascii 1.0";
    fout << "\nelement vertex " << frame->getPointCount();
    fout << "\nproperty int x";
    fout << "\nproperty int y";
    fout << "\nproperty int z";

    fout << "\nproperty uchar red";
    fout << "\nproperty uchar green";
    fout << "\nproperty uchar blue";
    fout << "\nend_header\n";

    fout << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (size_t i = 0; i < frame->getPointCount(); ++i) {
        point3d& position = frame->positions.at(i);
        fout << position.x() << " " << position.y() << " " << position.z();
        const uvg_color& color = frame->colors.at(i);
        fout << " " << static_cast<int>(color.data_[0]) << " " << static_cast<int>(color.data_[1]) << " "
             << static_cast<int>(color.data_[2]);
        fout << std::endl;
    }
    fout.close();
}

/* ------------------------ ripped from tmc2------------------------ */
bool write( const std::string fileName, point_cloud_frame* frame, const bool asAscii ) {

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Adaptation", "Write to file " + fileName + " \n");
    std::ofstream fout( fileName, std::ofstream::out );
    if ( !fout.is_open() ) { return false; }
    const size_t pointCount = frame->getPointCount();
    bool x = asAscii;
    fout << "ply" << std::endl;

    if ( asAscii ) {
        fout << "format ascii 1.0" << std::endl;
    } else {
        PCCEndianness endianess = PCCSystemEndianness();
        if ( endianess == PCC_BIG_ENDIAN ) {
        fout << "format binary_big_endian 1.0" << std::endl;
        } else {
        fout << "format binary_little_endian 1.0" << std::endl;
        }
    }
    fout << "element vertex " << pointCount << std::endl;
    if ( asAscii ) {
        fout << "property float x" << std::endl;
        fout << "property float y" << std::endl;
        fout << "property float z" << std::endl;
    } else {
        fout << "property float x" << std::endl;
        fout << "property float y" << std::endl;
        fout << "property float z" << std::endl;
    }
    if ( !frame->colors.empty() ) {
        fout << "property uchar red" << std::endl;
        fout << "property uchar green" << std::endl;
        fout << "property uchar blue" << std::endl;
    }
    fout << "element face 0" << std::endl;
    fout << "property list uint8 int32 vertex_index" << std::endl;
    fout << "end_header" << std::endl;
    if ( asAscii ) {
        fout << std::setprecision( std::numeric_limits<double>::max_digits10 );
        for ( size_t i = 0; i < pointCount; ++i ) {
            point3d& position = frame->positions.at(i);
            //const PCCPoint3D& position = ( *this )[i];
            fout << position.x() << " " << position.y() << " " << position.z();

            if ( !frame->colors.empty() ) {
                const uvg_color& color = frame->colors.at(i);
                fout << " " << static_cast<int>( color.data_[0] ) << " " << static_cast<int>( color.data_[1] ) << " "
                    << static_cast<int>( color.data_[2] );
            }

            fout << std::endl;
        }
    } else {
        fout.clear();
        fout.close();
        fout.open( fileName, std::ofstream::binary | std::ofstream::out | std::ofstream::app );
        for ( size_t i = 0; i < pointCount; ++i ) {
            point3d& position = frame->positions.at(i);
            //const PCCPoint3D& position = ( *this )[i];
            // fout.write( reinterpret_cast<const char* const>( &position ), sizeof( PCCType ) * 3 );
            float value[3];
            value[0] = position.data_[0];
            value[1] = position.data_[1];
            value[2] = position.data_[2];
            fout.write( reinterpret_cast<const char*>( &value ), sizeof( float ) * 3 );
            if ( !frame->colors.empty() ) {
                const uvg_color& color = frame->colors.at(i);
                fout.write( reinterpret_cast<const char*>( &color.data_ ), sizeof( uint8_t ) * 3 );
            }
        }
    }
    fout.close();
    return true;
}

v3c_streams init_v3c_streams(uvgrtp::session* sess, uint16_t src_port, uint16_t dst_port, int flags, bool rec)
{
    flags |= RCE_NO_H26X_PREPEND_SC;
    v3c_streams streams = {};

    streams.vps = sess->create_stream(src_port, dst_port, RTP_FORMAT_GENERIC, flags);
    streams.ad = sess->create_stream(src_port, dst_port, RTP_FORMAT_ATLAS, flags);
    streams.ovd = sess->create_stream(src_port, dst_port, RTP_FORMAT_H265, flags);
    streams.gvd = sess->create_stream(src_port, dst_port, RTP_FORMAT_H265, flags);
    streams.avd = sess->create_stream(src_port, dst_port, RTP_FORMAT_H265, flags);
    
    if (rec) {
        streams.vps->configure_ctx(RCC_REMOTE_SSRC, 1);
        streams.ad->configure_ctx(RCC_REMOTE_SSRC, 2);
        streams.ovd->configure_ctx(RCC_REMOTE_SSRC, 3);
        streams.gvd->configure_ctx(RCC_REMOTE_SSRC, 4);
        streams.avd->configure_ctx(RCC_REMOTE_SSRC, 5);
    }
    else {
        streams.vps->configure_ctx(RCC_SSRC, 1);
        streams.ad->configure_ctx(RCC_SSRC, 2);
        streams.ovd->configure_ctx(RCC_SSRC, 3);
        streams.gvd->configure_ctx(RCC_SSRC, 4);
        streams.avd->configure_ctx(RCC_SSRC, 5);
    }
    //streams.gvd->configure_ctx(RCC_FPS_NUMERATOR, 10);
    return streams;
}

v3c_file_map init_mmap()
{
    v3c_file_map mmap = {};

    v3c_unit_header hdr = { V3C_AD };
    v3c_unit_info unit = { hdr, {}, 0, false };
    mmap.ad_units.push_back(unit);

    hdr = { V3C_OVD };
    unit = { hdr, {}, 0, false };
    mmap.ovd_units.push_back(unit);

    hdr = { V3C_GVD };
    unit = { hdr, {}, 0, false };
    mmap.gvd_units.push_back(unit);

    hdr = { V3C_AVD };
    unit = { hdr, {}, 0, false };
    mmap.avd_units.push_back(unit);
    return mmap;
}

void vps_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;

    char* cbuf = new char[frame->payload_len];
    memcpy(cbuf, frame->payload, frame->payload_len);
    v3c_unit_info vps;
    nal_info aa = {0, frame->payload_len, cbuf};
    vps.nal_infos.push_back({0, frame->payload_len, cbuf});
    vec->push_back(vps);
    std::cout << "rec vps" << std::endl;
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ad_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    std::cout << "rec atlas nal, size " << frame->payload_len << std::endl;
    copy_rtp_payload(vec, AD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void ovd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    std::cout << "rec OVD nal, size " << frame->payload_len << std::endl;
    copy_rtp_payload(vec, OVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void gvd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    std::cout << "rec GVD nal, size " << frame->payload_len << std::endl;
    copy_rtp_payload(vec, GVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}
void avd_receive_hook(void* arg, uvgrtp::frame::rtp_frame* frame)
{
    std::vector<v3c_unit_info>* vec = (std::vector<v3c_unit_info>*)arg;
    std::cout << "rec AVD nal, size " << frame->payload_len << std::endl;
    copy_rtp_payload(vec, AVD_NALS, frame);
    (void)uvgrtp::frame::dealloc_frame(frame);
}

void copy_rtp_payload(std::vector<v3c_unit_info>* units, uint64_t max_size, uvgrtp::frame::rtp_frame* frame)
{
    uint32_t seq = frame->header.seq;
    if (units->back().nal_infos.size() == max_size) {
        v3c_unit_header hdr = { units->back().header.vuh_unit_type};
        v3c_unit_info info = { hdr, {}, 0, false };
        /*switch (units->back().header.vuh_unit_type) {
            case V3C_AD: {
                info.header.ad = { (uint8_t)units->size(), 0};
                break;
            }
            case V3C_OVD: {
                info.header.ovd = { (uint8_t)units->size(), 0 };
                break;
            }
            case V3C_GVD: {
                info.header.gvd = { (uint8_t)units->size(), 0, 0, 0 };
                break;
            }
            case V3C_AVD: {
                info.header.avd = { (uint8_t)units->size(), 0 };
                break;
            }
        }*/
        units->push_back(info);
    }

    if (units->back().nal_infos.size() <= max_size) {
        char* cbuf = new char[frame->payload_len];
        memcpy(cbuf, frame->payload, frame->payload_len);
        nal_info nalu = { units->back().ptr, frame->payload_len, cbuf};
        units->back().nal_infos.push_back(nalu);
        units->back().ptr += frame->payload_len;
    }
    if (units->back().nal_infos.size() == max_size) {
        units->back().ready = true;
    }
}