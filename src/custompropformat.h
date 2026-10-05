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
/* Config word 3: accept C/G names as categorized static model assets. */
#define CUSTOM_PROP_FEATURE_MODEL_CATEGORIES 1u
#define CUSTOM_PROP_FEATURE_CHARACTERS 2u
#define CUSTOM_CHARACTER_BASE 80
/* ChrRecord stores signed-byte body/head IDs. Keep 0x80..0xff reserved. */
#define CUSTOM_CHARACTER_LIMIT 128
#define CUSTOM_CHARACTER_BODY 1u
#define CUSTOM_CHARACTER_HEAD 2u
/* Header: magic, count, entry size, ID base (four big-endian words).
 * Entry: name[64], bank-relative data offset, length, f32 radius, data hash,
 * f32 placement scale, kind, stock template ID, character ID. All are big endian.
 * Kind 0 retains the static format (last two words zero). Kinds 1/2 are
 * animated bodies / attachable heads and require FEATURE_CHARACTERS. Their
 * IDs are contiguous from 80, independently of the bank/prop slot. They
 * inherit the template header, skeleton, scale, POV, sex and head policy.
 * Static models have one switch, one matrix, and no texture-header entries;
 * texture references are ordinary C0 commands in their display lists. */

#endif
