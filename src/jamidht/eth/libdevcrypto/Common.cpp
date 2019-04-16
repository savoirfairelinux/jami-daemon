/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Common.cpp
 * @author Alex Leverington <nessence@gmail.com>
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Common.h"
#include <secp256k1.h>
#include <libdevcore/SHA3.h>
#include <memory>
using namespace std;
using namespace dev;
using namespace dev::crypto;

namespace
{

secp256k1_context const* getCtx()
{
	static std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
		secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY),
		&secp256k1_context_destroy
	};
	return s_ctx.get();
}

}

bool dev::SignatureStruct::isValid() const noexcept
{
	static const h256 s_max{"0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"};
	static const h256 s_zero;

	return (v <= 1 && r > s_zero && s > s_zero && r < s_max && s < s_max);
}

Public dev::toPublic(Secret const& _secret)
{
	auto* ctx = getCtx();
	secp256k1_pubkey rawPubkey;
	// Creation will fail if the secret key is invalid.
	if (!secp256k1_ec_pubkey_create(ctx, &rawPubkey, _secret.data()))
		return {};
	std::array<byte, 65> serializedPubkey;
	size_t serializedPubkeySize = serializedPubkey.size();
	secp256k1_ec_pubkey_serialize(
			ctx, serializedPubkey.data(), &serializedPubkeySize,
			&rawPubkey, SECP256K1_EC_UNCOMPRESSED
	);
	assert(serializedPubkeySize == serializedPubkey.size());
	// Expect single byte header of value 0x04 -- uncompressed public key.
	assert(serializedPubkey[0] == 0x04);
	// Create the Public skipping the header.
	return Public{&serializedPubkey[1], Public::ConstructFromPointer};
}

Address dev::toAddress(Public const& _public)
{
	return right160(sha3(_public.ref()));
}

Address dev::toAddress(Secret const& _secret)
{
	return toAddress(toPublic(_secret));
}

KeyPair::KeyPair(Secret const& _sec):
	m_secret(_sec),
	m_public(toPublic(_sec))
{
	// Assign address only if the secret key is valid.
	if (m_public)
		m_address = toAddress(m_public);
}

KeyPair KeyPair::create()
{
	while (true)
	{
		KeyPair keyPair(Secret::random());
		if (keyPair.address())
			return keyPair;
	}
}

h256 crypto::kdf(Secret const& _priv, h256 const& _hash)
{
	// H(H(r||k)^h)
	h256 s;
	sha3mac(Secret::random().ref(), _priv.ref(), s.ref());
	s ^= _hash;
	sha3(s.ref(), s.ref());

	if (!s || !_hash || !_priv)
		throw InvalidState();
	return s;
}

Secret Nonce::next()
{
	std::lock_guard<std::mutex> l(x_value);
	if (!m_value)
	{
		m_value = Secret::random();
		if (!m_value)
			throw InvalidState();
	}
	m_value = sha3Secure(m_value.ref());
	return sha3(~m_value);
}
