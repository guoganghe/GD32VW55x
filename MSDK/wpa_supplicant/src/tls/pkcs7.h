/*
 * PKCS #7 (Cryptographic Message Syntax)
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef PKCS7_H
#define PKCS7_H

struct wpabuf *pkcs7_get_certificates(const struct wpabuf *pkcs7);

#endif /* PKCS7_H */
