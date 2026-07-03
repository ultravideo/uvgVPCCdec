#pragma once

#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#define BITSTREAM_DEBUG false

/* Size of data chunks */
#define UVG_DATA_CHUNK_SIZE 4096

/* A linked list of chunks of data, used for returning the encoded data */
typedef struct uvg_data_chunk {
    /* Buffer for the data */
    uint8_t data[UVG_DATA_CHUNK_SIZE];

    /* Number of bytes filled in this chunk */
    uint32_t len;

    /* Next chunk in the list */
    struct uvg_data_chunk *next;
} uvg_data_chunk;

/* A stream of bits */
typedef struct bitstream_t {
    /* Total number of complete bytes */
    uint32_t len = 0;

    /* Pointer to the first chunk, or NULL */
    uvg_data_chunk *first;

    /* Pointer to the last chunk, or NULL */
    uvg_data_chunk *last;

    /* The incomplete byte */
    //uint8_t* data;
    std::vector<uint8_t> data;

    /* Number of bits in the incomplete byte */
    uint8_t cur_bit = 0;
} bitstream_t;

// Check if the byte is aligned in the bitstream
bool bitsream_byte_aligned(bitstream_t* stream);

// Move n-bits around the bitstream from the chosen position 
void bitstream_advance(bitstream_t* stream, size_t bits);

/* Read rbsp_trailing_bits syntax element, which aligns the bitstream */
void bitstream_align_rbsp_trailing_bits(bitstream_t* stream);

// Align the bitstream for reading bitstream
void bitstream_align(bitstream_t* stream);

uint32_t bitstream_read_no_advance(bitstream_t* stream, uint8_t bits);

size_t bitstream_read_size_from_poiter(const uint8_t* src, size_t len);

// Read n-bits from the chosen position from the bitstream
uint32_t bitstream_read(bitstream_t* stream, uint8_t bits);

// Read ue from the stream
uint32_t bitstream_read_ue(bitstream_t* stream);

// Read se from the stream
int32_t bitstream_read_se(bitstream_t* stream);

// Copy bytes to dest
void bitstream_copy_bytes(uint8_t* dest, const uint8_t* bytes, const size_t len);

bool moreRbspData(bitstream_t* stream);

/* In debug mode print out some extra info */
// #if BITSTREAM_DEBUG
// /* Counter to keep up with bits read */
// #define READ_U(bitstream_data, bits, pos, name)                                    
//     {                                                                             
//         uint32_t value = bitstream_read(bitstream_data, bits, pos);                
//         printf("%-50s u(%u) : %d : %zu\n", name, bits, value, (size_t)pos.bytes); 
//         return value;                                                             
//     }
// #define READ_UE(bitstream_data, pos, name)                       
//     {                                                           
//         uint32_t value = bitstream_read_ue(bitstream_data, pos); 
//         printf("%-50s ue(v): %d\n", name, value);               
//         return value;                                           
//     }
// #else
// #define READ_U(bitstream_data, bits, pos, name)                     
//     {                                                              
//         uint32_t value = bitstream_read(bitstream_data, bits, pos); 
//         return value;                                              
//     }
// #define READ_UE(bitstream_data, pos, name)                       
//     {                                                           
//         uint32_t value = bitstream_read_ue(bitstream_data, pos); 
//         return value;
//     }
// #endif

uint32_t readU(bitstream_t* stream, uint8_t bits, std::string name, size_t gofId);
uint32_t readUE(bitstream_t* stream, std::string name, size_t gofId);