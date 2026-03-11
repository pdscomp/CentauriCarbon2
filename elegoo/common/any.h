/***************************************************************************** 
 * @Author       : loping
 * @Date         : 2024-11-26 10:38:23
 * @LastEditors  : Jack
 * @LastEditTime : 2024-12-30 11:16:47
 * @Description  : 
 * 
 * Copyright (c) 2024 by ELEGOO, All Rights Reserved. 
 *****************************************************************************/

#ifndef __ANY_H__
#define __ANY_H__


#pragma once


#include <iostream>
#include <new>       // 用于std::bad_alloc
#include <typeinfo>  // 用于typeid和std::bad_cast
#include <functional> // 用于std::hash
#include <memory>    // 用于std::unique_ptr和std::allocator
#include <stdexcept> // 用于std::runtime_error和std::bad_cast
#include <type_traits> // 用于std::decay和std::is_same
#include <cstring>   // 用于std::memcpy和std::strlen（如果存储字符串）

class Any {
public:
    // 默认构造函数
    Any() noexcept : holder(nullptr), typeInfo(nullptr) {}
 
    // 拷贝构造函数
    Any(const Any& other) {
        if (other.holder) {
            other.holder->cloneInto(*this);
        }
    }
 
    // 移动构造函数
    Any(Any&& other) noexcept : holder(other.holder), typeInfo(other.typeInfo) {
        other.holder = nullptr;
        other.typeInfo = nullptr;
    }
 
    // 析构函数
    ~Any() {
        delete holder;
    }
 
    // 赋值运算符
    Any& operator=(const Any& other) {
        if (this != &other) {
            Any temp(other);
            std::swap(holder, temp.holder);
            std::swap(typeInfo, temp.typeInfo);
        }
        return *this;
    }
 
    // 移动赋值运算符
    Any& operator=(Any&& other) noexcept {
        if (this != &other) {
            delete holder;
            holder = other.holder;
            typeInfo = other.typeInfo;
            other.holder = nullptr;
            other.typeInfo = nullptr;
        }
        return *this;
    }
 
    // 模板化构造函数，用于存储任意类型的值
    template<typename ValueType, typename = typename std::enable_if<!std::is_same<Any, typename std::decay<ValueType>::type>::value, int>::type>
    Any(ValueType value) {
        typeInfo = &typeid(ValueType);
        holder = new Holder<ValueType>(value);
    }
 
    // 检查存储的值是否为给定类型
    template<typename ValueType>
    bool is() const {
        return typeInfo == &typeid(ValueType);
    }
 
    // 转换存储的值为给定类型，如果类型不匹配则抛出std::bad_cast
    template<typename ValueType>
    ValueType as() const {
        if (!is<ValueType>()) {
            throw std::bad_cast();
        }
        return static_cast<Holder<ValueType>*>(holder)->value;
    }
 
    // 返回存储的值的类型信息
    const std::type_info& type() const {
        return *typeInfo;
    }
 
    // 判断Any是否为空（即未存储任何值）
    bool empty() const {
        return !holder;
    }
 
// private:
    // 基类占位符，用于多态存储不同类型的值
    struct Placeholder {
        virtual ~Placeholder() {}
        virtual Placeholder* clone() const = 0;
        virtual void cloneInto(Any& other) const = 0;
    };
 
    // 模板化派生类，用于实际存储值
    template<typename ValueType>
    struct Holder : public Placeholder {
        Holder(ValueType value) : value(value) {}
        virtual Placeholder* clone() const override {
            return new Holder(value);
        }
        virtual void cloneInto(Any& other) const override {
            other.holder = new Holder(value);
            other.typeInfo = &typeid(ValueType);
        }
        ValueType value;
    };
 
    Placeholder* holder;
    const std::type_info* typeInfo;
};

class BadAnyCastException : public std::runtime_error {
public:
    explicit BadAnyCastException(const char* what_arg)
        : std::runtime_error(what_arg) {}
};

template<typename ValueType>
ValueType any_cast(Any& operand) {
    if (operand.empty()) {
        throw BadAnyCastException("Nullptr Any cast");
    }
    if (operand.type() == typeid(ValueType)) {
        return static_cast<Any::Holder<ValueType>*>(operand.holder)->value;
    } else {
        throw BadAnyCastException("Bad Any cast");
    }
}

template<typename ValueType>
ValueType any_cast(const Any& operand) {
    if (operand.empty()) {
        throw BadAnyCastException("Nullptr Any cast");
    }
    if (operand.type() == typeid(ValueType)) {
        return static_cast<const Any::Holder<ValueType>*>(operand.holder)->value;
    } else {
        throw BadAnyCastException("Bad Any cast");
    }
}

class TestAny
{
public:
    TestAny(){}
    ~TestAny(){}
    void test();
    void printObj(std::string name,Any any);
};








#endif