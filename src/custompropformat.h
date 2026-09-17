#ifndef GUD_CUSTOMPROPFORMAT_H
#define GUD_CUSTOMPROPFORMAT_H

/* Project/ROM contract. No host pointers or platform types in the file.
 * IDs are stable and separate from the original prop enumeration. */
#define CUSTOM_PROP_BASE 512
#define CUSTOM_PROP_CAPACITY 128
#define CUSTOM_PROP_MAGIC 0x474e5031u /* GNP1 */
#define CUSTOM_PROP_HEADER_SIZE 16u
#define CUSTOM_PROP_ENTRY_SIZE 96u
#define CUSTOM_PROP_CONFIG_VERSION 1u
#define CUSTOM_PROP_MANIFEST_KIND 0x4e505250u /* NPRP */
#define CUSTOM_PROP_DATA_KIND 0x4e504d44u /* NPMD */
/* Header: magic, count, entry size, ID base (four big-endian words).
 * Entry: name[64], bank-relative data offset, length, f32 radius, data hash,
 * f32 placement scale, then 12 reserved zero bytes. All data is 16-byte aligned.
 * Native models have one switch, one matrix, and no texture-header entries;
 * texture references are ordinary C0 commands in their display lists. */

#endif
