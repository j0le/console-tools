#pragma once
#include <optional>
#include <cstdint>
#include <string>
#include <bit>

#include <Windows.h>

enum class result : bool { FAIL = false, SUCCESS = true };

constexpr std::string_view quote_open{ "\xC2\xBB" };  // >> U+00BB
constexpr std::string_view quote_close{ "\xC2\xAB" }; // << U+00AB

// ---------

#include <type_traits>
#include <limits>




template<class uint_type, std::size_t max_number_of_digits>
constexpr std::optional<uint_type> string_base10_to_integer(const uint_type(&digits_of_max_number)[max_number_of_digits], const std::string_view& str) {
    static_assert(sizeof(digits_of_max_number) == sizeof(uint_type) * max_number_of_digits);
    static_assert(std::is_unsigned_v<uint_type>);

    if (str.empty() || str.size() > max_number_of_digits)
        return std::nullopt;


    bool must_check_for_max_num = str.size() == max_number_of_digits;
    unsigned index = 0;

    uint_type out_number = 0;
    for (char c : str) {
        static_assert('0' < '9');
        if (c < '0' || c > '9')
            return std::nullopt;

        uint_type digit = c - '0';

        if (digit == 0 && out_number == 0)
            continue;

        if (must_check_for_max_num) {
            auto max_num_digit = digits_of_max_number[index];
            ++index;
            if (digit > max_num_digit)
                return std::nullopt;
            if (digit < max_num_digit)
                must_check_for_max_num = false;
        }

        out_number *= 10;
        out_number += digit;
    }
    return out_number;
}

template<class uint_type, std::size_t max_number_of_digits>
constexpr bool does_array_represent_max_number(const uint_type(&digits)[max_number_of_digits]) {
    uint_type value{ 0 };
    for (int i{ 0 }; i < max_number_of_digits; ++i) {
        value *= 10;
        value += digits[i];
    }
    return value == std::numeric_limits<uint_type>::max();
}


template<class uint>
constexpr std::optional<uint> string_to_uint(const std::string_view& str);

template<>
constexpr std::optional<std::uint32_t> string_to_uint<uint32_t>(const std::string_view& str) {

    static_assert(sizeof(uint32_t) == 4);
    // 2^32 - 1  == 4'294'967'295
    constexpr const uint32_t max_number_digits[10] = { 4,2,9,4,9,6,7,2,9,5 };
    static_assert(does_array_represent_max_number(max_number_digits));

    return string_base10_to_integer(max_number_digits, str);
}

             
template<>
constexpr std::optional<std::uint64_t> string_to_uint<uint64_t>(const std::string_view& str) {

    // 2^64 - 1 == 18446744073709551615
    constexpr const uint64_t digits_of_max_number[] = { 1,8,4,4,6,7,4,4,0,7,3,7,0,9,5,5,1,6,1,5 };
    static_assert(does_array_represent_max_number(digits_of_max_number));

    return string_base10_to_integer(digits_of_max_number, str);
}




constexpr std::optional<HANDLE> string_to_HANDLE(const std::string_view& str) {

    static_assert(sizeof(HANDLE) == 4 || sizeof(HANDLE) == 8);

    typedef std::enable_if_t< sizeof(HANDLE) == 4 || sizeof(HANDLE) == 8,
        std::conditional_t<sizeof(HANDLE) == 4, uint32_t, uint64_t>>
        uint_HANDLE;

    std::optional<uint_HANDLE> uint_opt = string_to_uint<uint_HANDLE>(str);
    if (!uint_opt)
        return std::nullopt;
    
    return std::bit_cast<HANDLE>(*uint_opt);
}



template<class T>
struct set_and_reset {
    static_assert(std::is_unsigned_v<T>);
    T set_all_ones_to_one{ 0u };
    T set_all_zeros_to_zero{ std::numeric_limits<T>::max() };

    T change(T v) {
        v |= set_all_ones_to_one;
        v &= set_all_zeros_to_zero;
        return v;
    }

    bool unchanging() {
        constexpr set_and_reset default_value{};
        return set_all_ones_to_one == default_value.set_all_ones_to_one 
            && set_all_zeros_to_zero == default_value.set_all_zeros_to_zero;
    }

    std::optional<std::string> to_string() const {
        constexpr std::size_t bits{ sizeof(T) * 8 };
        std::string ret(bits, '.');
        constexpr T one{ 1u };
        constexpr T zero{ 0u };
        T set_ones = set_all_ones_to_one;
        T set_zeros = set_all_zeros_to_zero;
        for (int i = 0; i < bits; ++i) {
            bool make_one = one == (one & set_ones);
            bool make_zero = zero == (one & set_zeros);
            if (make_one && make_zero)
                return std::nullopt;
            if (make_one)
                ret[i] = '1';
            if (make_zero)
                ret[i] = '0';
        }
        return ret;
    }
};


template<class T>
std::optional<set_and_reset<T>> parse_set_and_reset_string(std::string_view str) {
    static_assert(std::is_unsigned_v<T>);
    constexpr std::size_t bits{ sizeof(T) * 8 };
    if (str.length() > bits)
        return std::nullopt;

    constexpr T one{ 1u };

    set_and_reset<T> ret{0u,0u};
    for (char c : str) {
        ret.set_all_ones_to_one <<= 1;
        ret.set_all_zeros_to_zero <<= 1;
        if (c == '1') {
            ret.set_all_ones_to_one |= one;
        }
        else if (c == '0') {
            ret.set_all_zeros_to_zero |= one;
        }
        else if (c != '.') {
            return std::nullopt;
        }
    }
    ret.set_all_zeros_to_zero = ~ret.set_all_zeros_to_zero;
    return ret;
}