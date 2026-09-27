// Raizes para api.ticktick.com e ticktick.com (spec 6.5). Varias raizes para
// sobreviver a rotacao de CA:
//   USERTrust RSA Certification Authority — ancora atual (Sectigo); expira 2038-01-18
//   Sectigo Public Server Authentication Root R46 — a mesma cadeia sem o cross-sign; 2046-03-21
//   ISRG Root X1 (Let's Encrypt) — alvo comum de migracao; 2035-06-04
// Conferido em 2026-09-25 com: openssl s_client -connect api.ticktick.com:443 -showcerts
#ifndef NET_CERTS_H
#define NET_CERTS_H

extern const char CA_BUNDLE[];

#endif // NET_CERTS_H
