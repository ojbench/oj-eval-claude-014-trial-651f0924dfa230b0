#pragma once
#ifndef PYTHON_INTERPRETER_BIGINTEGER_H
#define PYTHON_INTERPRETER_BIGINTEGER_H

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

class BigInteger {
private:
    std::vector<int> digits; // Store digits in reverse order (least significant first)
    bool negative;

    void removeLeadingZeros() {
        while (digits.size() > 1 && digits.back() == 0) {
            digits.pop_back();
        }
        if (digits.size() == 1 && digits[0] == 0) {
            negative = false;
        }
    }

public:
    BigInteger() : digits(1, 0), negative(false) {}

    BigInteger(long long val) {
        if (val == 0) {
            digits.push_back(0);
            negative = false;
        } else {
            negative = val < 0;
            val = std::abs(val);
            while (val > 0) {
                digits.push_back(val % 10);
                val /= 10;
            }
        }
    }

    BigInteger(const std::string& str) {
        if (str.empty() || str == "0") {
            digits.push_back(0);
            negative = false;
            return;
        }

        size_t start = 0;
        negative = (str[0] == '-');
        if (negative || str[0] == '+') start = 1;

        for (int i = str.length() - 1; i >= (int)start; i--) {
            digits.push_back(str[i] - '0');
        }
        removeLeadingZeros();
    }

    bool isZero() const {
        return digits.size() == 1 && digits[0] == 0;
    }

    bool isNegative() const {
        return negative && !isZero();
    }

    std::string toString() const {
        if (isZero()) return "0";
        std::string result;
        if (negative) result += "-";
        for (int i = digits.size() - 1; i >= 0; i--) {
            result += char('0' + digits[i]);
        }
        return result;
    }

    double toDouble() const {
        return std::stod(toString());
    }

    // Comparison operators
    bool operator<(const BigInteger& other) const {
        if (negative != other.negative) {
            return negative;
        }
        if (digits.size() != other.digits.size()) {
            return negative ? (digits.size() > other.digits.size()) : (digits.size() < other.digits.size());
        }
        for (int i = digits.size() - 1; i >= 0; i--) {
            if (digits[i] != other.digits[i]) {
                return negative ? (digits[i] > other.digits[i]) : (digits[i] < other.digits[i]);
            }
        }
        return false;
    }

    bool operator>(const BigInteger& other) const { return other < *this; }
    bool operator<=(const BigInteger& other) const { return !(other < *this); }
    bool operator>=(const BigInteger& other) const { return !(*this < other); }
    bool operator==(const BigInteger& other) const {
        return negative == other.negative && digits == other.digits;
    }
    bool operator!=(const BigInteger& other) const { return !(*this == other); }

    // Arithmetic operators
    BigInteger operator+(const BigInteger& other) const {
        if (negative == other.negative) {
            BigInteger result;
            result.negative = negative;
            result.digits.clear();

            int carry = 0;
            size_t maxLen = std::max(digits.size(), other.digits.size());
            for (size_t i = 0; i < maxLen || carry; i++) {
                int sum = carry;
                if (i < digits.size()) sum += digits[i];
                if (i < other.digits.size()) sum += other.digits[i];
                result.digits.push_back(sum % 10);
                carry = sum / 10;
            }
            result.removeLeadingZeros();
            return result;
        } else {
            if (negative) {
                return other - (-*this);
            } else {
                return *this - (-other);
            }
        }
    }

    BigInteger operator-() const {
        BigInteger result = *this;
        if (!isZero()) result.negative = !negative;
        return result;
    }

    BigInteger operator-(const BigInteger& other) const {
        if (negative != other.negative) {
            BigInteger result = *this + (-other);
            result.negative = negative;
            return result;
        }

        if (negative) {
            return -((-*this) - (-other));
        }

        if (*this < other) {
            return -(other - *this);
        }

        BigInteger result;
        result.digits.clear();
        int borrow = 0;
        for (size_t i = 0; i < digits.size(); i++) {
            int diff = digits[i] - borrow;
            if (i < other.digits.size()) diff -= other.digits[i];
            if (diff < 0) {
                diff += 10;
                borrow = 1;
            } else {
                borrow = 0;
            }
            result.digits.push_back(diff);
        }
        result.removeLeadingZeros();
        return result;
    }

    BigInteger operator*(const BigInteger& other) const {
        BigInteger result;
        result.digits.assign(digits.size() + other.digits.size(), 0);

        for (size_t i = 0; i < digits.size(); i++) {
            for (size_t j = 0; j < other.digits.size(); j++) {
                result.digits[i + j] += digits[i] * other.digits[j];
            }
        }

        for (size_t i = 0; i < result.digits.size() - 1; i++) {
            result.digits[i + 1] += result.digits[i] / 10;
            result.digits[i] %= 10;
        }

        result.negative = (negative != other.negative);
        result.removeLeadingZeros();
        return result;
    }

    // Integer division (floor division)
    BigInteger operator/(const BigInteger& other) const {
        if (other.isZero()) {
            throw std::runtime_error("Division by zero");
        }

        BigInteger dividend = *this;
        dividend.negative = false;
        BigInteger divisor = other;
        divisor.negative = false;

        if (dividend < divisor) {
            if (negative != other.negative) {
                return BigInteger(-1);
            }
            return BigInteger(0);
        }

        BigInteger quotient;
        quotient.digits.clear();
        BigInteger current;
        current.digits.clear();

        for (int i = dividend.digits.size() - 1; i >= 0; i--) {
            current.digits.insert(current.digits.begin(), dividend.digits[i]);
            current.removeLeadingZeros();

            int q = 0;
            while (!(current < divisor)) {
                current = current - divisor;
                q++;
            }
            quotient.digits.insert(quotient.digits.begin(), q);
        }

        quotient.removeLeadingZeros();

        // Floor division adjustment for negative results
        if (negative != other.negative) {
            if (!current.isZero()) {
                quotient = quotient + BigInteger(1);
            }
            quotient.negative = true;
        }

        return quotient;
    }

    BigInteger operator%(const BigInteger& other) const {
        BigInteger quotient = *this / other;
        BigInteger result = *this - (quotient * other);
        return result;
    }
};

#endif // PYTHON_INTERPRETER_BIGINTEGER_H
