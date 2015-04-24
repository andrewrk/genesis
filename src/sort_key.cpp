#include "sort_key.hpp"
#include "debug.hpp"

#include <string.h>

SortKey::SortKey() : SortKey(0) { }

SortKey::SortKey(int value) {
    assert(value >= 0 && value < 256);
    if (value == 0) {
        magnitude = 0;
    } else {
        magnitude = 1;
        ok_or_panic(digits.append(value));
    }
}

SortKey::SortKey(const SortKey &other) {
    *this = other;
}

SortKey& SortKey::operator= (const SortKey& other) {
    if (this != &other) {
        ok_or_panic(digits.resize(other.digits.length()));
        memcpy(digits.raw(), other.digits.raw(), digits.length());
        magnitude = other.magnitude;
    }
    return *this;
}

int SortKey::compare(const SortKey &a, const SortKey &b) {
    int mag_cmp = a.magnitude - b.magnitude;
    if (mag_cmp != 0)
        return mag_cmp;

    int cmp = memcmp(a.digits.raw(), b.digits.raw(), min(a.digits.length(), b.digits.length()));
    if (cmp != 0)
        return cmp;

    return a.digits.length() - b.digits.length();
}

SortKey SortKey::single(const SortKey *low, const SortKey *high) {
    if (low) {
        if (high) {
            return average(*low, *high);
        } else {
            return increment(*low);
        }
    } else {
        if (high) {
            return average(SortKey(0), *high);
        } else {
            return SortKey(1);
        }
    }
}

void SortKey::multi(List<SortKey> &out_sort_key_list,
        const SortKey *low, const SortKey *high, int count)
{
    panic("TODO");
}

void SortKey::truncate_fraction(SortKey &value) {
    value.digits.remove_range(value.magnitude, value.digits.length());
}

SortKey SortKey::increment(const SortKey &value) {
    SortKey without_fraction = value;
    truncate_fraction(without_fraction);
    return add(without_fraction, SortKey(1));
}

SortKey SortKey::average(const SortKey &low, const SortKey &high) {
    assert(compare(low, high) < 0);
    SortKey a_padded = low;
    SortKey b_padded = high;
    pad_to_equal_magnitude(a_padded, b_padded);
    int b_carry = 0;
    int max_digit_length = max(a_padded.digits.length(), b_padded.digits.length());
    for (int i = 0; i < max_digit_length || b_carry > 0; i += 1) {
        int a_value =            (i < a_padded.digits.length()) ? a_padded.digits.at(i) : 0;
        int b_value = b_carry + ((i < b_padded.digits.length()) ? b_padded.digits.at(i) : 0);
        if (a_value == b_value)
            continue;
        if (a_value == b_value - 1) {
            // we need more digits, but remember that b is ahead
            b_carry = 256;
            continue;
        }
        // we have a distance of at least 2 between the values.
        // half the distance floored is sure to be a positive single digit.
        SortKey half_distance;
        half_distance.magnitude = a_padded.magnitude;
        for (int j = 0; j < i; j += 1)
            ok_or_panic(half_distance.digits.append(0));
        int half_distance_value = (b_value - a_value) / 2;
        ok_or_panic(half_distance.digits.append(half_distance_value));
        // truncate insignificant digits of a
        if (i < a_padded.digits.length())
            a_padded.digits.remove_range(i, a_padded.digits.length());
        return add(a_padded, half_distance);
    }
    panic("unreachable");
}

SortKey SortKey::add(const SortKey &a, const SortKey &b) {
    SortKey a_padded = a;
    SortKey b_padded = b;
    pad_to_equal_magnitude(a_padded, b_padded);
    SortKey result;
    result.magnitude = a_padded.magnitude;
    int value = 0;
    List<uint8_t> result_digits;
    for (int i = max(a_padded.digits.length(), b_padded.digits.length()) - 1; i >= 0; i -= 1) {
        value += (i < a_padded.digits.length()) ? a_padded.digits.at(i) : 0;
        value += (i < b_padded.digits.length()) ? b_padded.digits.at(i) : 0;
        ok_or_panic(result_digits.append(value % 256));
        value /= 256;
    }
    // overflow up to more digits
    while (value > 0) {
        ok_or_panic(result_digits.append(value % 256));
        value /= 256;
        result.magnitude += 1;
    }
    for (int i = result_digits.length() - 1; i >= 0; i -= 1)
        ok_or_panic(result.digits.append(result_digits.at(i)));
    return result;
}

void SortKey::pad_to_equal_magnitude(SortKey &a, SortKey &b) {
    pad_in_place(a, b.magnitude);
    pad_in_place(b, a.magnitude);
}

void SortKey::pad_in_place(SortKey &sort_key, int magnitude) {
    int amount_to_add = magnitude - sort_key.magnitude;
    if (amount_to_add <= 0)
        return;

    ok_or_panic(sort_key.digits.insert_space(0, amount_to_add));
    for (int i = 0; i < amount_to_add; i += 1)
        sort_key.digits.at(i) = 0;
    sort_key.magnitude += amount_to_add;
}
