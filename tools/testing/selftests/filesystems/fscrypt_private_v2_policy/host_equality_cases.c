// SPDX-License-Identifier: GPL-2.0-only
#include "production_excerpt.h"

static unsigned int cases, failures;

static union fscrypt_policy make_policy(u8 version, u8 mode)
{
	union fscrypt_policy p = {0};
	p.version = version;
	if (version == FSCRYPT_POLICY_V1) {
		p.v1.contents_encryption_mode = mode;
		p.v1.filenames_encryption_mode = FSCRYPT_MODE_AES_256_CTS;
		for (unsigned int i = 0; i < FSCRYPT_KEY_DESCRIPTOR_SIZE; ++i)
			p.v1.master_key_descriptor[i] = (u8)(i + 1);
	} else {
		p.v2.contents_encryption_mode = mode;
		p.v2.filenames_encryption_mode = FSCRYPT_MODE_AES_256_CTS;
		p.v2.flags = FSCRYPT_POLICY_FLAGS_PAD_16;
		for (unsigned int i = 0; i < FSCRYPT_KEY_IDENTIFIER_SIZE; ++i)
			p.v2.master_key_identifier[i] = (u8)(i + 1);
	}
	return p;
}

static void check(const char *name, const union fscrypt_policy *a,
		  const union fscrypt_policy *b, bool expected)
{
	bool actual = fscrypt_policies_equal(a, b);
	++cases;
	failures += actual != expected;
	printf("%s %u - %s expected=%d actual=%d\n",
	       actual == expected ? "ok" : "not ok", cases, name, expected, actual);
}

static void pair(const char *name, const union fscrypt_policy *a,
		 const union fscrypt_policy *b, bool expected)
{
	check(name, a, b, expected);
	check("reverse operand order", b, a, expected);
}

int main(void)
{
	_Static_assert(sizeof(struct fscrypt_policy_v1) == 12, "V1 ABI");
	_Static_assert(sizeof(struct fscrypt_policy_v2) == 24, "V2 ABI");
	_Static_assert(offsetof(struct fscrypt_policy_v2, master_key_identifier) == 8,
		       "V2 identifier offset");
	union fscrypt_policy v2 = make_policy(FSCRYPT_POLICY_V2, FSCRYPT_MODE_PRIVATE);
	union fscrypt_policy v1 = make_policy(FSCRYPT_POLICY_V1, FSCRYPT_MODE_PRIVATE);
	union fscrypt_policy changed;
	char name[80];
	puts("TAP version 13");
	pair("V2 PRIVATE identical", &v2, &v2, true);
	for (unsigned int i = 0; i < FSCRYPT_KEY_IDENTIFIER_SIZE; ++i) {
		changed = v2;
		changed.v2.master_key_identifier[i] ^= 1;
		snprintf(name, sizeof(name), "V2 identifier byte %u differs", i);
		pair(name, &v2, &changed, false);
	}
	for (unsigned int i = 0; i < 8; ++i) {
		changed = v2;
		changed.v2.flags ^= (u8)(1U << i);
		snprintf(name, sizeof(name), "V2 flags bit %u differs", i);
		pair(name, &v2, &changed, false);
	}
	changed = v2;
	changed.v2.contents_encryption_mode = FSCRYPT_MODE_AES_256_XTS;
	pair("V2 contents mode differs", &v2, &changed, false);
	changed = v2;
	changed.v2.filenames_encryption_mode = FSCRYPT_MODE_AES_128_CTS;
	pair("V2 filenames mode differs", &v2, &changed, false);
	for (unsigned int i = 0; i < sizeof(v2.v2.__reserved); ++i) {
		changed = v2;
		changed.v2.__reserved[i] = 1;
		pair("V2 reserved byte differs", &v2, &changed, false);
	}
	pair("V1 PRIVATE identical", &v1, &v1, true);
	changed = v1;
	changed.v1.flags ^= FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32;
	pair("V1 PRIVATE IV32-only ignored", &v1, &changed, true);
	for (unsigned int i = 0; i < 8; ++i) {
		u8 bit = (u8)(1U << i);
		if (bit == FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32)
			continue;
		changed = v1;
		changed.v1.flags ^= bit;
		pair("V1 PRIVATE other flag differs", &v1, &changed, false);
	}
	for (unsigned int i = 0; i < FSCRYPT_KEY_DESCRIPTOR_SIZE; ++i) {
		changed = v1;
		changed.v1.master_key_descriptor[i] ^= 1;
		pair("V1 PRIVATE descriptor differs", &v1, &changed, false);
	}
	changed = v1;
	changed.v1.contents_encryption_mode = FSCRYPT_MODE_AES_256_XTS;
	pair("V1 contents mode differs", &v1, &changed, false);
	changed = v1;
	changed.v1.filenames_encryption_mode = FSCRYPT_MODE_AES_128_CTS;
	pair("V1 filenames mode differs", &v1, &changed, false);
	union fscrypt_policy ordinary = make_policy(FSCRYPT_POLICY_V1, FSCRYPT_MODE_AES_256_XTS);
	changed = ordinary;
	changed.v1.flags ^= FSCRYPT_POLICY_FLAG_IV_INO_LBLK_32;
	pair("V1 non-PRIVATE IV32 not ignored", &ordinary, &changed, false);
	pair("V1/V2 version mismatch", &v1, &v2, false);
	printf("1..%u\n# cases=%u failures=%u actual_production_function=fscrypt_policies_equal\n",
	       cases, cases, failures);
	return failures ? 1 : 0;
}
