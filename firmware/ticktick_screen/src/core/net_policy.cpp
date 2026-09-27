#include "net_policy.h"

bool policy_should_renew(int64_t nowUtc, int64_t exp) {
    if (exp <= 0 || nowUtc <= 0) return false;
    return exp - nowUtc < RENEW_BEFORE_S;
}

int64_t policy_backoff(int64_t currentS, int64_t normalS) {
    const int64_t base = currentS > normalS ? currentS : normalS;
    int64_t next = base * 2;
    if (next > BACKOFF_MAX_S) next = BACKOFF_MAX_S;
    if (next < normalS) next = normalS;
    return next;
}
