#pragma once


#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ==================== Error codes ====================

#define BEDROCK_SUCCESS 0
#define BEDROCK_ERR_OVERFLOW (-1)
#define BEDROCK_ERR_INVALID_DATA (-2)
#define BEDROCK_ERR_INVALID_ARG (-3)
#define BEDROCK_ERR_UNSUPPORTED (-4)
#define BEDROCK_ERR_NBT (-5)

// ==================== Error / utility ====================

const char *bedrock_last_error(void);
void bedrock_free(void *ptr);

// ==================== Stream lifecycle ====================

void *bedrock_stream_create(bool big_endian);
void *bedrock_stream_from_bytes(const uint8_t *data, size_t len, bool big_endian);
void bedrock_stream_destroy(void *stream);

// ==================== Stream write ====================

int bedrock_stream_write_bool(void *stream, bool value);
int bedrock_stream_write_u8(void *stream, uint8_t value);
int bedrock_stream_write_i16(void *stream, int16_t value);
int bedrock_stream_write_u16(void *stream, uint16_t value);
int bedrock_stream_write_i32(void *stream, int32_t value);
int bedrock_stream_write_u32(void *stream, uint32_t value);
int bedrock_stream_write_i32_be(void *stream, int32_t value);
int bedrock_stream_write_u32_be(void *stream, uint32_t value);
int bedrock_stream_write_u24(void *stream, uint32_t value);
int bedrock_stream_write_i64(void *stream, int64_t value);
int bedrock_stream_write_u64(void *stream, uint64_t value);
int bedrock_stream_write_f32(void *stream, float value);
int bedrock_stream_write_f64(void *stream, double value);
int bedrock_stream_write_varint(void *stream, int32_t value);
int bedrock_stream_write_varint64(void *stream, int64_t value);
int bedrock_stream_write_unsigned_varint(void *stream, uint32_t value);
int bedrock_stream_write_unsigned_varint64(void *stream, uint64_t value);
int bedrock_stream_write_normalized_f32(void *stream, float value);
int bedrock_stream_write_string(void *stream, const char *value);
int bedrock_stream_write_raw_bytes(void *stream, const uint8_t *data, size_t len);

// ==================== Stream read ====================

int bedrock_stream_read_bool(void *stream, bool *out);
int bedrock_stream_read_u8(void *stream, uint8_t *out);
int bedrock_stream_read_i16(void *stream, int16_t *out);
int bedrock_stream_read_u16(void *stream, uint16_t *out);
int bedrock_stream_read_i32(void *stream, int32_t *out);
int bedrock_stream_read_u32(void *stream, uint32_t *out);
int bedrock_stream_read_i32_be(void *stream, int32_t *out);
int bedrock_stream_read_u32_be(void *stream, uint32_t *out);
int bedrock_stream_read_u24(void *stream, uint32_t *out);
int bedrock_stream_read_i64(void *stream, int64_t *out);
int bedrock_stream_read_u64(void *stream, uint64_t *out);
int bedrock_stream_read_f32(void *stream, float *out);
int bedrock_stream_read_f64(void *stream, double *out);
int bedrock_stream_read_varint(void *stream, int32_t *out);
int bedrock_stream_read_varint64(void *stream, int64_t *out);
int bedrock_stream_read_unsigned_varint(void *stream, uint32_t *out);
int bedrock_stream_read_unsigned_varint64(void *stream, uint64_t *out);
int bedrock_stream_read_normalized_f32(void *stream, float *out);
int bedrock_stream_read_raw_bytes(void *stream, uint8_t *buffer, size_t len);
int bedrock_stream_read_string(void *stream, char *buffer, size_t *inout_len);

// ==================== Stream utility ====================

size_t bedrock_stream_size(const void *stream);
size_t bedrock_stream_position(const void *stream);
int bedrock_stream_set_position(void *stream, size_t pos);
int bedrock_stream_data(const void *stream, const uint8_t **out_data, size_t *out_len);

// ==================== NBT ====================

void *bedrock_nbt_create(void);
void bedrock_nbt_destroy(void *nbt);

int bedrock_nbt_set_string(void *nbt, const char *key, const char *value);
int bedrock_nbt_set_int(void *nbt, const char *key, int32_t value);
int bedrock_nbt_set_short(void *nbt, const char *key, int16_t value);
int bedrock_nbt_set_byte(void *nbt, const char *key, int8_t value);
int bedrock_nbt_set_tag(void *nbt, const char *key, void *child);

int bedrock_nbt_get_int(void *nbt, const char *key, int32_t *out);

int bedrock_nbt_list_append_string(void *nbt, const char *list_key, const char *value);
int bedrock_nbt_list_append_tag(void *nbt, const char *list_key, void *child);

int bedrock_nbt_to_network(void *nbt, uint8_t **out_data, size_t *out_len);
int bedrock_nbt_to_binary(void *nbt, uint8_t **out_data, size_t *out_len);
void *bedrock_nbt_to_snbt(void *nbt);

bool bedrock_nbt_empty(const void *nbt);
bool bedrock_nbt_contains(const void *nbt, const char *key);

int bedrock_nbt_from_network_into(void *nbt, const uint8_t *data, size_t len, size_t *consumed);
int bedrock_nbt_write_to_stream(void *nbt, void *stream);

// ==================== BlockPos ====================

void *bedrock_block_pos_create(int32_t x, int32_t y, int32_t z);
void bedrock_block_pos_destroy(void *pos);
int32_t bedrock_block_pos_get_x(const void *pos);
int32_t bedrock_block_pos_get_y(const void *pos);
int32_t bedrock_block_pos_get_z(const void *pos);

// ==================== Packet (UnimplementedPacket) ====================

void *bedrock_packet_create(int32_t packet_id);
void bedrock_packet_destroy(void *packet);
int bedrock_packet_serialize(void *packet, uint8_t **out_data, size_t *out_len);
int bedrock_packet_deserialize(void *packet, const uint8_t *data, size_t len);
int32_t bedrock_packet_get_id(const void *packet);
const char *bedrock_packet_get_name(const void *packet);

#ifdef __cplusplus
}
#endif
