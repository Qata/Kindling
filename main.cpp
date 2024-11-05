//Compiled on 11/5/2024 4:19:39 PM
#include <inttypes.h>
#include <stdbool.h>
#include <new>

#ifndef JUNIPER_H
#define JUNIPER_H

#include <stdlib.h>

#ifdef JUN_CUSTOM_PLACEMENT_NEW
void* operator new(size_t size, void* ptr)
{
    return ptr;
}
#endif

namespace juniper
{
    template<typename A>
    struct counted_cell {
        int ref_count;
        A data;

        counted_cell(A init_data)
            : ref_count(1), data(init_data) {}

        void destroy() {
            data.destroy();
        }
    };

    template <template <typename> class T, typename contained>
    class shared_ptr {
    private:
        counted_cell<T<contained>>* content;

        void inc_ref() {
            if (content != nullptr) {
                content->ref_count++;
            }
        }

        void dec_ref() {
            if (content != nullptr) {
                content->ref_count--;

                if (content->ref_count <= 0) {
                    content->destroy();
                    delete content;
                    content = nullptr;
                }
            }
        }

    public:
        shared_ptr()
            : content(nullptr)
        {
        }

        shared_ptr(T<contained> init_data)
            : content(new counted_cell<T<contained>>(init_data))
        {
        }

        // Copy constructor
        shared_ptr(const shared_ptr& rhs)
            : content(rhs.content)
        {
            inc_ref();
        }

        // Move constructor
        shared_ptr(shared_ptr&& dyingObj)
            : content(dyingObj.content) {

            // Clean the dying object
            dyingObj.content = nullptr;
        }

        ~shared_ptr()
        {
            dec_ref();
        }

        contained* get() {
            if (content != nullptr) {
                return content->data.get();
            }
            else {
                return nullptr;
            }
        }

        const contained* get() const {
            if (content != nullptr) {
                return content->data.get();
            } else {
                return nullptr;
            }
        }

        // Copy assignment
        shared_ptr& operator=(const shared_ptr& rhs) {
            // We're overwriting the current content with new content,
            // so decrement the current content referene count by 1.
            dec_ref();
            // We're now pointing to new content
            content = rhs.content;
            // Increment the reference count of the new content.
            inc_ref();

            return *this;
        }

        // Move assignment
        shared_ptr& operator=(shared_ptr&& dyingObj) {
            // We're overwriting the current content with new content,
            // so decrement the current content referene count by 1.
            dec_ref();

            content = dyingObj.content;

            // Clean the dying object
            dyingObj.content = nullptr;
            // Don't increment at all because the dying object
            // reduces the reference count by 1, which would cancel out
            // an increment.

            return *this;
        }

        // Two shared ptrs are equal if they point to the same data
        bool operator==(shared_ptr& rhs) {
            return content == rhs.content;
        }

        bool operator!=(shared_ptr& rhs) {
            return content != rhs.content;
        }
    };

    template <typename ClosureType, typename Result, typename ...Args>
    class function;

    template <typename Result, typename ...Args>
    class function<void, Result(Args...)> {
    private:
        Result(*F)(Args...);

    public:
        function(Result(*f)(Args...)) : F(f) {}

        Result operator()(Args... args) {
            return F(args...);
        }
    };

    template <typename ClosureType, typename Result, typename ...Args>
    class function<ClosureType, Result(Args...)> {
    private:
        ClosureType Closure;
        Result(*F)(ClosureType&, Args...);

    public:
        function(ClosureType closure, Result(*f)(ClosureType&, Args...)) : Closure(closure), F(f) {}

        Result operator()(Args... args) {
            return F(Closure, args...);
        }
    };

    template<typename T, size_t N>
    class array {
    public:
        array<T, N>& fill(T fillWith) {
            for (size_t i = 0; i < N; i++) {
                data[i] = fillWith;
            }

            return *this;
        }

        T& operator[](int i) {
            return data[i];
        }

        bool operator==(array<T, N>& rhs) {
            for (auto i = 0; i < N; i++) {
                if (data[i] != rhs[i]) {
                    return false;
                }
            }
            return true;
        }

        bool operator!=(array<T, N>& rhs) { return !(rhs == *this); }

        T data[N];
    };

    struct unit {
    public:
        bool operator==(unit rhs) {
            return true;
        }

        bool operator!=(unit rhs) {
            return !(rhs == *this);
        }
    };

    // A basic container that doesn't do anything special to destroy
    // the contained data when the reference count drops to 0. This
    // is used for ref types.
    template <typename contained>
    class basic_container {
    private:
        contained data;

    public:
        basic_container(contained initData)
            : data(initData) {}

        void destroy() {}

        contained *get() { return &data;  }
    };

    // A container that calls a finalizer once the reference count
    // drops to 0. This is used for rcptr types.
    template <typename contained>
    class finalized_container {
    private:
        contained data;
        function<void, unit(void*)> finalizer;

    public:
        finalized_container(void* initData, function<void, unit(contained)> initFinalizer)
            : data(initData), finalizer(initFinalizer) {}

        void destroy() {
            finalizer(data);
        }

        contained *get() { return &data; }
    };

    template <typename contained>
    using refcell = shared_ptr<basic_container, contained>;

    using rcptr = shared_ptr<finalized_container, void*>;

    rcptr make_rcptr(void* initData, function<void, unit(void*)> finalizer) {
        return rcptr(finalized_container<void *>(initData, finalizer));
    }

    template<typename T>
    T quit() {
        exit(1);
    }

    // Equivalent to std::aligned_storage
    template<unsigned int Len, unsigned int Align>
    struct aligned_storage {
        struct type {
            alignas(Align) unsigned char data[Len];
        };
    };

    template <unsigned int arg1, unsigned int ... others>
    struct static_max;

    template <unsigned int arg>
    struct static_max<arg>
    {
        static const unsigned int value = arg;
    };

    template <unsigned int arg1, unsigned int arg2, unsigned int ... others>
    struct static_max<arg1, arg2, others...>
    {
        static const unsigned int value = arg1 >= arg2 ? static_max<arg1, others...>::value :
            static_max<arg2, others...>::value;
    };

    template<class T> struct remove_reference { typedef T type; };
    template<class T> struct remove_reference<T&> { typedef T type; };
    template<class T> struct remove_reference<T&&> { typedef T type; };

    template<unsigned char n, typename... Ts>
    struct variant_helper_rec;

    template<unsigned char n, typename F, typename... Ts>
    struct variant_helper_rec<n, F, Ts...> {
        inline static void destroy(unsigned char id, void* data)
        {
            if (n == id) {
                reinterpret_cast<F*>(data)->~F();
            }
            else {
                variant_helper_rec<n + 1, Ts...>::destroy(id, data);
            }
        }

        inline static void move(unsigned char id, void* from, void* to)
        {
            if (n == id) {
                // This static_cast and use of remove_reference is equivalent to the use of std::move
                new (to) F(static_cast<typename remove_reference<F>::type&&>(*reinterpret_cast<F*>(from)));
            }
            else {
                variant_helper_rec<n + 1, Ts...>::move(id, from, to);
            }
        }

        inline static void copy(unsigned char id, const void* from, void* to)
        {
            if (n == id) {
                new (to) F(*reinterpret_cast<const F*>(from));
            }
            else {
                variant_helper_rec<n + 1, Ts...>::copy(id, from, to);
            }
        }

        inline static bool equal(unsigned char id, void* lhs, void* rhs)
        {
            if (n == id) {
                return (*reinterpret_cast<F*>(lhs)) == (*reinterpret_cast<F*>(rhs));
            }
            else {
                return variant_helper_rec<n + 1, Ts...>::equal(id, lhs, rhs);
            }
        }
    };

    template<unsigned char n> struct variant_helper_rec<n> {
        inline static void destroy(unsigned char id, void* data) { }
        inline static void move(unsigned char old_t, void* from, void* to) { }
        inline static void copy(unsigned char old_t, const void* from, void* to) { }
        inline static bool equal(unsigned char id, void* lhs, void* rhs) { return false; }
    };

    template<typename... Ts>
    struct variant_helper {
        inline static void destroy(unsigned char id, void* data) {
            variant_helper_rec<0, Ts...>::destroy(id, data);
        }

        inline static void move(unsigned char id, void* from, void* to) {
            variant_helper_rec<0, Ts...>::move(id, from, to);
        }

        inline static void copy(unsigned char id, const void* old_v, void* new_v) {
            variant_helper_rec<0, Ts...>::copy(id, old_v, new_v);
        }

        inline static bool equal(unsigned char id, void* lhs, void* rhs) {
            return variant_helper_rec<0, Ts...>::equal(id, lhs, rhs);
        }
    };

    template<> struct variant_helper<> {
        inline static void destroy(unsigned char id, void* data) { }
        inline static void move(unsigned char old_t, void* old_v, void* new_v) { }
        inline static void copy(unsigned char old_t, const void* old_v, void* new_v) { }
    };

    template<typename F>
    struct variant_helper_static;

    template<typename F>
    struct variant_helper_static {
        inline static void move(void* from, void* to) {
            new (to) F(static_cast<typename remove_reference<F>::type&&>(*reinterpret_cast<F*>(from)));
        }

        inline static void copy(const void* from, void* to) {
            new (to) F(*reinterpret_cast<const F*>(from));
        }
    };

    // Given a unsigned char i, selects the ith type from the list of item types
    template<unsigned char i, typename... Items>
    struct variant_alternative;

    template<typename HeadItem, typename... TailItems>
    struct variant_alternative<0, HeadItem, TailItems...>
    {
        using type = HeadItem;
    };

    template<unsigned char i, typename HeadItem, typename... TailItems>
    struct variant_alternative<i, HeadItem, TailItems...>
    {
        using type = typename variant_alternative<i - 1, TailItems...>::type;
    };

    template<typename... Ts>
    struct variant {
    private:
        static const unsigned int data_size = static_max<sizeof(Ts)...>::value;
        static const unsigned int data_align = static_max<alignof(Ts)...>::value;

        using data_t = typename aligned_storage<data_size, data_align>::type;

        using helper_t = variant_helper<Ts...>;

        template<unsigned char i>
        using alternative = typename variant_alternative<i, Ts...>::type;

        unsigned char variant_id;
        data_t data;

        variant(unsigned char id) : variant_id(id) {}

    public:
        template<unsigned char i>
        static variant create(alternative<i>& value)
        {
            variant ret(i);
            variant_helper_static<alternative<i>>::copy(&value, &ret.data);
            return ret;
        }

        template<unsigned char i>
        static variant create(alternative<i>&& value) {
            variant ret(i);
            variant_helper_static<alternative<i>>::move(&value, &ret.data);
            return ret;
        }

        variant() {}

        variant(const variant<Ts...>& from) : variant_id(from.variant_id)
        {
            helper_t::copy(from.variant_id, &from.data, &data);
        }

        variant(variant<Ts...>&& from) : variant_id(from.variant_id)
        {
            helper_t::move(from.variant_id, &from.data, &data);
        }

        variant<Ts...>& operator= (variant<Ts...>& rhs)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = rhs.variant_id;
            helper_t::copy(rhs.variant_id, &rhs.data, &data);
            return *this;
        }

        variant<Ts...>& operator= (variant<Ts...>&& rhs)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = rhs.variant_id;
            helper_t::move(rhs.variant_id, &rhs.data, &data);
            return *this;
        }

        unsigned char id() {
            return variant_id;
        }

        template<unsigned char i>
        void set(alternative<i>& value)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = i;
            variant_helper_static<alternative<i>>::copy(&value, &data);
        }

        template<unsigned char i>
        void set(alternative<i>&& value)
        {
            helper_t::destroy(variant_id, &data);
            variant_id = i;
            variant_helper_static<alternative<i>>::move(&value, &data);
        }

        template<unsigned char i>
        alternative<i>& get()
        {
            if (variant_id == i) {
                return *reinterpret_cast<alternative<i>*>(&data);
            }
            else {
                return quit<alternative<i>&>();
            }
        }

        ~variant() {
            helper_t::destroy(variant_id, &data);
        }

        bool operator==(variant& rhs) {
            if (variant_id == rhs.variant_id) {
                return helper_t::equal(variant_id, &data, &rhs.data);
            }
            else {
                return false;
            }
        }

        bool operator==(variant&& rhs) {
            if (variant_id == rhs.variant_id) {
                return helper_t::equal(variant_id, &data, &rhs.data);
            }
            else {
                return false;
            }
        }

        bool operator!=(variant& rhs) {
            return !(this->operator==(rhs));
        }

        bool operator!=(variant&& rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a, typename b>
    struct tuple2 {
        a e1;
        b e2;

        tuple2() {}

        tuple2(a initE1, b initE2) : e1(initE1), e2(initE2) {}

        bool operator==(tuple2<a, b> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2;
        }

        bool operator!=(tuple2<a, b> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c>
    struct tuple3 {
        a e1;
        b e2;
        c e3;

        tuple3() {}

        tuple3(a initE1, b initE2, c initE3) : e1(initE1), e2(initE2), e3(initE3) {}

        bool operator==(tuple3<a, b, c> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3;
        }

        bool operator!=(tuple3<a, b, c> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d>
    struct tuple4 {
        a e1;
        b e2;
        c e3;
        d e4;

        tuple4() {}

        tuple4(a initE1, b initE2, c initE3, d initE4) : e1(initE1), e2(initE2), e3(initE3), e4(initE4) {}

        bool operator==(tuple4<a, b, c, d> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4;
        }

        bool operator!=(tuple4<a, b, c, d> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e>
    struct tuple5 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;

        tuple5() {}

        tuple5(a initE1, b initE2, c initE3, d initE4, e initE5) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5) {}

        bool operator==(tuple5<a, b, c, d, e> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5;
        }

        bool operator!=(tuple5<a, b, c, d, e> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f>
    struct tuple6 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;

        tuple6() {}

        tuple6(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6) {}

        bool operator==(tuple6<a, b, c, d, e, f> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6;
        }

        bool operator!=(tuple6<a, b, c, d, e, f> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g>
    struct tuple7 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;

        tuple7() {}

        tuple7(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7) {}

        bool operator==(tuple7<a, b, c, d, e, f, g> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7;
        }

        bool operator!=(tuple7<a, b, c, d, e, f, g> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g, typename h>
    struct tuple8 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;
        h e8;

        tuple8() {}

        tuple8(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7, h initE8) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7), e8(initE8) {}

        bool operator==(tuple8<a, b, c, d, e, f, g, h> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8;
        }

        bool operator!=(tuple8<a, b, c, d, e, f, g, h> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g, typename h, typename i>
    struct tuple9 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;
        h e8;
        i e9;

        tuple9() {}

        tuple9(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7, h initE8, i initE9) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7), e8(initE8), e9(initE9) {}

        bool operator==(tuple9<a, b, c, d, e, f, g, h, i> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8 && e9 == rhs.e9;
        }

        bool operator!=(tuple9<a, b, c, d, e, f, g, h, i> rhs) {
            return !(rhs == *this);
        }
    };

    template<typename a, typename b, typename c, typename d, typename e, typename f, typename g, typename h, typename i, typename j>
    struct tuple10 {
        a e1;
        b e2;
        c e3;
        d e4;
        e e5;
        f e6;
        g e7;
        h e8;
        i e9;
        j e10;

        tuple10() {}

        tuple10(a initE1, b initE2, c initE3, d initE4, e initE5, f initE6, g initE7, h initE8, i initE9, j initE10) : e1(initE1), e2(initE2), e3(initE3), e4(initE4), e5(initE5), e6(initE6), e7(initE7), e8(initE8), e9(initE9), e10(initE10) {}

        bool operator==(tuple10<a, b, c, d, e, f, g, h, i, j> rhs) {
            return e1 == rhs.e1 && e2 == rhs.e2 && e3 == rhs.e3 && e4 == rhs.e4 && e5 == rhs.e5 && e6 == rhs.e6 && e7 == rhs.e7 && e8 == rhs.e8 && e9 == rhs.e9 && e10 == rhs.e10;
        }

        bool operator!=(tuple10<a, b, c, d, e, f, g, h, i, j> rhs) {
            return !(rhs == *this);
        }
    };
}

#endif

#include <Arduino.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

namespace Prelude {}
namespace List {}
namespace Signal {}
namespace Io {}
namespace Maybe {}
namespace Time {}
namespace Math {}
namespace Button {}
namespace Vector {}
namespace CharList {}
namespace StringM {}
namespace Random {}
namespace Color {}
namespace ListExt {}
namespace SignalExt {}
namespace NeoPixel {}
namespace TEA {}
namespace List {
    using namespace Prelude;

}

namespace Signal {
    using namespace Prelude;

}

namespace Io {
    using namespace Prelude;

}

namespace Maybe {
    using namespace Prelude;

}

namespace Time {
    using namespace Prelude;

}

namespace Math {
    using namespace Prelude;

}

namespace Button {
    using namespace Prelude;

}

namespace Button {
    using namespace Io;

}

namespace Vector {
    using namespace Prelude;

}

namespace Vector {
    using namespace List;
    using namespace Math;

}

namespace CharList {
    using namespace Prelude;

}

namespace StringM {
    using namespace Prelude;

}

namespace Random {
    using namespace Prelude;

}

namespace Color {
    using namespace Prelude;

}

namespace ListExt {
    using namespace Prelude;

}

namespace SignalExt {
    using namespace Prelude;

}

namespace SignalExt {
    using namespace Signal;

}

namespace NeoPixel {
    using namespace Prelude;

}

namespace NeoPixel {
    using namespace Io;
    using namespace Time;

}

namespace TEA {
    using namespace Prelude;

}

namespace TEA {
    using namespace NeoPixel;

}

namespace juniper {
    namespace records {
        template<typename T1,typename T2,typename T3,typename T4>
        struct recordt_4 {
            T1 a;
            T2 b;
            T3 g;
            T4 r;

            recordt_4() {}

            recordt_4(T1 init_a, T2 init_b, T3 init_g, T4 init_r)
                : a(init_a), b(init_b), g(init_g), r(init_r) {}

            bool operator==(recordt_4<T1, T2, T3, T4> rhs) {
                return true && a == rhs.a && b == rhs.b && g == rhs.g && r == rhs.r;
            }

            bool operator!=(recordt_4<T1, T2, T3, T4> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3,typename T4>
        struct recordt_6 {
            T1 a;
            T2 h;
            T3 s;
            T4 v;

            recordt_6() {}

            recordt_6(T1 init_a, T2 init_h, T3 init_s, T4 init_v)
                : a(init_a), h(init_h), s(init_s), v(init_v) {}

            bool operator==(recordt_6<T1, T2, T3, T4> rhs) {
                return true && a == rhs.a && h == rhs.h && s == rhs.s && v == rhs.v;
            }

            bool operator!=(recordt_6<T1, T2, T3, T4> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3>
        struct recordt_2 {
            T1 actualState;
            T2 lastDebounceTime;
            T3 lastState;

            recordt_2() {}

            recordt_2(T1 init_actualState, T2 init_lastDebounceTime, T3 init_lastState)
                : actualState(init_actualState), lastDebounceTime(init_lastDebounceTime), lastState(init_lastState) {}

            bool operator==(recordt_2<T1, T2, T3> rhs) {
                return true && actualState == rhs.actualState && lastDebounceTime == rhs.lastDebounceTime && lastState == rhs.lastState;
            }

            bool operator!=(recordt_2<T1, T2, T3> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3>
        struct recordt_3 {
            T1 b;
            T2 g;
            T3 r;

            recordt_3() {}

            recordt_3(T1 init_b, T2 init_g, T3 init_r)
                : b(init_b), g(init_g), r(init_r) {}

            bool operator==(recordt_3<T1, T2, T3> rhs) {
                return true && b == rhs.b && g == rhs.g && r == rhs.r;
            }

            bool operator!=(recordt_3<T1, T2, T3> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2>
        struct recordt_0 {
            T1 data;
            T2 length;

            recordt_0() {}

            recordt_0(T1 init_data, T2 init_length)
                : data(init_data), length(init_length) {}

            bool operator==(recordt_0<T1, T2> rhs) {
                return true && data == rhs.data && length == rhs.length;
            }

            bool operator!=(recordt_0<T1, T2> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3,typename T4,typename T5>
        struct recordt_9 {
            T1 device;
            T2 operation;
            T3 pin;
            T4 pixels;
            T5 previousPixels;

            recordt_9() {}

            recordt_9(T1 init_device, T2 init_operation, T3 init_pin, T4 init_pixels, T5 init_previousPixels)
                : device(init_device), operation(init_operation), pin(init_pin), pixels(init_pixels), previousPixels(init_previousPixels) {}

            bool operator==(recordt_9<T1, T2, T3, T4, T5> rhs) {
                return true && device == rhs.device && operation == rhs.operation && pin == rhs.pin && pixels == rhs.pixels && previousPixels == rhs.previousPixels;
            }

            bool operator!=(recordt_9<T1, T2, T3, T4, T5> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3,typename T4>
        struct recordt_8 {
            T1 endAfter;
            T2 function;
            T3 interval;
            T4 timer;

            recordt_8() {}

            recordt_8(T1 init_endAfter, T2 init_function, T3 init_interval, T4 init_timer)
                : endAfter(init_endAfter), function(init_function), interval(init_interval), timer(init_timer) {}

            bool operator==(recordt_8<T1, T2, T3, T4> rhs) {
                return true && endAfter == rhs.endAfter && function == rhs.function && interval == rhs.interval && timer == rhs.timer;
            }

            bool operator!=(recordt_8<T1, T2, T3, T4> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1,typename T2,typename T3>
        struct recordt_5 {
            T1 h;
            T2 s;
            T3 v;

            recordt_5() {}

            recordt_5(T1 init_h, T2 init_s, T3 init_v)
                : h(init_h), s(init_s), v(init_v) {}

            bool operator==(recordt_5<T1, T2, T3> rhs) {
                return true && h == rhs.h && s == rhs.s && v == rhs.v;
            }

            bool operator!=(recordt_5<T1, T2, T3> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1>
        struct recordt_1 {
            T1 lastPulse;

            recordt_1() {}

            recordt_1(T1 init_lastPulse)
                : lastPulse(init_lastPulse) {}

            bool operator==(recordt_1<T1> rhs) {
                return true && lastPulse == rhs.lastPulse;
            }

            bool operator!=(recordt_1<T1> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1>
        struct recordt_10 {
            T1 lines;

            recordt_10() {}

            recordt_10(T1 init_lines)
                : lines(init_lines) {}

            bool operator==(recordt_10<T1> rhs) {
                return true && lines == rhs.lines;
            }

            bool operator!=(recordt_10<T1> rhs) {
                return !(rhs == *this);
            }
        };

        template<typename T1>
        struct recordt_7 {
            T1 pin;

            recordt_7() {}

            recordt_7(T1 init_pin)
                : pin(init_pin) {}

            bool operator==(recordt_7<T1> rhs) {
                return true && pin == rhs.pin;
            }

            bool operator!=(recordt_7<T1> rhs) {
                return !(rhs == *this);
            }
        };


    }
}

namespace juniper {
    namespace closures {
        template<typename T1>
        struct closuret_7 {
            T1 color;


            closuret_7(T1 init_color) :
                color(init_color) {}
        };

        template<typename T1>
        struct closuret_1 {
            T1 f;


            closuret_1(T1 init_f) :
                f(init_f) {}
        };

        template<typename T1,typename T2>
        struct closuret_0 {
            T1 f;
            T2 g;


            closuret_0(T1 init_f, T2 init_g) :
                f(init_f), g(init_g) {}
        };

        template<typename T1,typename T2>
        struct closuret_2 {
            T1 f;
            T2 valueA;


            closuret_2(T1 init_f, T2 init_valueA) :
                f(init_f), valueA(init_valueA) {}
        };

        template<typename T1,typename T2,typename T3>
        struct closuret_3 {
            T1 f;
            T2 valueA;
            T3 valueB;


            closuret_3(T1 init_f, T2 init_valueA, T3 init_valueB) :
                f(init_f), valueA(init_valueA), valueB(init_valueB) {}
        };

        template<typename T1,typename T2>
        struct closuret_9 {
            T1 line;
            T2 operation;


            closuret_9(T1 init_line, T2 init_operation) :
                line(init_line), operation(init_operation) {}
        };

        template<typename T1>
        struct closuret_11 {
            T1 model;


            closuret_11(T1 init_model) :
                model(init_model) {}
        };

        template<typename T1>
        struct closuret_6 {
            T1 nPixels;


            closuret_6(T1 init_nPixels) :
                nPixels(init_nPixels) {}
        };

        template<typename T1>
        struct closuret_10 {
            T1 operation;


            closuret_10(T1 init_operation) :
                operation(init_operation) {}
        };

        template<typename T1>
        struct closuret_5 {
            T1 pin;


            closuret_5(T1 init_pin) :
                pin(init_pin) {}
        };

        template<typename T1>
        struct closuret_8 {
            T1 state;


            closuret_8(T1 init_state) :
                state(init_state) {}
        };

        template<typename T1,typename T2>
        struct closuret_4 {
            T1 val1;
            T2 val2;


            closuret_4(T1 init_val1, T2 init_val2) :
                val1(init_val1), val2(init_val2) {}
        };


    }
}

namespace Prelude {
    template<typename a>
    struct maybe {
        juniper::variant<a, uint8_t> data;

        maybe() {}

        maybe(juniper::variant<a, uint8_t> initData) : data(initData) {}

        a just() {
            return data.template get<0>();
        }

        uint8_t nothing() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(maybe rhs) {
            return data == rhs.data;
        }

        bool operator!=(maybe rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a>
    Prelude::maybe<a> just(a data0) {
        return Prelude::maybe<a>(juniper::variant<a, uint8_t>::template create<0>(data0));
    }

    template<typename a>
    Prelude::maybe<a> nothing() {
        return Prelude::maybe<a>(juniper::variant<a, uint8_t>::template create<1>(0));
    }


}

namespace Prelude {
    template<typename a, typename b>
    struct either {
        juniper::variant<a, b> data;

        either() {}

        either(juniper::variant<a, b> initData) : data(initData) {}

        a left() {
            return data.template get<0>();
        }

        b right() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(either rhs) {
            return data == rhs.data;
        }

        bool operator!=(either rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a, typename b>
    Prelude::either<a, b> left(a data0) {
        return Prelude::either<a, b>(juniper::variant<a, b>::template create<0>(data0));
    }

    template<typename a, typename b>
    Prelude::either<a, b> right(b data0) {
        return Prelude::either<a, b>(juniper::variant<a, b>::template create<1>(data0));
    }


}

namespace Prelude {
    template<typename a, int n>
    using list = juniper::records::recordt_0<juniper::array<a, n>, uint32_t>;


}

namespace Prelude {
    template<int n>
    using charlist = juniper::records::recordt_0<juniper::array<uint8_t, (1)+(n)>, uint32_t>;


}

namespace Prelude {
    template<typename a>
    struct sig {
        juniper::variant<Prelude::maybe<a>> data;

        sig() {}

        sig(juniper::variant<Prelude::maybe<a>> initData) : data(initData) {}

        Prelude::maybe<a> signal() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(sig rhs) {
            return data == rhs.data;
        }

        bool operator!=(sig rhs) {
            return !(this->operator==(rhs));
        }
    };

    template<typename a>
    Prelude::sig<a> signal(Prelude::maybe<a> data0) {
        return Prelude::sig<a>(juniper::variant<Prelude::maybe<a>>::template create<0>(data0));
    }


}

namespace Io {
    struct pinState {
        juniper::variant<uint8_t, uint8_t> data;

        pinState() {}

        pinState(juniper::variant<uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t high() {
            return data.template get<0>();
        }

        uint8_t low() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(pinState rhs) {
            return data == rhs.data;
        }

        bool operator!=(pinState rhs) {
            return !(this->operator==(rhs));
        }
    };

    Io::pinState high() {
        return Io::pinState(juniper::variant<uint8_t, uint8_t>::template create<0>(0));
    }

    Io::pinState low() {
        return Io::pinState(juniper::variant<uint8_t, uint8_t>::template create<1>(0));
    }


}

namespace Io {
    struct mode {
        juniper::variant<uint8_t, uint8_t, uint8_t> data;

        mode() {}

        mode(juniper::variant<uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t input() {
            return data.template get<0>();
        }

        uint8_t output() {
            return data.template get<1>();
        }

        uint8_t inputPullup() {
            return data.template get<2>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(mode rhs) {
            return data == rhs.data;
        }

        bool operator!=(mode rhs) {
            return !(this->operator==(rhs));
        }
    };

    Io::mode input() {
        return Io::mode(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    Io::mode output() {
        return Io::mode(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    Io::mode inputPullup() {
        return Io::mode(juniper::variant<uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }


}

namespace Io {
    struct base {
        juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t> data;

        base() {}

        base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t> initData) : data(initData) {}

        uint8_t binary() {
            return data.template get<0>();
        }

        uint8_t octal() {
            return data.template get<1>();
        }

        uint8_t decimal() {
            return data.template get<2>();
        }

        uint8_t hexadecimal() {
            return data.template get<3>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(base rhs) {
            return data == rhs.data;
        }

        bool operator!=(base rhs) {
            return !(this->operator==(rhs));
        }
    };

    Io::base binary() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<0>(0));
    }

    Io::base octal() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<1>(0));
    }

    Io::base decimal() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<2>(0));
    }

    Io::base hexadecimal() {
        return Io::base(juniper::variant<uint8_t, uint8_t, uint8_t, uint8_t>::template create<3>(0));
    }


}

namespace Time {
    using timerState = juniper::records::recordt_1<uint32_t>;


}

namespace Button {
    using buttonState = juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>;


}

namespace Vector {
    template<typename a, int n>
    using vector = juniper::array<a, n>;


}

namespace Color {
    using rgb = juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>;


}

namespace Color {
    using rgba = juniper::records::recordt_4<uint8_t, uint8_t, uint8_t, uint8_t>;


}

namespace Color {
    using hsv = juniper::records::recordt_5<float, float, float>;


}

namespace Color {
    using hsva = juniper::records::recordt_6<float, float, float, float>;


}

namespace NeoPixel {
    struct RawDevice {
        juniper::variant<void *> data;

        RawDevice() {}

        RawDevice(juniper::variant<void *> initData) : data(initData) {}

        void * device() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(RawDevice rhs) {
            return data == rhs.data;
        }

        bool operator!=(RawDevice rhs) {
            return !(this->operator==(rhs));
        }
    };

    NeoPixel::RawDevice device(void * data0) {
        return NeoPixel::RawDevice(juniper::variant<void *>::template create<0>(data0));
    }


}

namespace NeoPixel {
    using DeviceDescriptor = juniper::records::recordt_7<uint16_t>;


}

namespace NeoPixel {
    struct color {
        juniper::variant<juniper::tuple3<uint8_t, uint8_t, uint8_t>> data;

        color() {}

        color(juniper::variant<juniper::tuple3<uint8_t, uint8_t, uint8_t>> initData) : data(initData) {}

        juniper::tuple3<uint8_t, uint8_t, uint8_t> RGB() {
            return data.template get<0>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(color rhs) {
            return data == rhs.data;
        }

        bool operator!=(color rhs) {
            return !(this->operator==(rhs));
        }
    };

    NeoPixel::color RGB(uint8_t data0, uint8_t data1, uint8_t data2) {
        return NeoPixel::color(juniper::variant<juniper::tuple3<uint8_t, uint8_t, uint8_t>>::template create<0>(juniper::tuple3<uint8_t, uint8_t, uint8_t>(data0, data1, data2)));
    }


}

namespace NeoPixel {
    struct Function {
        juniper::variant<int16_t, NeoPixel::color, juniper::tuple2<NeoPixel::color, NeoPixel::color>> data;

        Function() {}

        Function(juniper::variant<int16_t, NeoPixel::color, juniper::tuple2<NeoPixel::color, NeoPixel::color>> initData) : data(initData) {}

        int16_t rotate() {
            return data.template get<0>();
        }

        NeoPixel::color set() {
            return data.template get<1>();
        }

        juniper::tuple2<NeoPixel::color, NeoPixel::color> alternate() {
            return data.template get<2>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(Function rhs) {
            return data == rhs.data;
        }

        bool operator!=(Function rhs) {
            return !(this->operator==(rhs));
        }
    };

    NeoPixel::Function rotate(int16_t data0) {
        return NeoPixel::Function(juniper::variant<int16_t, NeoPixel::color, juniper::tuple2<NeoPixel::color, NeoPixel::color>>::template create<0>(data0));
    }

    NeoPixel::Function set(NeoPixel::color data0) {
        return NeoPixel::Function(juniper::variant<int16_t, NeoPixel::color, juniper::tuple2<NeoPixel::color, NeoPixel::color>>::template create<1>(data0));
    }

    NeoPixel::Function alternate(NeoPixel::color data0, NeoPixel::color data1) {
        return NeoPixel::Function(juniper::variant<int16_t, NeoPixel::color, juniper::tuple2<NeoPixel::color, NeoPixel::color>>::template create<2>(juniper::tuple2<NeoPixel::color, NeoPixel::color>(data0, data1)));
    }


}

namespace NeoPixel {
    struct Action {
        juniper::variant<uint8_t, juniper::tuple2<uint8_t, NeoPixel::Function>, juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>, uint8_t> data;

        Action() {}

        Action(juniper::variant<uint8_t, juniper::tuple2<uint8_t, NeoPixel::Function>, juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>, uint8_t> initData) : data(initData) {}

        uint8_t start() {
            return data.template get<0>();
        }

        juniper::tuple2<uint8_t, NeoPixel::Function> run() {
            return data.template get<1>();
        }

        juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>> repeat() {
            return data.template get<2>();
        }

        uint8_t endRepeat() {
            return data.template get<3>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(Action rhs) {
            return data == rhs.data;
        }

        bool operator!=(Action rhs) {
            return !(this->operator==(rhs));
        }
    };

    NeoPixel::Action start() {
        return NeoPixel::Action(juniper::variant<uint8_t, juniper::tuple2<uint8_t, NeoPixel::Function>, juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>, uint8_t>::template create<0>(0));
    }

    NeoPixel::Action run(uint8_t data0, NeoPixel::Function data1) {
        return NeoPixel::Action(juniper::variant<uint8_t, juniper::tuple2<uint8_t, NeoPixel::Function>, juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>, uint8_t>::template create<1>(juniper::tuple2<uint8_t, NeoPixel::Function>(data0, data1)));
    }

    NeoPixel::Action repeat(uint8_t data0, NeoPixel::Function data1, uint32_t data2, Prelude::maybe<uint8_t> data3) {
        return NeoPixel::Action(juniper::variant<uint8_t, juniper::tuple2<uint8_t, NeoPixel::Function>, juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>, uint8_t>::template create<2>(juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>(data0, data1, data2, data3)));
    }

    NeoPixel::Action endRepeat(uint8_t data0) {
        return NeoPixel::Action(juniper::variant<uint8_t, juniper::tuple2<uint8_t, NeoPixel::Function>, juniper::tuple4<uint8_t, NeoPixel::Function, uint32_t, Prelude::maybe<uint8_t>>, uint8_t>::template create<3>(data0));
    }


}

namespace NeoPixel {
    using Operation = juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>;


}

namespace NeoPixel {
    template<int nPixels>
    using Line = juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>>;


}

namespace NeoPixel {
    template<int nLines, int nPixels>
    using Model = juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>>, nLines>, uint32_t>>;


}

namespace NeoPixel {
    struct Update {
        juniper::variant<NeoPixel::Action, uint8_t> data;

        Update() {}

        Update(juniper::variant<NeoPixel::Action, uint8_t> initData) : data(initData) {}

        NeoPixel::Action action() {
            return data.template get<0>();
        }

        uint8_t operation() {
            return data.template get<1>();
        }

        uint8_t id() {
            return data.id();
        }

        bool operator==(Update rhs) {
            return data == rhs.data;
        }

        bool operator!=(Update rhs) {
            return !(this->operator==(rhs));
        }
    };

    NeoPixel::Update action(NeoPixel::Action data0) {
        return NeoPixel::Update(juniper::variant<NeoPixel::Action, uint8_t>::template create<0>(data0));
    }

    NeoPixel::Update operation() {
        return NeoPixel::Update(juniper::variant<NeoPixel::Action, uint8_t>::template create<1>(0));
    }


}

namespace Prelude {
    void * extractptr(juniper::rcptr p);
}

namespace Prelude {
    juniper::rcptr makerc(void * p, juniper::function<void, juniper::unit(void *)> finalizer);
}

namespace Prelude {
    template<typename t48, typename t49, typename t50, typename t51, typename t53>
    juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)> compose(juniper::function<t51, t49(t48)> f, juniper::function<t50, t48(t53)> g);
}

namespace Prelude {
    template<typename t61>
    t61 id(t61 x);
}

namespace Prelude {
    template<typename t68, typename t69, typename t72, typename t73>
    juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)> curry(juniper::function<t69, t68(t72, t73)> f);
}

namespace Prelude {
    template<typename t84, typename t85, typename t86, typename t88, typename t89>
    juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88, t89)> uncurry(juniper::function<t85, juniper::function<t86, t84(t89)>(t88)> f);
}

namespace Prelude {
    template<typename t104, typename t106, typename t109, typename t110, typename t111>
    juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)> curry3(juniper::function<t106, t104(t109, t110, t111)> f);
}

namespace Prelude {
    template<typename t125, typename t126, typename t127, typename t128, typename t130, typename t131, typename t132>
    juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130, t131, t132)> uncurry3(juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)> f);
}

namespace Prelude {
    template<typename t143>
    bool eq(t143 x, t143 y);
}

namespace Prelude {
    template<typename t148>
    bool neq(t148 x, t148 y);
}

namespace Prelude {
    template<typename t155, typename t156>
    bool gt(t156 x, t155 y);
}

namespace Prelude {
    template<typename t162, typename t163>
    bool geq(t163 x, t162 y);
}

namespace Prelude {
    template<typename t169, typename t170>
    bool lt(t170 x, t169 y);
}

namespace Prelude {
    template<typename t176, typename t177>
    bool leq(t177 x, t176 y);
}

namespace Prelude {
    bool notf(bool x);
}

namespace Prelude {
    bool andf(bool x, bool y);
}

namespace Prelude {
    bool orf(bool x, bool y);
}

namespace Prelude {
    template<typename t200, typename t201, typename t202>
    t201 apply(juniper::function<t202, t201(t200)> f, t200 x);
}

namespace Prelude {
    template<typename t208, typename t209, typename t210, typename t211>
    t210 apply2(juniper::function<t211, t210(t208, t209)> f, juniper::tuple2<t208, t209> tup);
}

namespace Prelude {
    template<typename t222, typename t223, typename t224, typename t225, typename t226>
    t225 apply3(juniper::function<t226, t225(t222, t223, t224)> f, juniper::tuple3<t222, t223, t224> tup);
}

namespace Prelude {
    template<typename t240, typename t241, typename t242, typename t243, typename t244, typename t245>
    t244 apply4(juniper::function<t245, t244(t240, t241, t242, t243)> f, juniper::tuple4<t240, t241, t242, t243> tup);
}

namespace Prelude {
    template<typename t261, typename t262>
    t261 fst(juniper::tuple2<t261, t262> tup);
}

namespace Prelude {
    template<typename t266, typename t267>
    t267 snd(juniper::tuple2<t266, t267> tup);
}

namespace Prelude {
    template<typename t271>
    t271 add(t271 numA, t271 numB);
}

namespace Prelude {
    template<typename t273>
    t273 sub(t273 numA, t273 numB);
}

namespace Prelude {
    template<typename t275>
    t275 mul(t275 numA, t275 numB);
}

namespace Prelude {
    template<typename t277>
    t277 div(t277 numA, t277 numB);
}

namespace Prelude {
    template<typename t283, typename t284>
    juniper::tuple2<t284, t283> swap(juniper::tuple2<t283, t284> tup);
}

namespace Prelude {
    template<typename t290, typename t291, typename t292>
    t290 until(juniper::function<t292, bool(t290)> p, juniper::function<t291, t290(t290)> f, t290 a0);
}

namespace Prelude {
    template<typename t301>
    juniper::unit ignore(t301 val);
}

namespace Prelude {
    template<typename t304>
    juniper::unit clear(t304& val);
}

namespace Prelude {
    template<typename t306, int c7>
    juniper::array<t306, c7> array(t306 elem);
}

namespace Prelude {
    template<typename t312, int c9>
    juniper::array<t312, c9> zeros();
}

namespace Prelude {
    uint16_t u8ToU16(uint8_t n);
}

namespace Prelude {
    uint32_t u8ToU32(uint8_t n);
}

namespace Prelude {
    int8_t u8ToI8(uint8_t n);
}

namespace Prelude {
    int16_t u8ToI16(uint8_t n);
}

namespace Prelude {
    int32_t u8ToI32(uint8_t n);
}

namespace Prelude {
    float u8ToFloat(uint8_t n);
}

namespace Prelude {
    double u8ToDouble(uint8_t n);
}

namespace Prelude {
    uint8_t u16ToU8(uint16_t n);
}

namespace Prelude {
    uint32_t u16ToU32(uint16_t n);
}

namespace Prelude {
    int8_t u16ToI8(uint16_t n);
}

namespace Prelude {
    int16_t u16ToI16(uint16_t n);
}

namespace Prelude {
    int32_t u16ToI32(uint16_t n);
}

namespace Prelude {
    float u16ToFloat(uint16_t n);
}

namespace Prelude {
    double u16ToDouble(uint16_t n);
}

namespace Prelude {
    uint8_t u32ToU8(uint32_t n);
}

namespace Prelude {
    uint16_t u32ToU16(uint32_t n);
}

namespace Prelude {
    int8_t u32ToI8(uint32_t n);
}

namespace Prelude {
    int16_t u32ToI16(uint32_t n);
}

namespace Prelude {
    int32_t u32ToI32(uint32_t n);
}

namespace Prelude {
    float u32ToFloat(uint32_t n);
}

namespace Prelude {
    double u32ToDouble(uint32_t n);
}

namespace Prelude {
    uint8_t i8ToU8(int8_t n);
}

namespace Prelude {
    uint16_t i8ToU16(int8_t n);
}

namespace Prelude {
    uint32_t i8ToU32(int8_t n);
}

namespace Prelude {
    int16_t i8ToI16(int8_t n);
}

namespace Prelude {
    int32_t i8ToI32(int8_t n);
}

namespace Prelude {
    float i8ToFloat(int8_t n);
}

namespace Prelude {
    double i8ToDouble(int8_t n);
}

namespace Prelude {
    uint8_t i16ToU8(int16_t n);
}

namespace Prelude {
    uint16_t i16ToU16(int16_t n);
}

namespace Prelude {
    uint32_t i16ToU32(int16_t n);
}

namespace Prelude {
    int8_t i16ToI8(int16_t n);
}

namespace Prelude {
    int32_t i16ToI32(int16_t n);
}

namespace Prelude {
    float i16ToFloat(int16_t n);
}

namespace Prelude {
    double i16ToDouble(int16_t n);
}

namespace Prelude {
    uint8_t i32ToU8(int32_t n);
}

namespace Prelude {
    uint16_t i32ToU16(int32_t n);
}

namespace Prelude {
    uint32_t i32ToU32(int32_t n);
}

namespace Prelude {
    int8_t i32ToI8(int32_t n);
}

namespace Prelude {
    int16_t i32ToI16(int32_t n);
}

namespace Prelude {
    float i32ToFloat(int32_t n);
}

namespace Prelude {
    double i32ToDouble(int32_t n);
}

namespace Prelude {
    uint8_t floatToU8(float n);
}

namespace Prelude {
    uint16_t floatToU16(float n);
}

namespace Prelude {
    uint32_t floatToU32(float n);
}

namespace Prelude {
    int8_t floatToI8(float n);
}

namespace Prelude {
    int16_t floatToI16(float n);
}

namespace Prelude {
    int32_t floatToI32(float n);
}

namespace Prelude {
    double floatToDouble(float n);
}

namespace Prelude {
    uint8_t doubleToU8(double n);
}

namespace Prelude {
    uint16_t doubleToU16(double n);
}

namespace Prelude {
    uint32_t doubleToU32(double n);
}

namespace Prelude {
    int8_t doubleToI8(double n);
}

namespace Prelude {
    int16_t doubleToI16(double n);
}

namespace Prelude {
    int32_t doubleToI32(double n);
}

namespace Prelude {
    float doubleToFloat(double n);
}

namespace Prelude {
    template<typename t371>
    uint8_t toUInt8(t371 n);
}

namespace Prelude {
    template<typename t373>
    int8_t toInt8(t373 n);
}

namespace Prelude {
    template<typename t375>
    uint16_t toUInt16(t375 n);
}

namespace Prelude {
    template<typename t377>
    int16_t toInt16(t377 n);
}

namespace Prelude {
    template<typename t379>
    uint32_t toUInt32(t379 n);
}

namespace Prelude {
    template<typename t381>
    int32_t toInt32(t381 n);
}

namespace Prelude {
    template<typename t383>
    float toFloat(t383 n);
}

namespace Prelude {
    template<typename t385>
    double toDouble(t385 n);
}

namespace Prelude {
    template<typename t387>
    t387 fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t389>
    t389 fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t391>
    t391 fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t393>
    t393 fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t395>
    t395 fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t397>
    t397 fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t399>
    t399 fromFloat(float n);
}

namespace Prelude {
    template<typename t401>
    t401 fromDouble(double n);
}

namespace Prelude {
    template<typename t403, typename t404>
    t404 cast(t403 x);
}

namespace List {
    template<typename t407, int c10>
    juniper::records::recordt_0<juniper::array<t407, c10>, uint32_t> empty();
}

namespace List {
    template<typename t414, typename t415, typename t421, int c12>
    juniper::records::recordt_0<juniper::array<t414, c12>, uint32_t> map(juniper::function<t415, t414(t421)> f, juniper::records::recordt_0<juniper::array<t421, c12>, uint32_t> lst);
}

namespace List {
    template<typename t429, typename t431, typename t434, int c16>
    t429 fold(juniper::function<t431, t429(t434, t429)> f, t429 initState, juniper::records::recordt_0<juniper::array<t434, c16>, uint32_t> lst);
}

namespace List {
    template<typename t441, typename t443, typename t449, int c18>
    t441 foldBack(juniper::function<t443, t441(t449, t441)> f, t441 initState, juniper::records::recordt_0<juniper::array<t449, c18>, uint32_t> lst);
}

namespace List {
    template<typename t457, typename t459, int c20>
    t459 reduce(juniper::function<t457, t459(t459, t459)> f, juniper::records::recordt_0<juniper::array<t459, c20>, uint32_t> lst);
}

namespace List {
    template<typename t470, typename t471, int c23>
    Prelude::maybe<t470> tryReduce(juniper::function<t471, t470(t470, t470)> f, juniper::records::recordt_0<juniper::array<t470, c23>, uint32_t> lst);
}

namespace List {
    template<typename t514, typename t518, int c25>
    t518 reduceBack(juniper::function<t514, t518(t518, t518)> f, juniper::records::recordt_0<juniper::array<t518, c25>, uint32_t> lst);
}

namespace List {
    template<typename t530, typename t531, int c28>
    Prelude::maybe<t530> tryReduceBack(juniper::function<t531, t530(t530, t530)> f, juniper::records::recordt_0<juniper::array<t530, c28>, uint32_t> lst);
}

namespace List {
    template<typename t586, int c30, int c31, int c32>
    juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> concat(juniper::records::recordt_0<juniper::array<t586, c30>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t586, c31>, uint32_t> lstB);
}

namespace List {
    template<typename t591, int c38, int c39>
    juniper::records::recordt_0<juniper::array<t591, (((c38)*(-1))+((c39)*(-1)))*(-1)>, uint32_t> concatSafe(juniper::records::recordt_0<juniper::array<t591, c38>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t591, c39>, uint32_t> lstB);
}

namespace List {
    template<typename t599, typename t601, int c43>
    t601 get(t599 i, juniper::records::recordt_0<juniper::array<t601, c43>, uint32_t> lst);
}

namespace List {
    template<typename t604, typename t612, int c45>
    Prelude::maybe<t612> tryGet(t604 i, juniper::records::recordt_0<juniper::array<t612, c45>, uint32_t> lst);
}

namespace List {
    template<typename t645, int c47, int c48, int c49>
    juniper::records::recordt_0<juniper::array<t645, c49>, uint32_t> flatten(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t645, c47>, uint32_t>, c48>, uint32_t> listOfLists);
}

namespace List {
    template<typename t649, int c55, int c56>
    juniper::records::recordt_0<juniper::array<t649, (c56)*(c55)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t649, c55>, uint32_t>, c56>, uint32_t> listOfLists);
}

namespace Math {
    template<typename t655>
    t655 min_(t655 x, t655 y);
}

namespace List {
    template<typename t672, int c60, int c61>
    juniper::records::recordt_0<juniper::array<t672, c60>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t672, c61>, uint32_t> lst);
}

namespace List {
    template<typename t678, typename t681, int c65>
    bool all(juniper::function<t678, bool(t681)> pred, juniper::records::recordt_0<juniper::array<t681, c65>, uint32_t> lst);
}

namespace List {
    template<typename t688, typename t691, int c67>
    bool any(juniper::function<t688, bool(t691)> pred, juniper::records::recordt_0<juniper::array<t691, c67>, uint32_t> lst);
}

namespace List {
    template<typename t696, int c69>
    juniper::unit append(t696 elem, juniper::records::recordt_0<juniper::array<t696, c69>, uint32_t>& lst);
}

namespace List {
    template<typename t704, int c71>
    juniper::records::recordt_0<juniper::array<t704, c71>, uint32_t> appendPure(t704 elem, juniper::records::recordt_0<juniper::array<t704, c71>, uint32_t> lst);
}

namespace List {
    template<typename t712, int c73>
    juniper::records::recordt_0<juniper::array<t712, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> appendSafe(t712 elem, juniper::records::recordt_0<juniper::array<t712, c73>, uint32_t> lst);
}

namespace List {
    template<typename t727, int c77>
    juniper::unit prepend(t727 elem, juniper::records::recordt_0<juniper::array<t727, c77>, uint32_t>& lst);
}

namespace List {
    template<typename t744, int c81>
    juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> prependPure(t744 elem, juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> lst);
}

namespace List {
    template<typename t759, int c85>
    juniper::unit set(uint32_t index, t759 elem, juniper::records::recordt_0<juniper::array<t759, c85>, uint32_t>& lst);
}

namespace List {
    template<typename t764, int c87>
    juniper::records::recordt_0<juniper::array<t764, c87>, uint32_t> setPure(uint32_t index, t764 elem, juniper::records::recordt_0<juniper::array<t764, c87>, uint32_t> lst);
}

namespace List {
    template<typename t770, int c89>
    juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> replicate(uint32_t numOfElements, t770 elem);
}

namespace List {
    template<typename t791, int c92>
    juniper::unit remove(t791 elem, juniper::records::recordt_0<juniper::array<t791, c92>, uint32_t>& lst);
}

namespace List {
    template<typename t798, int c97>
    juniper::records::recordt_0<juniper::array<t798, c97>, uint32_t> removePure(t798 elem, juniper::records::recordt_0<juniper::array<t798, c97>, uint32_t> lst);
}

namespace List {
    template<typename t810, int c99>
    juniper::unit pop(juniper::records::recordt_0<juniper::array<t810, c99>, uint32_t>& lst);
}

namespace List {
    template<typename t817, int c101>
    juniper::records::recordt_0<juniper::array<t817, c101>, uint32_t> popPure(juniper::records::recordt_0<juniper::array<t817, c101>, uint32_t> lst);
}

namespace List {
    template<typename t825, typename t828, int c103>
    juniper::unit iter(juniper::function<t825, juniper::unit(t828)> f, juniper::records::recordt_0<juniper::array<t828, c103>, uint32_t> lst);
}

namespace List {
    template<typename t837, int c105>
    t837 last(juniper::records::recordt_0<juniper::array<t837, c105>, uint32_t> lst);
}

namespace List {
    template<typename t849, int c107>
    Prelude::maybe<t849> tryLast(juniper::records::recordt_0<juniper::array<t849, c107>, uint32_t> lst);
}

namespace List {
    template<typename t885, int c109>
    Prelude::maybe<t885> tryMax(juniper::records::recordt_0<juniper::array<t885, c109>, uint32_t> lst);
}

namespace List {
    template<typename t924, int c113>
    Prelude::maybe<t924> tryMin(juniper::records::recordt_0<juniper::array<t924, c113>, uint32_t> lst);
}

namespace List {
    template<typename t954, int c117>
    bool member(t954 elem, juniper::records::recordt_0<juniper::array<t954, c117>, uint32_t> lst);
}

namespace List {
    template<typename t972, typename t974, int c119>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t972, c119>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t974, c119>, uint32_t> lstB);
}

namespace List {
    template<typename t991, typename t992, int c124>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t991, t992>, c124>, uint32_t> lst);
}

namespace List {
    template<typename t1000, int c130>
    t1000 sum(juniper::records::recordt_0<juniper::array<t1000, c130>, uint32_t> lst);
}

namespace List {
    template<typename t1018, int c132>
    t1018 average(juniper::records::recordt_0<juniper::array<t1018, c132>, uint32_t> lst);
}

namespace List {
    uint32_t iLeftChild(uint32_t i);
}

namespace List {
    uint32_t iRightChild(uint32_t i);
}

namespace List {
    uint32_t iParent(uint32_t i);
}

namespace List {
    template<typename t1043, typename t1045, typename t1047, typename t1078, int c134>
    juniper::unit siftDown(juniper::records::recordt_0<juniper::array<t1078, c134>, uint32_t>& lst, juniper::function<t1047, t1043(t1078)> key, uint32_t root, t1045 end);
}

namespace List {
    template<typename t1089, typename t1098, typename t1100, int c143>
    juniper::unit heapify(juniper::records::recordt_0<juniper::array<t1089, c143>, uint32_t>& lst, juniper::function<t1100, t1098(t1089)> key);
}

namespace List {
    template<typename t1111, typename t1113, typename t1126, int c145>
    juniper::unit sort(juniper::function<t1113, t1111(t1126)> key, juniper::records::recordt_0<juniper::array<t1126, c145>, uint32_t>& lst);
}

namespace List {
    template<typename t1144, typename t1145, typename t1146, int c152>
    juniper::records::recordt_0<juniper::array<t1145, c152>, uint32_t> sorted(juniper::function<t1146, t1144(t1145)> key, juniper::records::recordt_0<juniper::array<t1145, c152>, uint32_t> lst);
}

namespace Signal {
    template<typename t1156, typename t1158, typename t1173>
    Prelude::sig<t1173> map(juniper::function<t1158, t1173(t1156)> f, Prelude::sig<t1156> s);
}

namespace Signal {
    template<typename t1242, typename t1243>
    juniper::unit sink(juniper::function<t1243, juniper::unit(t1242)> f, Prelude::sig<t1242> s);
}

namespace Signal {
    template<typename t1266, typename t1280>
    Prelude::sig<t1280> filter(juniper::function<t1266, bool(t1280)> f, Prelude::sig<t1280> s);
}

namespace Signal {
    template<typename t1360>
    Prelude::sig<t1360> merge(Prelude::sig<t1360> sigA, Prelude::sig<t1360> sigB);
}

namespace Signal {
    template<typename t1396, int c154>
    Prelude::sig<t1396> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t1396>, c154>, uint32_t> sigs);
}

namespace Signal {
    template<typename t1514, typename t1538>
    Prelude::sig<Prelude::either<t1538, t1514>> join(Prelude::sig<t1538> sigA, Prelude::sig<t1514> sigB);
}

namespace Signal {
    template<typename t1827>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t1827> s);
}

namespace Signal {
    template<typename t1861, typename t1863, typename t1879>
    Prelude::sig<t1879> foldP(juniper::function<t1863, t1879(t1861, t1879)> f, t1879& state0, Prelude::sig<t1861> incoming);
}

namespace Signal {
    template<typename t1956>
    Prelude::sig<t1956> dropRepeats(Prelude::maybe<t1956>& maybePrevValue, Prelude::sig<t1956> incoming);
}

namespace Signal {
    template<typename t2055>
    Prelude::sig<t2055> latch(t2055& prevValue, Prelude::sig<t2055> incoming);
}

namespace Signal {
    template<typename t2115, typename t2116, typename t2119, typename t2127>
    Prelude::sig<t2115> map2(juniper::function<t2116, t2115(t2119, t2127)> f, juniper::tuple2<t2119, t2127>& state, Prelude::sig<t2119> incomingA, Prelude::sig<t2127> incomingB);
}

namespace Signal {
    template<typename t2269, int c156>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t2269, c156>, uint32_t>> record(juniper::records::recordt_0<juniper::array<t2269, c156>, uint32_t>& pastValues, Prelude::sig<t2269> incoming);
}

namespace Signal {
    template<typename t2307>
    Prelude::sig<t2307> constant(t2307 val);
}

namespace Signal {
    template<typename t2342>
    Prelude::sig<Prelude::maybe<t2342>> meta(Prelude::sig<t2342> sigA);
}

namespace Signal {
    template<typename t2409>
    Prelude::sig<t2409> unmeta(Prelude::sig<Prelude::maybe<t2409>> sigA);
}

namespace Signal {
    template<typename t2485, typename t2486>
    Prelude::sig<juniper::tuple2<t2485, t2486>> zip(juniper::tuple2<t2485, t2486>& state, Prelude::sig<t2485> sigA, Prelude::sig<t2486> sigB);
}

namespace Signal {
    template<typename t2556, typename t2563>
    juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>> unzip(Prelude::sig<juniper::tuple2<t2556, t2563>> incoming);
}

namespace Signal {
    template<typename t2690, typename t2691>
    Prelude::sig<t2690> toggle(t2690 val1, t2690 val2, t2690& state, Prelude::sig<t2691> incoming);
}

namespace Io {
    Io::pinState toggle(Io::pinState p);
}

namespace Io {
    juniper::unit printStr(const char * str);
}

namespace Io {
    template<int c158>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c158>, uint32_t> cl);
}

namespace Io {
    juniper::unit printFloat(float f);
}

namespace Io {
    juniper::unit printInt(int32_t n);
}

namespace Io {
    template<typename t2746>
    t2746 baseToInt(Io::base b);
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b);
}

namespace Io {
    juniper::unit printFloatPlaces(float f, int32_t numPlaces);
}

namespace Io {
    juniper::unit beginSerial(uint32_t speed);
}

namespace Io {
    uint8_t pinStateToInt(Io::pinState value);
}

namespace Io {
    Io::pinState intToPinState(uint8_t value);
}

namespace Io {
    juniper::unit digWrite(uint16_t pin, Io::pinState value);
}

namespace Io {
    Io::pinState digRead(uint16_t pin);
}

namespace Io {
    Prelude::sig<Io::pinState> digIn(uint16_t pin);
}

namespace Io {
    juniper::unit digOut(uint16_t pin, Prelude::sig<Io::pinState> sig);
}

namespace Io {
    uint16_t anaRead(uint16_t pin);
}

namespace Io {
    juniper::unit anaWrite(uint16_t pin, uint8_t value);
}

namespace Io {
    Prelude::sig<uint16_t> anaIn(uint16_t pin);
}

namespace Io {
    juniper::unit anaOut(uint16_t pin, Prelude::sig<uint8_t> sig);
}

namespace Io {
    uint8_t pinModeToInt(Io::mode m);
}

namespace Io {
    Io::mode intToPinMode(uint8_t m);
}

namespace Io {
    juniper::unit setPinMode(uint16_t pin, Io::mode m);
}

namespace Io {
    Prelude::sig<juniper::unit> risingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState);
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState);
}

namespace Io {
    Prelude::sig<juniper::unit> edge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState);
}

namespace Maybe {
    template<typename t3200, typename t3202, typename t3211>
    Prelude::maybe<t3211> map(juniper::function<t3202, t3211(t3200)> f, Prelude::maybe<t3200> maybeVal);
}

namespace Maybe {
    template<typename t3241>
    t3241 get(Prelude::maybe<t3241> maybeVal);
}

namespace Maybe {
    template<typename t3250>
    bool isJust(Prelude::maybe<t3250> maybeVal);
}

namespace Maybe {
    template<typename t3261>
    bool isNothing(Prelude::maybe<t3261> maybeVal);
}

namespace Maybe {
    template<typename t3275>
    uint8_t count(Prelude::maybe<t3275> maybeVal);
}

namespace Maybe {
    template<typename t3289, typename t3290, typename t3291>
    t3289 fold(juniper::function<t3291, t3289(t3290, t3289)> f, t3289 initState, Prelude::maybe<t3290> maybeVal);
}

namespace Maybe {
    template<typename t3307, typename t3308>
    juniper::unit iter(juniper::function<t3308, juniper::unit(t3307)> f, Prelude::maybe<t3307> maybeVal);
}

namespace Time {
    juniper::unit wait(uint32_t time);
}

namespace Time {
    uint32_t now();
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::records::recordt_1<uint32_t>& state);
}

namespace Math {
    double degToRad(double degrees);
}

namespace Math {
    double radToDeg(double radians);
}

namespace Math {
    double acos_(double x);
}

namespace Math {
    double asin_(double x);
}

namespace Math {
    double atan_(double x);
}

namespace Math {
    double atan2_(double y, double x);
}

namespace Math {
    double cos_(double x);
}

namespace Math {
    double cosh_(double x);
}

namespace Math {
    double sin_(double x);
}

namespace Math {
    double sinh_(double x);
}

namespace Math {
    double tan_(double x);
}

namespace Math {
    double tanh_(double x);
}

namespace Math {
    double exp_(double x);
}

namespace Math {
    juniper::tuple2<double, int32_t> frexp_(double x);
}

namespace Math {
    double ldexp_(double x, int16_t exponent);
}

namespace Math {
    double log_(double x);
}

namespace Math {
    double log10_(double x);
}

namespace Math {
    juniper::tuple2<double, double> modf_(double x);
}

namespace Math {
    double pow_(double x, double y);
}

namespace Math {
    double sqrt_(double x);
}

namespace Math {
    double ceil_(double x);
}

namespace Math {
    double fabs_(double x);
}

namespace Math {
    template<typename t3452>
    t3452 abs_(t3452 x);
}

namespace Math {
    double floor_(double x);
}

namespace Math {
    double fmod_(double x, double y);
}

namespace Math {
    double round_(double x);
}

namespace Math {
    template<typename t3463>
    t3463 max_(t3463 x, t3463 y);
}

namespace Math {
    template<typename t3465>
    t3465 mapRange(t3465 x, t3465 a1, t3465 a2, t3465 b1, t3465 b2);
}

namespace Math {
    template<typename t3467>
    t3467 clamp(t3467 x, t3467 min, t3467 max);
}

namespace Math {
    template<typename t3472>
    int8_t sign(t3472 n);
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState);
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState);
}

namespace Vector {
    template<typename t3592, int c159>
    juniper::array<t3592, c159> add(juniper::array<t3592, c159> v1, juniper::array<t3592, c159> v2);
}

namespace Vector {
    template<typename t3596, int c162>
    juniper::array<t3596, c162> zero();
}

namespace Vector {
    template<typename t3604, int c164>
    juniper::array<t3604, c164> subtract(juniper::array<t3604, c164> v1, juniper::array<t3604, c164> v2);
}

namespace Vector {
    template<typename t3608, int c167>
    juniper::array<t3608, c167> scale(t3608 scalar, juniper::array<t3608, c167> v);
}

namespace Vector {
    template<typename t3614, int c169>
    t3614 dot(juniper::array<t3614, c169> v1, juniper::array<t3614, c169> v2);
}

namespace Vector {
    template<typename t3620, int c172>
    t3620 magnitude2(juniper::array<t3620, c172> v);
}

namespace Vector {
    template<typename t3622, int c175>
    double magnitude(juniper::array<t3622, c175> v);
}

namespace Vector {
    template<typename t3638, int c177>
    juniper::array<t3638, c177> multiply(juniper::array<t3638, c177> u, juniper::array<t3638, c177> v);
}

namespace Vector {
    template<typename t3647, int c180>
    juniper::array<t3647, c180> normalize(juniper::array<t3647, c180> v);
}

namespace Vector {
    template<typename t3658, int c184>
    double angle(juniper::array<t3658, c184> v1, juniper::array<t3658, c184> v2);
}

namespace Vector {
    template<typename t3700>
    juniper::array<t3700, 3> cross(juniper::array<t3700, 3> u, juniper::array<t3700, 3> v);
}

namespace Vector {
    template<typename t3702, int c200>
    juniper::array<t3702, c200> project(juniper::array<t3702, c200> a, juniper::array<t3702, c200> b);
}

namespace Vector {
    template<typename t3718, int c204>
    juniper::array<t3718, c204> projectPlane(juniper::array<t3718, c204> a, juniper::array<t3718, c204> m);
}

namespace CharList {
    template<int c207>
    juniper::records::recordt_0<juniper::array<uint8_t, c207>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c207>, uint32_t> str);
}

namespace CharList {
    template<int c208>
    juniper::records::recordt_0<juniper::array<uint8_t, c208>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c208>, uint32_t> str);
}

namespace CharList {
    template<int c209>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> i32ToCharList(int32_t m);
}

namespace CharList {
    template<int c211>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c211>, uint32_t> s);
}

namespace CharList {
    template<int c212, int c213, int c214>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c212)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c213)>, uint32_t> sB);
}

namespace CharList {
    template<int c222, int c223>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c222)*(-1)))+((c223)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c222)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c223)>, uint32_t> sB);
}

namespace Random {
    int32_t random_(int32_t low, int32_t high);
}

namespace Random {
    juniper::unit seed(uint32_t n);
}

namespace Random {
    template<typename t3796, int c227>
    t3796 choice(juniper::records::recordt_0<juniper::array<t3796, c227>, uint32_t> lst);
}

namespace Random {
    template<typename t3816, int c229>
    Prelude::maybe<t3816> tryChoice(juniper::records::recordt_0<juniper::array<t3816, c229>, uint32_t> lst);
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color);
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color);
}

namespace ListExt {
    template<typename t3947, int c231>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t3947>, c231>, uint32_t> enumerated(juniper::records::recordt_0<juniper::array<t3947, c231>, uint32_t> lst);
}

namespace ListExt {
    template<typename t3991, int c235>
    juniper::unit rotate(int32_t step, juniper::records::recordt_0<juniper::array<t3991, c235>, uint32_t>& lst);
}

namespace ListExt {
    template<typename t4028, int c239>
    juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> rotated(int32_t step, juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> lst);
}

namespace ListExt {
    template<typename t4049, int c243, int c244>
    juniper::records::recordt_0<juniper::array<t4049, c243>, uint32_t> replicateList(uint32_t nElements, juniper::records::recordt_0<juniper::array<t4049, c244>, uint32_t> elements);
}

namespace SignalExt {
    template<typename t4067>
    Prelude::sig<t4067> once(Prelude::maybe<t4067>& state);
}

namespace NeoPixel {
    Prelude::sig<Prelude::maybe<NeoPixel::Action>> actions(Prelude::maybe<NeoPixel::Action>& prevAction);
}

namespace NeoPixel {
    NeoPixel::RawDevice makeDevice(uint16_t pin, uint16_t pixels);
}

namespace NeoPixel {
    NeoPixel::color getPixelColor(uint16_t n, NeoPixel::RawDevice line);
}

namespace NeoPixel {
    template<int c249>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c249>, uint32_t> readPixels(NeoPixel::RawDevice device);
}

namespace NeoPixel {
    template<int c252, int c253>
    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, c253>, uint32_t>> initialState(juniper::array<juniper::records::recordt_7<uint16_t>, c253> descriptors, uint16_t nPixels);
}

namespace NeoPixel {
    template<int c257>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> applyFunction(NeoPixel::Function fn, juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> pixels);
}

namespace NeoPixel {
    template<int c261>
    juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c261>, uint32_t> diffPixels(juniper::records::recordt_0<juniper::array<NeoPixel::color, c261>, uint32_t> current, juniper::records::recordt_0<juniper::array<NeoPixel::color, c261>, uint32_t> next);
}

namespace NeoPixel {
    juniper::unit setPixelColor(uint16_t n, NeoPixel::color color, NeoPixel::RawDevice line);
}

namespace NeoPixel {
    juniper::unit show(NeoPixel::RawDevice line);
}

namespace NeoPixel {
    template<int c264>
    juniper::unit updatePixels(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>> state);
}

namespace NeoPixel {
    template<int c268>
    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> updateLine(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> line, NeoPixel::Function fn);
}

namespace NeoPixel {
    template<int c271>
    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> runOperation(juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> line);
}

namespace NeoPixel {
    template<int c273>
    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> updateOperation(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> line);
}

namespace NeoPixel {
    juniper::unit begin(NeoPixel::RawDevice line);
}

namespace NeoPixel {
    template<int c276, int c277>
    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> update(Prelude::maybe<NeoPixel::Action> act, juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> model);
}

namespace NeoPixel {
    juniper::unit setBrightness(uint8_t level, NeoPixel::RawDevice line);
}

namespace NeoPixel {
    uint8_t getBrightness(NeoPixel::RawDevice line);
}

namespace NeoPixel {
    juniper::unit clear(NeoPixel::RawDevice line);
}

namespace NeoPixel {
    bool canShow(NeoPixel::RawDevice line);
}

namespace TEA {
    juniper::unit setup();
}

namespace TEA {
    Prelude::sig<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>>> loop();
}

namespace Time {
    extern juniper::records::recordt_1<uint32_t> state;
}

namespace Math {
    extern double pi;
}

namespace Math {
    extern double e;
}

namespace Button {
    extern juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> state;
}

namespace Vector {
    extern uint8_t x;
}

namespace Vector {
    extern uint8_t y;
}

namespace Vector {
    extern uint8_t z;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> red;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> green;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> blue;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> black;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> white;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> yellow;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> magenta;
}

namespace Color {
    extern juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> cyan;
}

namespace NeoPixel {
    extern Prelude::maybe<NeoPixel::Action> startAction;
}

namespace NeoPixel {
    extern Prelude::maybe<NeoPixel::Action> firstAction;
}

namespace TEA {
    extern juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>> state;
}

namespace TEA {
    extern Prelude::maybe<NeoPixel::Action> previousAction;
}

namespace Prelude {
    void * extractptr(juniper::rcptr p) {
        return (([&]() -> void * {
            void * ret;
            
            (([&]() -> juniper::unit {
                ret = *p.get();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    juniper::rcptr makerc(void * p, juniper::function<void, juniper::unit(void *)> finalizer) {
        return (([&]() -> juniper::rcptr {
            juniper::rcptr ret;
            
            (([&]() -> juniper::unit {
                ret = juniper::make_rcptr(p, finalizer);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t48, typename t49, typename t50, typename t51, typename t53>
    juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)> compose(juniper::function<t51, t49(t48)> f, juniper::function<t50, t48(t53)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)> {
            using a = t53;
            using b = t48;
            using c = t49;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>, t49(t53)>(juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t51, t49(t48)>, juniper::function<t50, t48(t53)>>& junclosure, t53 x) -> t49 { 
                juniper::function<t51, t49(t48)>& f = junclosure.f;
                juniper::function<t50, t48(t53)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t61>
    t61 id(t61 x) {
        return (([&]() -> t61 {
            using a = t61;
            return x;
        })());
    }
}

namespace Prelude {
    template<typename t68, typename t69, typename t72, typename t73>
    juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)> curry(juniper::function<t69, t68(t72, t73)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)> {
            using a = t72;
            using b = t73;
            using c = t68;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(t72)>(juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>(f), [](juniper::closures::closuret_1<juniper::function<t69, t68(t72, t73)>>& junclosure, t72 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)> { 
                juniper::function<t69, t68(t72, t73)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>, t68(t73)>(juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t69, t68(t72, t73)>, t72>& junclosure, t73 valueB) -> t68 { 
                    juniper::function<t69, t68(t72, t73)>& f = junclosure.f;
                    t72& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t84, typename t85, typename t86, typename t88, typename t89>
    juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88, t89)> uncurry(juniper::function<t85, juniper::function<t86, t84(t89)>(t88)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88, t89)> {
            using a = t88;
            using b = t89;
            using c = t84;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>, t84(t88,t89)>(juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>(f), [](juniper::closures::closuret_1<juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>>& junclosure, t88 valueA, t89 valueB) -> t84 { 
                juniper::function<t85, juniper::function<t86, t84(t89)>(t88)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t104, typename t106, typename t109, typename t110, typename t111>
    juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)> curry3(juniper::function<t106, t104(t109, t110, t111)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)> {
            using a = t109;
            using b = t110;
            using c = t111;
            using d = t104;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(t109)>(juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>(f), [](juniper::closures::closuret_1<juniper::function<t106, t104(t109, t110, t111)>>& junclosure, t109 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)> { 
                juniper::function<t106, t104(t109, t110, t111)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>, juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(t110)>(juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t106, t104(t109, t110, t111)>, t109>& junclosure, t110 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)> { 
                    juniper::function<t106, t104(t109, t110, t111)>& f = junclosure.f;
                    t109& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>, t104(t111)>(juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t106, t104(t109, t110, t111)>, t109, t110>& junclosure, t111 valueC) -> t104 { 
                        juniper::function<t106, t104(t109, t110, t111)>& f = junclosure.f;
                        t109& valueA = junclosure.valueA;
                        t110& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t125, typename t126, typename t127, typename t128, typename t130, typename t131, typename t132>
    juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130, t131, t132)> uncurry3(juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130, t131, t132)> {
            using a = t130;
            using b = t131;
            using c = t132;
            using d = t125;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>, t125(t130,t131,t132)>(juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>(f), [](juniper::closures::closuret_1<juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>>& junclosure, t130 valueA, t131 valueB, t132 valueC) -> t125 { 
                juniper::function<t126, juniper::function<t127, juniper::function<t128, t125(t132)>(t131)>(t130)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t143>
    bool eq(t143 x, t143 y) {
        return (([&]() -> bool {
            using a = t143;
            return ((bool) (x == y));
        })());
    }
}

namespace Prelude {
    template<typename t148>
    bool neq(t148 x, t148 y) {
        return ((bool) (x != y));
    }
}

namespace Prelude {
    template<typename t155, typename t156>
    bool gt(t156 x, t155 y) {
        return ((bool) (x > y));
    }
}

namespace Prelude {
    template<typename t162, typename t163>
    bool geq(t163 x, t162 y) {
        return ((bool) (x >= y));
    }
}

namespace Prelude {
    template<typename t169, typename t170>
    bool lt(t170 x, t169 y) {
        return ((bool) (x < y));
    }
}

namespace Prelude {
    template<typename t176, typename t177>
    bool leq(t177 x, t176 y) {
        return ((bool) (x <= y));
    }
}

namespace Prelude {
    bool notf(bool x) {
        return !(x);
    }
}

namespace Prelude {
    bool andf(bool x, bool y) {
        return ((bool) (x && y));
    }
}

namespace Prelude {
    bool orf(bool x, bool y) {
        return ((bool) (x || y));
    }
}

namespace Prelude {
    template<typename t200, typename t201, typename t202>
    t201 apply(juniper::function<t202, t201(t200)> f, t200 x) {
        return (([&]() -> t201 {
            using a = t200;
            using b = t201;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t208, typename t209, typename t210, typename t211>
    t210 apply2(juniper::function<t211, t210(t208, t209)> f, juniper::tuple2<t208, t209> tup) {
        return (([&]() -> t210 {
            using a = t208;
            using b = t209;
            using c = t210;
            return (([&]() -> t210 {
                juniper::tuple2<t208, t209> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t209 b = (guid0).e2;
                t208 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t222, typename t223, typename t224, typename t225, typename t226>
    t225 apply3(juniper::function<t226, t225(t222, t223, t224)> f, juniper::tuple3<t222, t223, t224> tup) {
        return (([&]() -> t225 {
            using a = t222;
            using b = t223;
            using c = t224;
            using d = t225;
            return (([&]() -> t225 {
                juniper::tuple3<t222, t223, t224> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t224 c = (guid1).e3;
                t223 b = (guid1).e2;
                t222 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t240, typename t241, typename t242, typename t243, typename t244, typename t245>
    t244 apply4(juniper::function<t245, t244(t240, t241, t242, t243)> f, juniper::tuple4<t240, t241, t242, t243> tup) {
        return (([&]() -> t244 {
            using a = t240;
            using b = t241;
            using c = t242;
            using d = t243;
            using e = t244;
            return (([&]() -> t244 {
                juniper::tuple4<t240, t241, t242, t243> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t243 d = (guid2).e4;
                t242 c = (guid2).e3;
                t241 b = (guid2).e2;
                t240 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t261, typename t262>
    t261 fst(juniper::tuple2<t261, t262> tup) {
        return (([&]() -> t261 {
            using a = t261;
            using b = t262;
            return (([&]() -> t261 {
                juniper::tuple2<t261, t262> guid3 = tup;
                return (true ? 
                    (([&]() -> t261 {
                        t261 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t261>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t266, typename t267>
    t267 snd(juniper::tuple2<t266, t267> tup) {
        return (([&]() -> t267 {
            using a = t266;
            using b = t267;
            return (([&]() -> t267 {
                juniper::tuple2<t266, t267> guid4 = tup;
                return (true ? 
                    (([&]() -> t267 {
                        t267 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t267>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t271>
    t271 add(t271 numA, t271 numB) {
        return (([&]() -> t271 {
            using a = t271;
            return ((t271) (numA + numB));
        })());
    }
}

namespace Prelude {
    template<typename t273>
    t273 sub(t273 numA, t273 numB) {
        return (([&]() -> t273 {
            using a = t273;
            return ((t273) (numA - numB));
        })());
    }
}

namespace Prelude {
    template<typename t275>
    t275 mul(t275 numA, t275 numB) {
        return (([&]() -> t275 {
            using a = t275;
            return ((t275) (numA * numB));
        })());
    }
}

namespace Prelude {
    template<typename t277>
    t277 div(t277 numA, t277 numB) {
        return (([&]() -> t277 {
            using a = t277;
            return ((t277) (numA / numB));
        })());
    }
}

namespace Prelude {
    template<typename t283, typename t284>
    juniper::tuple2<t284, t283> swap(juniper::tuple2<t283, t284> tup) {
        return (([&]() -> juniper::tuple2<t284, t283> {
            juniper::tuple2<t283, t284> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t284 beta = (guid5).e2;
            t283 alpha = (guid5).e1;
            
            return (juniper::tuple2<t284,t283>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t290, typename t291, typename t292>
    t290 until(juniper::function<t292, bool(t290)> p, juniper::function<t291, t290(t290)> f, t290 a0) {
        return (([&]() -> t290 {
            using a = t290;
            return (([&]() -> t290 {
                t290 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t290 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> t290 {
                            return (a = f(a));
                        })());
                    }
                    return {};
                })());
                return a;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t301>
    juniper::unit ignore(t301 val) {
        return (([&]() -> juniper::unit {
            using a = t301;
            return juniper::unit();
        })());
    }
}

namespace Prelude {
    template<typename t304>
    juniper::unit clear(t304& val) {
        return (([&]() -> juniper::unit {
            using a = t304;
            return (([&]() -> juniper::unit {
                
    val.~a();
    memset(&val, 0, sizeof(val));
    
                return {};
            })());
        })());
    }
}

namespace Prelude {
    template<typename t306, int c7>
    juniper::array<t306, c7> array(t306 elem) {
        return (([&]() -> juniper::array<t306, c7> {
            using a = t306;
            constexpr int32_t n = c7;
            return (([&]() -> juniper::array<t306, c7> {
                juniper::array<t306, c7> ret;
                
                (([&]() -> juniper::unit {
                    int32_t guid7 = ((int32_t) 0);
                    int32_t guid8 = n;
                    for (int32_t i = guid7; i < guid8; i++) {
                        (([&]() -> t306 {
                            return ((ret)[i] = elem);
                        })());
                    }
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t312, int c9>
    juniper::array<t312, c9> zeros() {
        return (([&]() -> juniper::array<t312, c9> {
            using a = t312;
            constexpr int32_t n = c9;
            return (([&]() -> juniper::array<t312, c9> {
                juniper::array<t312, c9> ret;
                
                (([&]() -> juniper::unit {
                    memset(&ret, 0, sizeof(ret));
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    uint16_t u8ToU16(uint8_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t u8ToU32(uint8_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t u8ToI8(uint8_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t u8ToI16(uint8_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t u8ToI32(uint8_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float u8ToFloat(uint8_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double u8ToDouble(uint8_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t u16ToU8(uint16_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t u16ToU32(uint16_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t u16ToI8(uint16_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t u16ToI16(uint16_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t u16ToI32(uint16_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float u16ToFloat(uint16_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double u16ToDouble(uint16_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t u32ToU8(uint32_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t u32ToU16(uint32_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t u32ToI8(uint32_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t u32ToI16(uint32_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t u32ToI32(uint32_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float u32ToFloat(uint32_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double u32ToDouble(uint32_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t i8ToU8(int8_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t i8ToU16(int8_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t i8ToU32(int8_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t i8ToI16(int8_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t i8ToI32(int8_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float i8ToFloat(int8_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double i8ToDouble(int8_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t i16ToU8(int16_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t i16ToU16(int16_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t i16ToU32(int16_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t i16ToI8(int16_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t i16ToI32(int16_t n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float i16ToFloat(int16_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double i16ToDouble(int16_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t i32ToU8(int32_t n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t i32ToU16(int32_t n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t i32ToU32(int32_t n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t i32ToI8(int32_t n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t i32ToI16(int32_t n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float i32ToFloat(int32_t n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double i32ToDouble(int32_t n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t floatToU8(float n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t floatToU16(float n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t floatToU32(float n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t floatToI8(float n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t floatToI16(float n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t floatToI32(float n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    double floatToDouble(float n) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = (double) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint8_t doubleToU8(double n) {
        return (([&]() -> uint8_t {
            uint8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint16_t doubleToU16(double n) {
        return (([&]() -> uint16_t {
            uint16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    uint32_t doubleToU32(double n) {
        return (([&]() -> uint32_t {
            uint32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (uint32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int8_t doubleToI8(double n) {
        return (([&]() -> int8_t {
            int8_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int8_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int16_t doubleToI16(double n) {
        return (([&]() -> int16_t {
            int16_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int16_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    int32_t doubleToI32(double n) {
        return (([&]() -> int32_t {
            int32_t ret;
            
            (([&]() -> juniper::unit {
                ret = (int32_t) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    float doubleToFloat(double n) {
        return (([&]() -> float {
            float ret;
            
            (([&]() -> juniper::unit {
                ret = (float) n;
                return {};
            })());
            return ret;
        })());
    }
}

namespace Prelude {
    template<typename t371>
    uint8_t toUInt8(t371 n) {
        return (([&]() -> uint8_t {
            using t = t371;
            return (([&]() -> uint8_t {
                uint8_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (uint8_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t373>
    int8_t toInt8(t373 n) {
        return (([&]() -> int8_t {
            using t = t373;
            return (([&]() -> int8_t {
                int8_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (int8_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t375>
    uint16_t toUInt16(t375 n) {
        return (([&]() -> uint16_t {
            using t = t375;
            return (([&]() -> uint16_t {
                uint16_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (uint16_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t377>
    int16_t toInt16(t377 n) {
        return (([&]() -> int16_t {
            using t = t377;
            return (([&]() -> int16_t {
                int16_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (int16_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t379>
    uint32_t toUInt32(t379 n) {
        return (([&]() -> uint32_t {
            using t = t379;
            return (([&]() -> uint32_t {
                uint32_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (uint32_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t381>
    int32_t toInt32(t381 n) {
        return (([&]() -> int32_t {
            using t = t381;
            return (([&]() -> int32_t {
                int32_t ret;
                
                (([&]() -> juniper::unit {
                    ret = (int32_t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t383>
    float toFloat(t383 n) {
        return (([&]() -> float {
            using t = t383;
            return (([&]() -> float {
                float ret;
                
                (([&]() -> juniper::unit {
                    ret = (float) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t385>
    double toDouble(t385 n) {
        return (([&]() -> double {
            using t = t385;
            return (([&]() -> double {
                double ret;
                
                (([&]() -> juniper::unit {
                    ret = (double) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t387>
    t387 fromUInt8(uint8_t n) {
        return (([&]() -> t387 {
            using t = t387;
            return (([&]() -> t387 {
                t387 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t389>
    t389 fromInt8(int8_t n) {
        return (([&]() -> t389 {
            using t = t389;
            return (([&]() -> t389 {
                t389 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t391>
    t391 fromUInt16(uint16_t n) {
        return (([&]() -> t391 {
            using t = t391;
            return (([&]() -> t391 {
                t391 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t393>
    t393 fromInt16(int16_t n) {
        return (([&]() -> t393 {
            using t = t393;
            return (([&]() -> t393 {
                t393 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t395>
    t395 fromUInt32(uint32_t n) {
        return (([&]() -> t395 {
            using t = t395;
            return (([&]() -> t395 {
                t395 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t397>
    t397 fromInt32(int32_t n) {
        return (([&]() -> t397 {
            using t = t397;
            return (([&]() -> t397 {
                t397 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t399>
    t399 fromFloat(float n) {
        return (([&]() -> t399 {
            using t = t399;
            return (([&]() -> t399 {
                t399 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t401>
    t401 fromDouble(double n) {
        return (([&]() -> t401 {
            using t = t401;
            return (([&]() -> t401 {
                t401 ret;
                
                (([&]() -> juniper::unit {
                    ret = (t) n;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace Prelude {
    template<typename t403, typename t404>
    t404 cast(t403 x) {
        return (([&]() -> t404 {
            using a = t403;
            using b = t404;
            return (([&]() -> t404 {
                t404 ret;
                
                (([&]() -> juniper::unit {
                    ret = (b) x;
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t407, int c10>
    juniper::records::recordt_0<juniper::array<t407, c10>, uint32_t> empty() {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t407, c10>, uint32_t> {
            using a = t407;
            constexpr int32_t n = c10;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t407, c10>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t407, c10>, uint32_t> guid9;
                guid9.data = zeros<t407, c10>();
                guid9.length = ((uint32_t) 0);
                return guid9;
            })());
        })());
    }
}

namespace List {
    template<typename t414, typename t415, typename t421, int c12>
    juniper::records::recordt_0<juniper::array<t414, c12>, uint32_t> map(juniper::function<t415, t414(t421)> f, juniper::records::recordt_0<juniper::array<t421, c12>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t414, c12>, uint32_t> {
            using a = t421;
            using b = t414;
            constexpr int32_t n = c12;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t414, c12>, uint32_t> {
                juniper::array<t414, c12> guid10 = zeros<t414, c12>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t414, c12> ret = guid10;
                
                (([&]() -> juniper::unit {
                    uint32_t guid11 = ((uint32_t) 0);
                    uint32_t guid12 = (lst).length;
                    for (uint32_t i = guid11; i < guid12; i++) {
                        (([&]() -> t414 {
                            return ((ret)[i] = f(((lst).data)[i]));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t414, c12>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t414, c12>, uint32_t> guid13;
                    guid13.data = ret;
                    guid13.length = (lst).length;
                    return guid13;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t429, typename t431, typename t434, int c16>
    t429 fold(juniper::function<t431, t429(t434, t429)> f, t429 initState, juniper::records::recordt_0<juniper::array<t434, c16>, uint32_t> lst) {
        return (([&]() -> t429 {
            using state = t429;
            using t = t434;
            constexpr int32_t n = c16;
            return (([&]() -> t429 {
                t429 guid14 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t429 s = guid14;
                
                (([&]() -> juniper::unit {
                    uint32_t guid15 = ((uint32_t) 0);
                    uint32_t guid16 = (lst).length;
                    for (uint32_t i = guid15; i < guid16; i++) {
                        (([&]() -> t429 {
                            return (s = f(((lst).data)[i], s));
                        })());
                    }
                    return {};
                })());
                return s;
            })());
        })());
    }
}

namespace List {
    template<typename t441, typename t443, typename t449, int c18>
    t441 foldBack(juniper::function<t443, t441(t449, t441)> f, t441 initState, juniper::records::recordt_0<juniper::array<t449, c18>, uint32_t> lst) {
        return (([&]() -> t441 {
            using state = t441;
            using t = t449;
            constexpr int32_t n = c18;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> t441 {
                    return initState;
                })())
            :
                (([&]() -> t441 {
                    t441 guid17 = initState;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t441 s = guid17;
                    
                    uint32_t guid18 = (lst).length;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint32_t i = guid18;
                    
                    (([&]() -> juniper::unit {
                        do {
                            (([&]() -> t441 {
                                (i = ((uint32_t) (i - ((uint32_t) 1))));
                                return (s = f(((lst).data)[i], s));
                            })());
                        } while(((bool) (i > ((uint32_t) 0))));
                        return {};
                    })());
                    return s;
                })()));
        })());
    }
}

namespace List {
    template<typename t457, typename t459, int c20>
    t459 reduce(juniper::function<t457, t459(t459, t459)> f, juniper::records::recordt_0<juniper::array<t459, c20>, uint32_t> lst) {
        return (([&]() -> t459 {
            using t = t459;
            constexpr int32_t n = c20;
            return (([&]() -> t459 {
                t459 guid19 = ((lst).data)[((uint32_t) 0)];
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t459 s = guid19;
                
                (([&]() -> juniper::unit {
                    uint32_t guid20 = ((uint32_t) 1);
                    uint32_t guid21 = (lst).length;
                    for (uint32_t i = guid20; i < guid21; i++) {
                        (([&]() -> t459 {
                            return (s = f(((lst).data)[i], s));
                        })());
                    }
                    return {};
                })());
                return s;
            })());
        })());
    }
}

namespace List {
    template<typename t470, typename t471, int c23>
    Prelude::maybe<t470> tryReduce(juniper::function<t471, t470(t470, t470)> f, juniper::records::recordt_0<juniper::array<t470, c23>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t470> {
            using t = t470;
            constexpr int32_t n = c23;
            return (([&]() -> Prelude::maybe<t470> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t470> {
                        return nothing<t470>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t470> {
                        return just<t470>(reduce<t471, t470, c23>(f, lst));
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t514, typename t518, int c25>
    t518 reduceBack(juniper::function<t514, t518(t518, t518)> f, juniper::records::recordt_0<juniper::array<t518, c25>, uint32_t> lst) {
        return (([&]() -> t518 {
            using t = t518;
            constexpr int32_t n = c25;
            return (([&]() -> t518 {
                t518 guid22 = ((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))];
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t518 s = guid22;
                
                uint32_t guid23 = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t i = guid23;
                
                (([&]() -> juniper::unit {
                    do {
                        (([&]() -> t518 {
                            (i = ((uint32_t) (i - ((uint32_t) 1))));
                            return (s = f(((lst).data)[i], s));
                        })());
                    } while(((bool) (i > ((uint32_t) 0))));
                    return {};
                })());
                return s;
            })());
        })());
    }
}

namespace List {
    template<typename t530, typename t531, int c28>
    Prelude::maybe<t530> tryReduceBack(juniper::function<t531, t530(t530, t530)> f, juniper::records::recordt_0<juniper::array<t530, c28>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t530> {
            using t = t530;
            constexpr int32_t n = c28;
            return (([&]() -> Prelude::maybe<t530> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t530> {
                        return nothing<t530>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t530> {
                        return just<t530>(reduceBack<t531, t530, c28>(f, lst));
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t586, int c30, int c31, int c32>
    juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> concat(juniper::records::recordt_0<juniper::array<t586, c30>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t586, c31>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> {
            using t = t586;
            constexpr int32_t aCap = c30;
            constexpr int32_t bCap = c31;
            constexpr int32_t retCap = c32;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> {
                uint32_t guid24 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid24;
                
                juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> guid25 = (([&]() -> juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> guid26;
                    guid26.data = zeros<t586, c32>();
                    guid26.length = ((uint32_t) ((lstA).length + (lstB).length));
                    return guid26;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t586, c32>, uint32_t> out = guid25;
                
                (([&]() -> juniper::unit {
                    uint32_t guid27 = ((uint32_t) 0);
                    uint32_t guid28 = (lstA).length;
                    for (uint32_t i = guid27; i < guid28; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid29 = ((uint32_t) 0);
                    uint32_t guid30 = (lstB).length;
                    for (uint32_t i = guid29; i < guid30; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((lstB).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return out;
            })());
        })());
    }
}

namespace List {
    template<typename t591, int c38, int c39>
    juniper::records::recordt_0<juniper::array<t591, (((c38)*(-1))+((c39)*(-1)))*(-1)>, uint32_t> concatSafe(juniper::records::recordt_0<juniper::array<t591, c38>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t591, c39>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t591, (((c38)*(-1))+((c39)*(-1)))*(-1)>, uint32_t> {
            using t = t591;
            constexpr int32_t aCap = c38;
            constexpr int32_t bCap = c39;
            return concat<t591, c38, c39, (((c38)*(-1))+((c39)*(-1)))*(-1)>(lstA, lstB);
        })());
    }
}

namespace List {
    template<typename t599, typename t601, int c43>
    t601 get(t599 i, juniper::records::recordt_0<juniper::array<t601, c43>, uint32_t> lst) {
        return (([&]() -> t601 {
            using t = t601;
            using u = t599;
            constexpr int32_t n = c43;
            return ((lst).data)[i];
        })());
    }
}

namespace List {
    template<typename t604, typename t612, int c45>
    Prelude::maybe<t612> tryGet(t604 i, juniper::records::recordt_0<juniper::array<t612, c45>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t612> {
            using t = t612;
            using u = t604;
            constexpr int32_t n = c45;
            return (((bool) (i < (lst).length)) ? 
                just<t612>(((lst).data)[i])
            :
                nothing<t612>());
        })());
    }
}

namespace List {
    template<typename t645, int c47, int c48, int c49>
    juniper::records::recordt_0<juniper::array<t645, c49>, uint32_t> flatten(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t645, c47>, uint32_t>, c48>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t645, c49>, uint32_t> {
            using t = t645;
            constexpr int32_t m = c47;
            constexpr int32_t n = c48;
            constexpr int32_t retCap = c49;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t645, c49>, uint32_t> {
                juniper::array<t645, c49> guid31 = zeros<t645, c49>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t645, c49> ret = guid31;
                
                uint32_t guid32 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid32;
                
                (([&]() -> juniper::unit {
                    uint32_t guid33 = ((uint32_t) 0);
                    uint32_t guid34 = (listOfLists).length;
                    for (uint32_t i = guid33; i < guid34; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                uint32_t guid35 = ((uint32_t) 0);
                                uint32_t guid36 = (((listOfLists).data)[i]).length;
                                for (uint32_t j = guid35; j < guid36; j++) {
                                    (([&]() -> uint32_t {
                                        ((ret)[index] = ((((listOfLists).data)[i]).data)[j]);
                                        return (index += ((uint32_t) 1));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t645, c49>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t645, c49>, uint32_t> guid37;
                    guid37.data = ret;
                    guid37.length = index;
                    return guid37;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t649, int c55, int c56>
    juniper::records::recordt_0<juniper::array<t649, (c56)*(c55)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t649, c55>, uint32_t>, c56>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t649, (c56)*(c55)>, uint32_t> {
            using t = t649;
            constexpr int32_t m = c55;
            constexpr int32_t n = c56;
            return flatten<t649, c55, c56, (c56)*(c55)>(listOfLists);
        })());
    }
}

namespace Math {
    template<typename t655>
    t655 min_(t655 x, t655 y) {
        return (([&]() -> t655 {
            using a = t655;
            return (((bool) (x > y)) ? 
                (([&]() -> t655 {
                    return y;
                })())
            :
                (([&]() -> t655 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t672, int c60, int c61>
    juniper::records::recordt_0<juniper::array<t672, c60>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t672, c61>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t672, c60>, uint32_t> {
            using t = t672;
            constexpr int32_t m = c60;
            constexpr int32_t n = c61;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t672, c60>, uint32_t> {
                juniper::array<t672, c60> guid38 = zeros<t672, c60>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t672, c60> ret = guid38;
                
                (([&]() -> juniper::unit {
                    uint32_t guid39 = ((uint32_t) 0);
                    uint32_t guid40 = Math::min_<uint32_t>((lst).length, toUInt32<int32_t>(m));
                    for (uint32_t i = guid39; i < guid40; i++) {
                        (([&]() -> t672 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t672, c60>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t672, c60>, uint32_t> guid41;
                    guid41.data = ret;
                    guid41.length = (lst).length;
                    return guid41;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t678, typename t681, int c65>
    bool all(juniper::function<t678, bool(t681)> pred, juniper::records::recordt_0<juniper::array<t681, c65>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t681;
            constexpr int32_t n = c65;
            return (([&]() -> bool {
                bool guid42 = true;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid42;
                
                (([&]() -> juniper::unit {
                    uint32_t guid43 = ((uint32_t) 0);
                    uint32_t guid44 = (lst).length;
                    for (uint32_t i = guid43; i < guid44; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (satisfied) {
                                    (([&]() -> bool {
                                        return (satisfied = pred(((lst).data)[i]));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return satisfied;
            })());
        })());
    }
}

namespace List {
    template<typename t688, typename t691, int c67>
    bool any(juniper::function<t688, bool(t691)> pred, juniper::records::recordt_0<juniper::array<t691, c67>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t691;
            constexpr int32_t n = c67;
            return (([&]() -> bool {
                bool guid45 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool satisfied = guid45;
                
                (([&]() -> juniper::unit {
                    uint32_t guid46 = ((uint32_t) 0);
                    uint32_t guid47 = (lst).length;
                    for (uint32_t i = guid46; i < guid47; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (!(satisfied)) {
                                    (([&]() -> bool {
                                        return (satisfied = pred(((lst).data)[i]));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return satisfied;
            })());
        })());
    }
}

namespace List {
    template<typename t696, int c69>
    juniper::unit append(t696 elem, juniper::records::recordt_0<juniper::array<t696, c69>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t696;
            constexpr int32_t n = c69;
            return (([&]() -> juniper::unit {
                if (((bool) ((lst).length < n))) {
                    (([&]() -> uint32_t {
                        (((lst).data)[(lst).length] = elem);
                        return ((lst).length += ((uint32_t) 1));
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t704, int c71>
    juniper::records::recordt_0<juniper::array<t704, c71>, uint32_t> appendPure(t704 elem, juniper::records::recordt_0<juniper::array<t704, c71>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t704, c71>, uint32_t> {
            using t = t704;
            constexpr int32_t n = c71;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t704, c71>, uint32_t> {
                (([&]() -> juniper::unit {
                    if (((bool) ((lst).length < n))) {
                        (([&]() -> uint32_t {
                            (((lst).data)[(lst).length] = elem);
                            return ((lst).length += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t712, int c73>
    juniper::records::recordt_0<juniper::array<t712, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> appendSafe(t712 elem, juniper::records::recordt_0<juniper::array<t712, c73>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t712, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> {
            using t = t712;
            constexpr int32_t n = c73;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t712, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t712, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> guid48 = resize<t712, ((-1)+((c73)*(-1)))*(-1), c73>(lst);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t712, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> ret = guid48;
                
                (((ret).data)[(lst).length] = elem);
                ((ret).length += ((uint32_t) 1));
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t727, int c77>
    juniper::unit prepend(t727 elem, juniper::records::recordt_0<juniper::array<t727, c77>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t727;
            constexpr int32_t n = c77;
            return (([&]() -> juniper::unit {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::unit {
                        return juniper::unit();
                    })())
                :
                    (([&]() -> juniper::unit {
                        uint32_t guid49 = (lst).length;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint32_t i = guid49;
                        
                        (([&]() -> juniper::unit {
                            do {
                                (([&]() -> uint32_t {
                                    (([&]() -> juniper::unit {
                                        if (((bool) (i < n))) {
                                            (([&]() -> t727 {
                                                return (((lst).data)[i] = ((lst).data)[((uint32_t) (i - ((uint32_t) 1)))]);
                                            })());
                                        }
                                        return {};
                                    })());
                                    return (i -= ((uint32_t) 1));
                                })());
                            } while(((bool) (i > ((uint32_t) 0))));
                            return {};
                        })());
                        (((lst).data)[((uint32_t) 0)] = elem);
                        return (([&]() -> juniper::unit {
                            if (((bool) ((lst).length < n))) {
                                (([&]() -> uint32_t {
                                    return ((lst).length += ((uint32_t) 1));
                                })());
                            }
                            return {};
                        })());
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t744, int c81>
    juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> prependPure(t744 elem, juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> {
            using t = t744;
            constexpr int32_t n = c81;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t744, c81>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = ((uint32_t) 0);
                            uint32_t guid51 = (lst).length;
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> juniper::unit {
                                    return (([&]() -> juniper::unit {
                                        if (((bool) (((uint32_t) (i + ((uint32_t) 1))) < n))) {
                                            (([&]() -> t744 {
                                                return (((ret).data)[((uint32_t) (i + ((uint32_t) 1)))] = ((lst).data)[i]);
                                            })());
                                        }
                                        return {};
                                    })());
                                })());
                            }
                            return {};
                        })());
                        (((bool) ((lst).length == cast<int32_t, uint32_t>(n))) ? 
                            (([&]() -> uint32_t {
                                return ((ret).length = (lst).length);
                            })())
                        :
                            (([&]() -> uint32_t {
                                return ((ret).length += ((uint32_t) 1));
                            })()));
                        return ret;
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t759, int c85>
    juniper::unit set(uint32_t index, t759 elem, juniper::records::recordt_0<juniper::array<t759, c85>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t759;
            constexpr int32_t n = c85;
            return (([&]() -> juniper::unit {
                if (((bool) (index < (lst).length))) {
                    (([&]() -> t759 {
                        return (((lst).data)[index] = elem);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t764, int c87>
    juniper::records::recordt_0<juniper::array<t764, c87>, uint32_t> setPure(uint32_t index, t764 elem, juniper::records::recordt_0<juniper::array<t764, c87>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t764, c87>, uint32_t> {
            using t = t764;
            constexpr int32_t n = c87;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t764, c87>, uint32_t> {
                (([&]() -> juniper::unit {
                    if (((bool) (index < (lst).length))) {
                        (([&]() -> t764 {
                            return (((lst).data)[index] = elem);
                        })());
                    }
                    return {};
                })());
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t770, int c89>
    juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> replicate(uint32_t numOfElements, t770 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> {
            using t = t770;
            constexpr int32_t n = c89;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> guid52 = (([&]() -> juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> guid53;
                    guid53.data = zeros<t770, c89>();
                    guid53.length = numOfElements;
                    return guid53;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t770, c89>, uint32_t> ret = guid52;
                
                (([&]() -> juniper::unit {
                    uint32_t guid54 = ((uint32_t) 0);
                    uint32_t guid55 = numOfElements;
                    for (uint32_t i = guid54; i < guid55; i++) {
                        (([&]() -> t770 {
                            return (((ret).data)[i] = elem);
                        })());
                    }
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t791, int c92>
    juniper::unit remove(t791 elem, juniper::records::recordt_0<juniper::array<t791, c92>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t791;
            constexpr int32_t n = c92;
            return (([&]() -> juniper::unit {
                uint32_t guid56 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t index = guid56;
                
                bool guid57 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid57;
                
                (([&]() -> juniper::unit {
                    uint32_t guid58 = ((uint32_t) 0);
                    uint32_t guid59 = (lst).length;
                    for (uint32_t i = guid58; i < guid59; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (((bool) (!(found) && ((bool) (((lst).data)[i] == elem))))) {
                                    (([&]() -> bool {
                                        (index = i);
                                        return (found = true);
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::unit {
                    if (found) {
                        (([&]() -> juniper::unit {
                            ((lst).length -= ((uint32_t) 1));
                            (([&]() -> juniper::unit {
                                uint32_t guid60 = index;
                                uint32_t guid61 = (lst).length;
                                for (uint32_t i = guid60; i < guid61; i++) {
                                    (([&]() -> t791 {
                                        return (((lst).data)[i] = ((lst).data)[((uint32_t) (i + ((uint32_t) 1)))]);
                                    })());
                                }
                                return {};
                            })());
                            return clear<t791>(((lst).data)[(lst).length]);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t798, int c97>
    juniper::records::recordt_0<juniper::array<t798, c97>, uint32_t> removePure(t798 elem, juniper::records::recordt_0<juniper::array<t798, c97>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t798, c97>, uint32_t> {
            using t = t798;
            constexpr int32_t n = c97;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t798, c97>, uint32_t> {
                remove<t798, c97>(elem, lst);
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t810, int c99>
    juniper::unit pop(juniper::records::recordt_0<juniper::array<t810, c99>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t810;
            constexpr int32_t n = c99;
            return (([&]() -> juniper::unit {
                if (((bool) ((lst).length > ((uint32_t) 0)))) {
                    (([&]() -> juniper::unit {
                        ((lst).length -= ((uint32_t) 1));
                        return clear<t810>(((lst).data)[(lst).length]);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t817, int c101>
    juniper::records::recordt_0<juniper::array<t817, c101>, uint32_t> popPure(juniper::records::recordt_0<juniper::array<t817, c101>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t817, c101>, uint32_t> {
            using t = t817;
            constexpr int32_t n = c101;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t817, c101>, uint32_t> {
                pop<t817, c101>(lst);
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t825, typename t828, int c103>
    juniper::unit iter(juniper::function<t825, juniper::unit(t828)> f, juniper::records::recordt_0<juniper::array<t828, c103>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using t = t828;
            constexpr int32_t n = c103;
            return (([&]() -> juniper::unit {
                uint32_t guid62 = ((uint32_t) 0);
                uint32_t guid63 = (lst).length;
                for (uint32_t i = guid62; i < guid63; i++) {
                    (([&]() -> juniper::unit {
                        return f(((lst).data)[i]);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t837, int c105>
    t837 last(juniper::records::recordt_0<juniper::array<t837, c105>, uint32_t> lst) {
        return (([&]() -> t837 {
            using t = t837;
            constexpr int32_t n = c105;
            return ((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))];
        })());
    }
}

namespace List {
    template<typename t849, int c107>
    Prelude::maybe<t849> tryLast(juniper::records::recordt_0<juniper::array<t849, c107>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t849> {
            using t = t849;
            constexpr int32_t n = c107;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t849> {
                    return nothing<t849>();
                })())
            :
                (([&]() -> Prelude::maybe<t849> {
                    return just<t849>(((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t885, int c109>
    Prelude::maybe<t885> tryMax(juniper::records::recordt_0<juniper::array<t885, c109>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t885> {
            using t = t885;
            constexpr int32_t n = c109;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t885> {
                    return nothing<t885>();
                })())
            :
                (([&]() -> Prelude::maybe<t885> {
                    t885 guid64 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t885 maxVal = guid64;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid65 = ((uint32_t) 1);
                        uint32_t guid66 = (lst).length;
                        for (uint32_t i = guid65; i < guid66; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] > maxVal))) {
                                        (([&]() -> t885 {
                                            return (maxVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t885>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t924, int c113>
    Prelude::maybe<t924> tryMin(juniper::records::recordt_0<juniper::array<t924, c113>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t924> {
            using t = t924;
            constexpr int32_t n = c113;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t924> {
                    return nothing<t924>();
                })())
            :
                (([&]() -> Prelude::maybe<t924> {
                    t924 guid67 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t924 minVal = guid67;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid68 = ((uint32_t) 1);
                        uint32_t guid69 = (lst).length;
                        for (uint32_t i = guid68; i < guid69; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] < minVal))) {
                                        (([&]() -> t924 {
                                            return (minVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t924>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t954, int c117>
    bool member(t954 elem, juniper::records::recordt_0<juniper::array<t954, c117>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t954;
            constexpr int32_t n = c117;
            return (([&]() -> bool {
                bool guid70 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool found = guid70;
                
                (([&]() -> juniper::unit {
                    uint32_t guid71 = ((uint32_t) 0);
                    uint32_t guid72 = (lst).length;
                    for (uint32_t i = guid71; i < guid72; i++) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                if (((bool) (!(found) && ((bool) (((lst).data)[i] == elem))))) {
                                    (([&]() -> bool {
                                        return (found = true);
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return found;
            })());
        })());
    }
}

namespace List {
    template<typename t972, typename t974, int c119>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t972, c119>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t974, c119>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> {
            using a = t972;
            using b = t974;
            constexpr int32_t n = c119;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> {
                uint32_t guid73 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid73;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> guid74 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> guid75;
                    guid75.data = zeros<juniper::tuple2<t972, t974>, c119>();
                    guid75.length = outLen;
                    return guid75;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t972, t974>, c119>, uint32_t> ret = guid74;
                
                (([&]() -> juniper::unit {
                    uint32_t guid76 = ((uint32_t) 0);
                    uint32_t guid77 = outLen;
                    for (uint32_t i = guid76; i < guid77; i++) {
                        (([&]() -> juniper::tuple2<t972, t974> {
                            return (((ret).data)[i] = (juniper::tuple2<t972,t974>{((lstA).data)[i], ((lstB).data)[i]}));
                        })());
                    }
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t991, typename t992, int c124>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t991, t992>, c124>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t>> {
            using a = t991;
            using b = t992;
            constexpr int32_t n = c124;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t> guid78 = (([&]() -> juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t> guid79;
                    guid79.data = zeros<t991, c124>();
                    guid79.length = (lst).length;
                    return guid79;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t> retA = guid78;
                
                juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t> guid80 = (([&]() -> juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t> guid81;
                    guid81.data = zeros<t992, c124>();
                    guid81.length = (lst).length;
                    return guid81;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t> retB = guid80;
                
                (([&]() -> juniper::unit {
                    uint32_t guid82 = ((uint32_t) 0);
                    uint32_t guid83 = (lst).length;
                    for (uint32_t i = guid82; i < guid83; i++) {
                        (([&]() -> t992 {
                            juniper::tuple2<t991, t992> guid84 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t992 b = (guid84).e2;
                            t991 a = (guid84).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t991, c124>, uint32_t>,juniper::records::recordt_0<juniper::array<t992, c124>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t1000, int c130>
    t1000 sum(juniper::records::recordt_0<juniper::array<t1000, c130>, uint32_t> lst) {
        return (([&]() -> t1000 {
            using a = t1000;
            constexpr int32_t n = c130;
            return List::fold<t1000, void, t1000, c130>(Prelude::add<t1000>, ((t1000) 0), lst);
        })());
    }
}

namespace List {
    template<typename t1018, int c132>
    t1018 average(juniper::records::recordt_0<juniper::array<t1018, c132>, uint32_t> lst) {
        return (([&]() -> t1018 {
            using a = t1018;
            constexpr int32_t n = c132;
            return ((t1018) (sum<t1018, c132>(lst) / cast<uint32_t, t1018>((lst).length)));
        })());
    }
}

namespace List {
    uint32_t iLeftChild(uint32_t i) {
        return ((uint32_t) (((uint32_t) (((uint32_t) 2) * i)) + ((uint32_t) 1)));
    }
}

namespace List {
    uint32_t iRightChild(uint32_t i) {
        return ((uint32_t) (((uint32_t) (((uint32_t) 2) * i)) + ((uint32_t) 2)));
    }
}

namespace List {
    uint32_t iParent(uint32_t i) {
        return ((uint32_t) (((uint32_t) (i - ((uint32_t) 1))) / ((uint32_t) 2)));
    }
}

namespace List {
    template<typename t1043, typename t1045, typename t1047, typename t1078, int c134>
    juniper::unit siftDown(juniper::records::recordt_0<juniper::array<t1078, c134>, uint32_t>& lst, juniper::function<t1047, t1043(t1078)> key, uint32_t root, t1045 end) {
        return (([&]() -> juniper::unit {
            using m = t1043;
            using t = t1078;
            constexpr int32_t n = c134;
            return (([&]() -> juniper::unit {
                bool guid85 = false;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                bool done = guid85;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (((bool) (iLeftChild(root) < end)) && !(done)))) {
                        (([&]() -> juniper::unit {
                            uint32_t guid86 = iLeftChild(root);
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            uint32_t child = guid86;
                            
                            (([&]() -> juniper::unit {
                                if (((bool) (((bool) (((uint32_t) (child + ((uint32_t) 1))) < end)) && ((bool) (key(((lst).data)[child]) < key(((lst).data)[((uint32_t) (child + ((uint32_t) 1)))])))))) {
                                    (([&]() -> uint32_t {
                                        return (child += ((uint32_t) 1));
                                    })());
                                }
                                return {};
                            })());
                            return (((bool) (key(((lst).data)[root]) < key(((lst).data)[child]))) ? 
                                (([&]() -> juniper::unit {
                                    t1078 guid87 = ((lst).data)[root];
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    t1078 tmp = guid87;
                                    
                                    (((lst).data)[root] = ((lst).data)[child]);
                                    (((lst).data)[child] = tmp);
                                    (root = child);
                                    return juniper::unit();
                                })())
                            :
                                (([&]() -> juniper::unit {
                                    (done = true);
                                    return juniper::unit();
                                })()));
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1089, typename t1098, typename t1100, int c143>
    juniper::unit heapify(juniper::records::recordt_0<juniper::array<t1089, c143>, uint32_t>& lst, juniper::function<t1100, t1098(t1089)> key) {
        return (([&]() -> juniper::unit {
            using t = t1089;
            constexpr int32_t n = c143;
            return (([&]() -> juniper::unit {
                uint32_t guid88 = iParent(((uint32_t) ((lst).length + ((uint32_t) 1))));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t start = guid88;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (start > ((uint32_t) 0)))) {
                        (([&]() -> juniper::unit {
                            (start -= ((uint32_t) 1));
                            return siftDown<t1098, uint32_t, t1100, t1089, c143>(lst, key, start, (lst).length);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1111, typename t1113, typename t1126, int c145>
    juniper::unit sort(juniper::function<t1113, t1111(t1126)> key, juniper::records::recordt_0<juniper::array<t1126, c145>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using m = t1111;
            using t = t1126;
            constexpr int32_t n = c145;
            return (([&]() -> juniper::unit {
                heapify<t1126, t1111, t1113, c145>(lst, key);
                uint32_t guid89 = (lst).length;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t end = guid89;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (end > ((uint32_t) 1)))) {
                        (([&]() -> juniper::unit {
                            (end -= ((uint32_t) 1));
                            t1126 guid90 = ((lst).data)[((uint32_t) 0)];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t1126 tmp = guid90;
                            
                            (((lst).data)[((uint32_t) 0)] = ((lst).data)[end]);
                            (((lst).data)[end] = tmp);
                            return siftDown<t1111, uint32_t, t1113, t1126, c145>(lst, key, ((uint32_t) 0), end);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1144, typename t1145, typename t1146, int c152>
    juniper::records::recordt_0<juniper::array<t1145, c152>, uint32_t> sorted(juniper::function<t1146, t1144(t1145)> key, juniper::records::recordt_0<juniper::array<t1145, c152>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t1145, c152>, uint32_t> {
            using m = t1144;
            using t = t1145;
            constexpr int32_t n = c152;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t1145, c152>, uint32_t> {
                sort<t1144, t1146, t1145, c152>(key, lst);
                return lst;
            })());
        })());
    }
}

namespace Signal {
    template<typename t1156, typename t1158, typename t1173>
    Prelude::sig<t1173> map(juniper::function<t1158, t1173(t1156)> f, Prelude::sig<t1156> s) {
        return (([&]() -> Prelude::sig<t1173> {
            using a = t1156;
            using b = t1173;
            return (([&]() -> Prelude::sig<t1173> {
                Prelude::sig<t1156> guid91 = s;
                return (((bool) (((bool) ((guid91).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid91).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1173> {
                        t1156 val = ((guid91).signal()).just();
                        return signal<t1173>(just<t1173>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1173> {
                            return signal<t1173>(nothing<t1173>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1173>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1242, typename t1243>
    juniper::unit sink(juniper::function<t1243, juniper::unit(t1242)> f, Prelude::sig<t1242> s) {
        return (([&]() -> juniper::unit {
            using a = t1242;
            return (([&]() -> juniper::unit {
                Prelude::sig<t1242> guid92 = s;
                return (((bool) (((bool) ((guid92).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid92).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::unit {
                        t1242 val = ((guid92).signal()).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            return juniper::unit();
                        })())
                    :
                        juniper::quit<juniper::unit>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1266, typename t1280>
    Prelude::sig<t1280> filter(juniper::function<t1266, bool(t1280)> f, Prelude::sig<t1280> s) {
        return (([&]() -> Prelude::sig<t1280> {
            using a = t1280;
            return (([&]() -> Prelude::sig<t1280> {
                Prelude::sig<t1280> guid93 = s;
                return (((bool) (((bool) ((guid93).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid93).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1280> {
                        t1280 val = ((guid93).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t1280> {
                                return signal<t1280>(nothing<t1280>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t1280> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1280> {
                            return signal<t1280>(nothing<t1280>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1280>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1360>
    Prelude::sig<t1360> merge(Prelude::sig<t1360> sigA, Prelude::sig<t1360> sigB) {
        return (([&]() -> Prelude::sig<t1360> {
            using a = t1360;
            return (([&]() -> Prelude::sig<t1360> {
                Prelude::sig<t1360> guid94 = sigA;
                return (((bool) (((bool) ((guid94).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid94).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1360> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1360> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t1360>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1396, int c154>
    Prelude::sig<t1396> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t1396>, c154>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t1396> {
            using a = t1396;
            constexpr int32_t n = c154;
            return (([&]() -> Prelude::sig<t1396> {
                Prelude::maybe<t1396> guid95 = List::fold<Prelude::maybe<t1396>, void, Prelude::sig<t1396>, c154>(juniper::function<void, Prelude::maybe<t1396>(Prelude::sig<t1396>,Prelude::maybe<t1396>)>([](Prelude::sig<t1396> sig, Prelude::maybe<t1396> accum) -> Prelude::maybe<t1396> { 
                    return (([&]() -> Prelude::maybe<t1396> {
                        Prelude::maybe<t1396> guid96 = accum;
                        return (((bool) (((bool) ((guid96).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::maybe<t1396> {
                                return (([&]() -> Prelude::maybe<t1396> {
                                    Prelude::sig<t1396> guid97 = sig;
                                    if (!(((bool) (((bool) ((guid97).id() == ((uint8_t) 0))) && true)))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t1396> heldValue = (guid97).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t1396> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t1396>>()));
                    })());
                 }), nothing<t1396>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t1396> ret = guid95;
                
                return signal<t1396>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t1514, typename t1538>
    Prelude::sig<Prelude::either<t1538, t1514>> join(Prelude::sig<t1538> sigA, Prelude::sig<t1514> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t1538, t1514>> {
            using a = t1538;
            using b = t1514;
            return (([&]() -> Prelude::sig<Prelude::either<t1538, t1514>> {
                juniper::tuple2<Prelude::sig<t1538>, Prelude::sig<t1514>> guid98 = (juniper::tuple2<Prelude::sig<t1538>,Prelude::sig<t1514>>{sigA, sigB});
                return (((bool) (((bool) (((guid98).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid98).e1).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t1538, t1514>> {
                        t1538 value = (((guid98).e1).signal()).just();
                        return signal<Prelude::either<t1538, t1514>>(just<Prelude::either<t1538, t1514>>(left<t1538, t1514>(value)));
                    })())
                :
                    (((bool) (((bool) (((guid98).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid98).e2).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t1538, t1514>> {
                            t1514 value = (((guid98).e2).signal()).just();
                            return signal<Prelude::either<t1538, t1514>>(just<Prelude::either<t1538, t1514>>(right<t1538, t1514>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t1538, t1514>> {
                                return signal<Prelude::either<t1538, t1514>>(nothing<Prelude::either<t1538, t1514>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t1538, t1514>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1827>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t1827> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t1827;
            return map<t1827, void, juniper::unit>(juniper::function<void, juniper::unit(t1827)>([](t1827 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t1861, typename t1863, typename t1879>
    Prelude::sig<t1879> foldP(juniper::function<t1863, t1879(t1861, t1879)> f, t1879& state0, Prelude::sig<t1861> incoming) {
        return (([&]() -> Prelude::sig<t1879> {
            using a = t1861;
            using state = t1879;
            return (([&]() -> Prelude::sig<t1879> {
                Prelude::sig<t1861> guid99 = incoming;
                return (((bool) (((bool) ((guid99).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid99).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1879> {
                        t1861 val = ((guid99).signal()).just();
                        return (([&]() -> Prelude::sig<t1879> {
                            t1879 guid100 = f(val, state0);
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t1879 state1 = guid100;
                            
                            (state0 = state1);
                            return signal<t1879>(just<t1879>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1879> {
                            return signal<t1879>(nothing<t1879>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1879>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1956>
    Prelude::sig<t1956> dropRepeats(Prelude::maybe<t1956>& maybePrevValue, Prelude::sig<t1956> incoming) {
        return (([&]() -> Prelude::sig<t1956> {
            using a = t1956;
            return (([&]() -> Prelude::sig<t1956> {
                Prelude::sig<t1956> guid101 = incoming;
                return (((bool) (((bool) ((guid101).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid101).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1956> {
                        t1956 value = ((guid101).signal()).just();
                        return (([&]() -> Prelude::sig<t1956> {
                            Prelude::sig<t1956> guid102 = (([&]() -> Prelude::sig<t1956> {
                                Prelude::maybe<t1956> guid103 = maybePrevValue;
                                return (((bool) (((bool) ((guid103).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> Prelude::sig<t1956> {
                                        return incoming;
                                    })())
                                :
                                    (((bool) (((bool) ((guid103).id() == ((uint8_t) 0))) && true)) ? 
                                        (([&]() -> Prelude::sig<t1956> {
                                            t1956 prevValue = (guid103).just();
                                            return (((bool) (value == prevValue)) ? 
                                                signal<t1956>(nothing<t1956>())
                                            :
                                                incoming);
                                        })())
                                    :
                                        juniper::quit<Prelude::sig<t1956>>()));
                            })());
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            Prelude::sig<t1956> ret = guid102;
                            
                            (maybePrevValue = just<t1956>(value));
                            return ret;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1956> {
                            return incoming;
                        })())
                    :
                        juniper::quit<Prelude::sig<t1956>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2055>
    Prelude::sig<t2055> latch(t2055& prevValue, Prelude::sig<t2055> incoming) {
        return (([&]() -> Prelude::sig<t2055> {
            using a = t2055;
            return (([&]() -> Prelude::sig<t2055> {
                Prelude::sig<t2055> guid104 = incoming;
                return (((bool) (((bool) ((guid104).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid104).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t2055> {
                        t2055 val = ((guid104).signal()).just();
                        return (([&]() -> Prelude::sig<t2055> {
                            (prevValue = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t2055> {
                            return signal<t2055>(just<t2055>(prevValue));
                        })())
                    :
                        juniper::quit<Prelude::sig<t2055>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2115, typename t2116, typename t2119, typename t2127>
    Prelude::sig<t2115> map2(juniper::function<t2116, t2115(t2119, t2127)> f, juniper::tuple2<t2119, t2127>& state, Prelude::sig<t2119> incomingA, Prelude::sig<t2127> incomingB) {
        return (([&]() -> Prelude::sig<t2115> {
            using a = t2119;
            using b = t2127;
            using c = t2115;
            return (([&]() -> Prelude::sig<t2115> {
                t2119 guid105 = (([&]() -> t2119 {
                    Prelude::sig<t2119> guid106 = incomingA;
                    return (((bool) (((bool) ((guid106).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid106).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t2119 {
                            t2119 val1 = ((guid106).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t2119 {
                                return fst<t2119, t2127>(state);
                            })())
                        :
                            juniper::quit<t2119>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t2119 valA = guid105;
                
                t2127 guid107 = (([&]() -> t2127 {
                    Prelude::sig<t2127> guid108 = incomingB;
                    return (((bool) (((bool) ((guid108).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid108).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t2127 {
                            t2127 val2 = ((guid108).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t2127 {
                                return snd<t2119, t2127>(state);
                            })())
                        :
                            juniper::quit<t2127>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t2127 valB = guid107;
                
                (state = (juniper::tuple2<t2119,t2127>{valA, valB}));
                return (([&]() -> Prelude::sig<t2115> {
                    juniper::tuple2<Prelude::sig<t2119>, Prelude::sig<t2127>> guid109 = (juniper::tuple2<Prelude::sig<t2119>,Prelude::sig<t2127>>{incomingA, incomingB});
                    return (((bool) (((bool) (((guid109).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid109).e2).signal()).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid109).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid109).e1).signal()).id() == ((uint8_t) 1))) && true)))))))) ? 
                        (([&]() -> Prelude::sig<t2115> {
                            return signal<t2115>(nothing<t2115>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t2115> {
                                return signal<t2115>(just<t2115>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t2115>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t2269, int c156>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t2269, c156>, uint32_t>> record(juniper::records::recordt_0<juniper::array<t2269, c156>, uint32_t>& pastValues, Prelude::sig<t2269> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t2269, c156>, uint32_t>> {
            using a = t2269;
            constexpr int32_t n = c156;
            return foldP<t2269, void, juniper::records::recordt_0<juniper::array<t2269, c156>, uint32_t>>(List::prependPure<t2269, c156>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t2307>
    Prelude::sig<t2307> constant(t2307 val) {
        return (([&]() -> Prelude::sig<t2307> {
            using a = t2307;
            return signal<t2307>(just<t2307>(val));
        })());
    }
}

namespace Signal {
    template<typename t2342>
    Prelude::sig<Prelude::maybe<t2342>> meta(Prelude::sig<t2342> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t2342>> {
            using a = t2342;
            return (([&]() -> Prelude::sig<Prelude::maybe<t2342>> {
                Prelude::sig<t2342> guid110 = sigA;
                if (!(((bool) (((bool) ((guid110).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t2342> val = (guid110).signal();
                
                return constant<Prelude::maybe<t2342>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t2409>
    Prelude::sig<t2409> unmeta(Prelude::sig<Prelude::maybe<t2409>> sigA) {
        return (([&]() -> Prelude::sig<t2409> {
            using a = t2409;
            return (([&]() -> Prelude::sig<t2409> {
                Prelude::sig<Prelude::maybe<t2409>> guid111 = sigA;
                return (((bool) (((bool) ((guid111).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid111).signal()).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid111).signal()).just()).id() == ((uint8_t) 0))) && true)))))) ? 
                    (([&]() -> Prelude::sig<t2409> {
                        t2409 val = (((guid111).signal()).just()).just();
                        return constant<t2409>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t2409> {
                            return signal<t2409>(nothing<t2409>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t2409>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2485, typename t2486>
    Prelude::sig<juniper::tuple2<t2485, t2486>> zip(juniper::tuple2<t2485, t2486>& state, Prelude::sig<t2485> sigA, Prelude::sig<t2486> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t2485, t2486>> {
            using a = t2485;
            using b = t2486;
            return map2<juniper::tuple2<t2485, t2486>, void, t2485, t2486>(juniper::function<void, juniper::tuple2<t2485, t2486>(t2485,t2486)>([](t2485 valA, t2486 valB) -> juniper::tuple2<t2485, t2486> { 
                return (juniper::tuple2<t2485,t2486>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t2556, typename t2563>
    juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>> unzip(Prelude::sig<juniper::tuple2<t2556, t2563>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>> {
            using a = t2556;
            using b = t2563;
            return (([&]() -> juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>> {
                Prelude::sig<juniper::tuple2<t2556, t2563>> guid112 = incoming;
                return (((bool) (((bool) ((guid112).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid112).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>> {
                        t2563 y = (((guid112).signal()).just()).e2;
                        t2556 x = (((guid112).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t2556>,Prelude::sig<t2563>>{signal<t2556>(just<t2556>(x)), signal<t2563>(just<t2563>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>> {
                            return (juniper::tuple2<Prelude::sig<t2556>,Prelude::sig<t2563>>{signal<t2556>(nothing<t2556>()), signal<t2563>(nothing<t2563>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t2556>, Prelude::sig<t2563>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2690, typename t2691>
    Prelude::sig<t2690> toggle(t2690 val1, t2690 val2, t2690& state, Prelude::sig<t2691> incoming) {
        return (([&]() -> Prelude::sig<t2690> {
            using a = t2690;
            using b = t2691;
            return foldP<t2691, juniper::closures::closuret_4<t2690, t2690>, t2690>(juniper::function<juniper::closures::closuret_4<t2690, t2690>, t2690(t2691,t2690)>(juniper::closures::closuret_4<t2690, t2690>(val1, val2), [](juniper::closures::closuret_4<t2690, t2690>& junclosure, t2691 event, t2690 prevVal) -> t2690 { 
                t2690& val1 = junclosure.val1;
                t2690& val2 = junclosure.val2;
                return (((bool) (prevVal == val1)) ? 
                    (([&]() -> t2690 {
                        return val2;
                    })())
                :
                    (([&]() -> t2690 {
                        return val1;
                    })()));
             }), state, incoming);
        })());
    }
}

namespace Io {
    Io::pinState toggle(Io::pinState p) {
        return (([&]() -> Io::pinState {
            Io::pinState guid113 = p;
            return (((bool) (((bool) ((guid113).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::pinState {
                    return low();
                })())
            :
                (((bool) (((bool) ((guid113).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> Io::pinState {
                        return high();
                    })())
                :
                    juniper::quit<Io::pinState>()));
        })());
    }
}

namespace Io {
    juniper::unit printStr(const char * str) {
        return (([&]() -> juniper::unit {
            Serial.print(str);
            return {};
        })());
    }
}

namespace Io {
    template<int c158>
    juniper::unit printCharList(juniper::records::recordt_0<juniper::array<uint8_t, c158>, uint32_t> cl) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c158;
            return (([&]() -> juniper::unit {
                Serial.print((char *) &cl.data[0]);
                return {};
            })());
        })());
    }
}

namespace Io {
    juniper::unit printFloat(float f) {
        return (([&]() -> juniper::unit {
            Serial.print(f);
            return {};
        })());
    }
}

namespace Io {
    juniper::unit printInt(int32_t n) {
        return (([&]() -> juniper::unit {
            Serial.print(n);
            return {};
        })());
    }
}

namespace Io {
    template<typename t2746>
    t2746 baseToInt(Io::base b) {
        return (([&]() -> t2746 {
            Io::base guid114 = b;
            return (((bool) (((bool) ((guid114).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> t2746 {
                    return ((t2746) 2);
                })())
            :
                (((bool) (((bool) ((guid114).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> t2746 {
                        return ((t2746) 8);
                    })())
                :
                    (((bool) (((bool) ((guid114).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> t2746 {
                            return ((t2746) 10);
                        })())
                    :
                        (((bool) (((bool) ((guid114).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> t2746 {
                                return ((t2746) 16);
                            })())
                        :
                            juniper::quit<t2746>()))));
        })());
    }
}

namespace Io {
    juniper::unit printIntBase(int32_t n, Io::base b) {
        return (([&]() -> juniper::unit {
            int32_t guid115 = baseToInt<int32_t>(b);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t bint = guid115;
            
            return (([&]() -> juniper::unit {
                Serial.print(n, bint);
                return {};
            })());
        })());
    }
}

namespace Io {
    juniper::unit printFloatPlaces(float f, int32_t numPlaces) {
        return (([&]() -> juniper::unit {
            Serial.print(f, numPlaces);
            return {};
        })());
    }
}

namespace Io {
    juniper::unit beginSerial(uint32_t speed) {
        return (([&]() -> juniper::unit {
            Serial.begin(speed);
            return {};
        })());
    }
}

namespace Io {
    uint8_t pinStateToInt(Io::pinState value) {
        return (([&]() -> uint8_t {
            Io::pinState guid116 = value;
            return (((bool) (((bool) ((guid116).id() == ((uint8_t) 1))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid116).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    juniper::quit<uint8_t>()));
        })());
    }
}

namespace Io {
    Io::pinState intToPinState(uint8_t value) {
        return (((bool) (value == ((uint8_t) 0))) ? 
            low()
        :
            high());
    }
}

namespace Io {
    juniper::unit digWrite(uint16_t pin, Io::pinState value) {
        return (([&]() -> juniper::unit {
            uint8_t guid117 = pinStateToInt(value);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t intVal = guid117;
            
            return (([&]() -> juniper::unit {
                digitalWrite(pin, intVal);
                return {};
            })());
        })());
    }
}

namespace Io {
    Io::pinState digRead(uint16_t pin) {
        return (([&]() -> Io::pinState {
            uint8_t intVal;
            
            (([&]() -> juniper::unit {
                intVal = digitalRead(pin);
                return {};
            })());
            return intToPinState(intVal);
        })());
    }
}

namespace Io {
    Prelude::sig<Io::pinState> digIn(uint16_t pin) {
        return signal<Io::pinState>(just<Io::pinState>(digRead(pin)));
    }
}

namespace Io {
    juniper::unit digOut(uint16_t pin, Prelude::sig<Io::pinState> sig) {
        return Signal::sink<Io::pinState, juniper::closures::closuret_5<uint16_t>>(juniper::function<juniper::closures::closuret_5<uint16_t>, juniper::unit(Io::pinState)>(juniper::closures::closuret_5<uint16_t>(pin), [](juniper::closures::closuret_5<uint16_t>& junclosure, Io::pinState value) -> juniper::unit { 
            uint16_t& pin = junclosure.pin;
            return digWrite(pin, value);
         }), sig);
    }
}

namespace Io {
    uint16_t anaRead(uint16_t pin) {
        return (([&]() -> uint16_t {
            uint16_t value;
            
            (([&]() -> juniper::unit {
                value = analogRead(pin);
                return {};
            })());
            return value;
        })());
    }
}

namespace Io {
    juniper::unit anaWrite(uint16_t pin, uint8_t value) {
        return (([&]() -> juniper::unit {
            analogWrite(pin, value);
            return {};
        })());
    }
}

namespace Io {
    Prelude::sig<uint16_t> anaIn(uint16_t pin) {
        return signal<uint16_t>(just<uint16_t>(anaRead(pin)));
    }
}

namespace Io {
    juniper::unit anaOut(uint16_t pin, Prelude::sig<uint8_t> sig) {
        return Signal::sink<uint8_t, juniper::closures::closuret_5<uint16_t>>(juniper::function<juniper::closures::closuret_5<uint16_t>, juniper::unit(uint8_t)>(juniper::closures::closuret_5<uint16_t>(pin), [](juniper::closures::closuret_5<uint16_t>& junclosure, uint8_t value) -> juniper::unit { 
            uint16_t& pin = junclosure.pin;
            return anaWrite(pin, value);
         }), sig);
    }
}

namespace Io {
    uint8_t pinModeToInt(Io::mode m) {
        return (([&]() -> uint8_t {
            Io::mode guid118 = m;
            return (((bool) (((bool) ((guid118).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> uint8_t {
                    return ((uint8_t) 0);
                })())
            :
                (((bool) (((bool) ((guid118).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (((bool) (((bool) ((guid118).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 2);
                        })())
                    :
                        juniper::quit<uint8_t>())));
        })());
    }
}

namespace Io {
    Io::mode intToPinMode(uint8_t m) {
        return (([&]() -> Io::mode {
            uint8_t guid119 = m;
            return (((bool) (((bool) (guid119 == ((uint8_t) 0))) && true)) ? 
                (([&]() -> Io::mode {
                    return input();
                })())
            :
                (((bool) (((bool) (guid119 == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> Io::mode {
                        return output();
                    })())
                :
                    (((bool) (((bool) (guid119 == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> Io::mode {
                            return inputPullup();
                        })())
                    :
                        juniper::quit<Io::mode>())));
        })());
    }
}

namespace Io {
    juniper::unit setPinMode(uint16_t pin, Io::mode m) {
        return (([&]() -> juniper::unit {
            uint8_t guid120 = pinModeToInt(m);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t m2 = guid120;
            
            return (([&]() -> juniper::unit {
                pinMode(pin, m2);
                return {};
            })());
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> risingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            Prelude::sig<Io::pinState> guid121 = sig;
            return (((bool) (((bool) ((guid121).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid121).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<juniper::unit> {
                    Io::pinState currState = ((guid121).signal()).just();
                    return (([&]() -> Prelude::sig<juniper::unit> {
                        Prelude::maybe<juniper::unit> guid122 = (([&]() -> Prelude::maybe<juniper::unit> {
                            juniper::tuple2<Io::pinState, Io::pinState> guid123 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, prevState});
                            return (((bool) (((bool) (((guid123).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid123).e1).id() == ((uint8_t) 0))) && true)))) ? 
                                (([&]() -> Prelude::maybe<juniper::unit> {
                                    return just<juniper::unit>(juniper::unit());
                                })())
                            :
                                (true ? 
                                    (([&]() -> Prelude::maybe<juniper::unit> {
                                        return nothing<juniper::unit>();
                                    })())
                                :
                                    juniper::quit<Prelude::maybe<juniper::unit>>()));
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<juniper::unit> ret = guid122;
                        
                        (prevState = currState);
                        return signal<juniper::unit>(ret);
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<juniper::unit> {
                        return signal<juniper::unit>(nothing<juniper::unit>());
                    })())
                :
                    juniper::quit<Prelude::sig<juniper::unit>>()));
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> fallingEdge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            Prelude::sig<Io::pinState> guid124 = sig;
            return (((bool) (((bool) ((guid124).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid124).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<juniper::unit> {
                    Io::pinState currState = ((guid124).signal()).just();
                    return (([&]() -> Prelude::sig<juniper::unit> {
                        Prelude::maybe<juniper::unit> guid125 = (([&]() -> Prelude::maybe<juniper::unit> {
                            juniper::tuple2<Io::pinState, Io::pinState> guid126 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, prevState});
                            return (((bool) (((bool) (((guid126).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid126).e1).id() == ((uint8_t) 1))) && true)))) ? 
                                (([&]() -> Prelude::maybe<juniper::unit> {
                                    return just<juniper::unit>(juniper::unit());
                                })())
                            :
                                (true ? 
                                    (([&]() -> Prelude::maybe<juniper::unit> {
                                        return nothing<juniper::unit>();
                                    })())
                                :
                                    juniper::quit<Prelude::maybe<juniper::unit>>()));
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<juniper::unit> ret = guid125;
                        
                        (prevState = currState);
                        return signal<juniper::unit>(ret);
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<juniper::unit> {
                        return signal<juniper::unit>(nothing<juniper::unit>());
                    })())
                :
                    juniper::quit<Prelude::sig<juniper::unit>>()));
        })());
    }
}

namespace Io {
    Prelude::sig<juniper::unit> edge(Prelude::sig<Io::pinState> sig, Io::pinState& prevState) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            Prelude::sig<Io::pinState> guid127 = sig;
            return (((bool) (((bool) ((guid127).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid127).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<juniper::unit> {
                    Io::pinState currState = ((guid127).signal()).just();
                    return (([&]() -> Prelude::sig<juniper::unit> {
                        Prelude::maybe<juniper::unit> guid128 = (([&]() -> Prelude::maybe<juniper::unit> {
                            juniper::tuple2<Io::pinState, Io::pinState> guid129 = (juniper::tuple2<Io::pinState,Io::pinState>{currState, prevState});
                            return (((bool) (((bool) (((guid129).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid129).e1).id() == ((uint8_t) 1))) && true)))) ? 
                                (([&]() -> Prelude::maybe<juniper::unit> {
                                    return just<juniper::unit>(juniper::unit());
                                })())
                            :
                                (((bool) (((bool) (((guid129).e2).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid129).e1).id() == ((uint8_t) 0))) && true)))) ? 
                                    (([&]() -> Prelude::maybe<juniper::unit> {
                                        return just<juniper::unit>(juniper::unit());
                                    })())
                                :
                                    (true ? 
                                        (([&]() -> Prelude::maybe<juniper::unit> {
                                            return nothing<juniper::unit>();
                                        })())
                                    :
                                        juniper::quit<Prelude::maybe<juniper::unit>>())));
                        })());
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<juniper::unit> ret = guid128;
                        
                        (prevState = currState);
                        return signal<juniper::unit>(ret);
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<juniper::unit> {
                        return signal<juniper::unit>(nothing<juniper::unit>());
                    })())
                :
                    juniper::quit<Prelude::sig<juniper::unit>>()));
        })());
    }
}

namespace Maybe {
    template<typename t3200, typename t3202, typename t3211>
    Prelude::maybe<t3211> map(juniper::function<t3202, t3211(t3200)> f, Prelude::maybe<t3200> maybeVal) {
        return (([&]() -> Prelude::maybe<t3211> {
            using a = t3200;
            using b = t3211;
            return (([&]() -> Prelude::maybe<t3211> {
                Prelude::maybe<t3200> guid130 = maybeVal;
                return (((bool) (((bool) ((guid130).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t3211> {
                        t3200 val = (guid130).just();
                        return just<t3211>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t3211> {
                            return nothing<t3211>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t3211>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3241>
    t3241 get(Prelude::maybe<t3241> maybeVal) {
        return (([&]() -> t3241 {
            using a = t3241;
            return (([&]() -> t3241 {
                Prelude::maybe<t3241> guid131 = maybeVal;
                return (((bool) (((bool) ((guid131).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t3241 {
                        t3241 val = (guid131).just();
                        return val;
                    })())
                :
                    juniper::quit<t3241>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3250>
    bool isJust(Prelude::maybe<t3250> maybeVal) {
        return (([&]() -> bool {
            using a = t3250;
            return (([&]() -> bool {
                Prelude::maybe<t3250> guid132 = maybeVal;
                return (((bool) (((bool) ((guid132).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> bool {
                        return true;
                    })())
                :
                    (true ? 
                        (([&]() -> bool {
                            return false;
                        })())
                    :
                        juniper::quit<bool>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3261>
    bool isNothing(Prelude::maybe<t3261> maybeVal) {
        return (([&]() -> bool {
            using a = t3261;
            return !(isJust<t3261>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t3275>
    uint8_t count(Prelude::maybe<t3275> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t3275;
            return (([&]() -> uint8_t {
                Prelude::maybe<t3275> guid133 = maybeVal;
                return (((bool) (((bool) ((guid133).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> uint8_t {
                        return ((uint8_t) 1);
                    })())
                :
                    (true ? 
                        (([&]() -> uint8_t {
                            return ((uint8_t) 0);
                        })())
                    :
                        juniper::quit<uint8_t>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3289, typename t3290, typename t3291>
    t3289 fold(juniper::function<t3291, t3289(t3290, t3289)> f, t3289 initState, Prelude::maybe<t3290> maybeVal) {
        return (([&]() -> t3289 {
            using state = t3289;
            using t = t3290;
            return (([&]() -> t3289 {
                Prelude::maybe<t3290> guid134 = maybeVal;
                return (((bool) (((bool) ((guid134).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t3289 {
                        t3290 val = (guid134).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t3289 {
                            return initState;
                        })())
                    :
                        juniper::quit<t3289>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3307, typename t3308>
    juniper::unit iter(juniper::function<t3308, juniper::unit(t3307)> f, Prelude::maybe<t3307> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t3307;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t3307> guid135 = maybeVal;
                return (((bool) (((bool) ((guid135).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> juniper::unit {
                        t3307 val = (guid135).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t3307> nothing = guid135;
                            return juniper::unit();
                        })())
                    :
                        juniper::quit<juniper::unit>()));
            })());
        })());
    }
}

namespace Time {
    juniper::unit wait(uint32_t time) {
        return (([&]() -> juniper::unit {
            delay(time);
            return {};
        })());
    }
}

namespace Time {
    uint32_t now() {
        return (([&]() -> uint32_t {
            uint32_t guid136 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t ret = guid136;
            
            (([&]() -> juniper::unit {
                ret = millis();
                return {};
            })());
            return ret;
        })());
    }
}

namespace Time {
    juniper::records::recordt_1<uint32_t> state = (([]() -> juniper::records::recordt_1<uint32_t>{
        juniper::records::recordt_1<uint32_t> guid137;
        guid137.lastPulse = ((uint32_t) 0);
        return guid137;
    })());
}

namespace Time {
    Prelude::sig<uint32_t> every(uint32_t interval, juniper::records::recordt_1<uint32_t>& state) {
        return (([&]() -> Prelude::sig<uint32_t> {
            uint32_t guid138 = now();
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t t = guid138;
            
            uint32_t guid139 = (((bool) (interval == ((uint32_t) 0))) ? 
                (([&]() -> uint32_t {
                    return t;
                })())
            :
                (([&]() -> uint32_t {
                    return ((uint32_t) (((uint32_t) (t / interval)) * interval));
                })()));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t lastWindow = guid139;
            
            return (((bool) ((state).lastPulse >= lastWindow)) ? 
                (([&]() -> Prelude::sig<uint32_t> {
                    return signal<uint32_t>(nothing<uint32_t>());
                })())
            :
                (([&]() -> Prelude::sig<uint32_t> {
                    ((state).lastPulse = t);
                    return signal<uint32_t>(just<uint32_t>(t));
                })()));
        })());
    }
}

namespace Math {
    double pi = ((double) 3.141592653589793238462643383279502884197169399375105820974);
}

namespace Math {
    double e = ((double) 2.718281828459045235360287471352662497757247093699959574966);
}

namespace Math {
    double degToRad(double degrees) {
        return ((double) (degrees * ((double) 0.017453292519943295769236907684886)));
    }
}

namespace Math {
    double radToDeg(double radians) {
        return ((double) (radians * ((double) 57.295779513082320876798154814105)));
    }
}

namespace Math {
    double acos_(double x) {
        return (([&]() -> double {
            double guid140 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid140;
            
            (([&]() -> juniper::unit {
                ret = acos(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double asin_(double x) {
        return (([&]() -> double {
            double guid141 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid141;
            
            (([&]() -> juniper::unit {
                ret = asin(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double atan_(double x) {
        return (([&]() -> double {
            double guid142 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid142;
            
            (([&]() -> juniper::unit {
                ret = atan(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double atan2_(double y, double x) {
        return (([&]() -> double {
            double guid143 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid143;
            
            (([&]() -> juniper::unit {
                ret = atan2(y, x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double cos_(double x) {
        return (([&]() -> double {
            double guid144 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid144;
            
            (([&]() -> juniper::unit {
                ret = cos(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double cosh_(double x) {
        return (([&]() -> double {
            double guid145 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid145;
            
            (([&]() -> juniper::unit {
                ret = cosh(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double sin_(double x) {
        return (([&]() -> double {
            double guid146 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid146;
            
            (([&]() -> juniper::unit {
                ret = sin(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double sinh_(double x) {
        return (([&]() -> double {
            double guid147 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid147;
            
            (([&]() -> juniper::unit {
                ret = sinh(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double tan_(double x) {
        return ((double) (sin_(x) / cos_(x)));
    }
}

namespace Math {
    double tanh_(double x) {
        return (([&]() -> double {
            double guid148 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid148;
            
            (([&]() -> juniper::unit {
                ret = tanh(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double exp_(double x) {
        return (([&]() -> double {
            double guid149 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid149;
            
            (([&]() -> juniper::unit {
                ret = exp(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    juniper::tuple2<double, int32_t> frexp_(double x) {
        return (([&]() -> juniper::tuple2<double, int32_t> {
            double guid150 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid150;
            
            int32_t guid151 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t exponent = guid151;
            
            (([&]() -> juniper::unit {
                int exponent2;
    ret = frexp(x, &exponent2);
    exponent = (int32_t) exponent2;
                return {};
            })());
            return (juniper::tuple2<double,int32_t>{ret, exponent});
        })());
    }
}

namespace Math {
    double ldexp_(double x, int16_t exponent) {
        return (([&]() -> double {
            double guid152 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid152;
            
            (([&]() -> juniper::unit {
                ret = ldexp(x, exponent);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double log_(double x) {
        return (([&]() -> double {
            double guid153 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid153;
            
            (([&]() -> juniper::unit {
                ret = log(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double log10_(double x) {
        return (([&]() -> double {
            double guid154 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid154;
            
            (([&]() -> juniper::unit {
                ret = log10(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    juniper::tuple2<double, double> modf_(double x) {
        return (([&]() -> juniper::tuple2<double, double> {
            double guid155 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid155;
            
            double guid156 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double integer = guid156;
            
            (([&]() -> juniper::unit {
                ret = modf(x, &integer);
                return {};
            })());
            return (juniper::tuple2<double,double>{ret, integer});
        })());
    }
}

namespace Math {
    double pow_(double x, double y) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = pow(x, y);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double sqrt_(double x) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = sqrt(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double ceil_(double x) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = ceil(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double fabs_(double x) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = fabs(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    template<typename t3452>
    t3452 abs_(t3452 x) {
        return (([&]() -> t3452 {
            using t = t3452;
            return (([&]() -> t3452 {
                return (((bool) (x < ((t3452) 0))) ? 
                    (([&]() -> t3452 {
                        return -(x);
                    })())
                :
                    (([&]() -> t3452 {
                        return x;
                    })()));
            })());
        })());
    }
}

namespace Math {
    double floor_(double x) {
        return (([&]() -> double {
            double ret;
            
            (([&]() -> juniper::unit {
                ret = floor(x);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double fmod_(double x, double y) {
        return (([&]() -> double {
            double guid157 = ((double) 0.0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            double ret = guid157;
            
            (([&]() -> juniper::unit {
                ret = fmod(x, y);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Math {
    double round_(double x) {
        return floor_(((double) (x + ((double) 0.5))));
    }
}

namespace Math {
    template<typename t3463>
    t3463 max_(t3463 x, t3463 y) {
        return (([&]() -> t3463 {
            using a = t3463;
            return (((bool) (x > y)) ? 
                (([&]() -> t3463 {
                    return x;
                })())
            :
                (([&]() -> t3463 {
                    return y;
                })()));
        })());
    }
}

namespace Math {
    template<typename t3465>
    t3465 mapRange(t3465 x, t3465 a1, t3465 a2, t3465 b1, t3465 b2) {
        return (([&]() -> t3465 {
            using t = t3465;
            return ((t3465) (b1 + ((t3465) (((t3465) (((t3465) (x - a1)) * ((t3465) (b2 - b1)))) / ((t3465) (a2 - a1))))));
        })());
    }
}

namespace Math {
    template<typename t3467>
    t3467 clamp(t3467 x, t3467 min, t3467 max) {
        return (([&]() -> t3467 {
            using a = t3467;
            return (((bool) (min > x)) ? 
                (([&]() -> t3467 {
                    return min;
                })())
            :
                (((bool) (x > max)) ? 
                    (([&]() -> t3467 {
                        return max;
                    })())
                :
                    (([&]() -> t3467 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t3472>
    int8_t sign(t3472 n) {
        return (([&]() -> int8_t {
            using a = t3472;
            return (((bool) (n == ((t3472) 0))) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                (((bool) (n > ((t3472) 0))) ? 
                    (([&]() -> int8_t {
                        return ((int8_t) 1);
                    })())
                :
                    (([&]() -> int8_t {
                        return -(((int8_t) 1));
                    })())));
        })());
    }
}

namespace Button {
    juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> state = (([]() -> juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>{
        juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid158;
        guid158.actualState = Io::low();
        guid158.lastState = Io::low();
        guid158.lastDebounceTime = ((uint32_t) 0);
        return guid158;
    })());
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState) {
        return (([&]() -> Prelude::sig<Io::pinState> {
            Prelude::sig<Io::pinState> guid159 = incoming;
            return (((bool) (((bool) ((guid159).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid159).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                (([&]() -> Prelude::sig<Io::pinState> {
                    Io::pinState currentState = ((guid159).signal()).just();
                    return (([&]() -> Prelude::sig<Io::pinState> {
                        juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState> guid160 = buttonState;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        uint32_t lastDebounceTime = (guid160).lastDebounceTime;
                        Io::pinState lastState = (guid160).lastState;
                        Io::pinState actualState = (guid160).actualState;
                        
                        Io::pinState guid161 = (((bool) (currentState != lastState)) ? 
                            (([&]() -> Io::pinState {
                                ((buttonState).lastState = currentState);
                                ((buttonState).lastDebounceTime = Time::now());
                                return actualState;
                            })())
                        :
                            (((bool) (((bool) (currentState != actualState)) && ((bool) (((uint32_t) (Time::now() - (buttonState).lastDebounceTime)) > delay)))) ? 
                                (([&]() -> Io::pinState {
                                    ((buttonState).actualState = currentState);
                                    ((buttonState).lastState = currentState);
                                    return currentState;
                                })())
                            :
                                (([&]() -> Io::pinState {
                                    ((buttonState).lastState = currentState);
                                    return actualState;
                                })())));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Io::pinState retState = guid161;
                        
                        return signal<Io::pinState>(just<Io::pinState>(retState));
                    })());
                })())
            :
                (true ? 
                    (([&]() -> Prelude::sig<Io::pinState> {
                        return incoming;
                    })())
                :
                    juniper::quit<Prelude::sig<Io::pinState>>()));
        })());
    }
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState) {
        return debounceDelay(incoming, ((uint16_t) 50), buttonState);
    }
}

namespace Vector {
    uint8_t x = ((uint8_t) 0);
}

namespace Vector {
    uint8_t y = ((uint8_t) 1);
}

namespace Vector {
    uint8_t z = ((uint8_t) 2);
}

namespace Vector {
    template<typename t3592, int c159>
    juniper::array<t3592, c159> add(juniper::array<t3592, c159> v1, juniper::array<t3592, c159> v2) {
        return (([&]() -> juniper::array<t3592, c159> {
            using a = t3592;
            constexpr int32_t n = c159;
            return (([&]() -> juniper::array<t3592, c159> {
                (([&]() -> juniper::unit {
                    int32_t guid162 = ((int32_t) 0);
                    int32_t guid163 = n;
                    for (int32_t i = guid162; i < guid163; i++) {
                        (([&]() -> t3592 {
                            return ((v1)[i] += (v2)[i]);
                        })());
                    }
                    return {};
                })());
                return v1;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3596, int c162>
    juniper::array<t3596, c162> zero() {
        return (([&]() -> juniper::array<t3596, c162> {
            using a = t3596;
            constexpr int32_t n = c162;
            return array<t3596, c162>(((t3596) 0));
        })());
    }
}

namespace Vector {
    template<typename t3604, int c164>
    juniper::array<t3604, c164> subtract(juniper::array<t3604, c164> v1, juniper::array<t3604, c164> v2) {
        return (([&]() -> juniper::array<t3604, c164> {
            using a = t3604;
            constexpr int32_t n = c164;
            return (([&]() -> juniper::array<t3604, c164> {
                (([&]() -> juniper::unit {
                    int32_t guid164 = ((int32_t) 0);
                    int32_t guid165 = n;
                    for (int32_t i = guid164; i < guid165; i++) {
                        (([&]() -> t3604 {
                            return ((v1)[i] -= (v2)[i]);
                        })());
                    }
                    return {};
                })());
                return v1;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3608, int c167>
    juniper::array<t3608, c167> scale(t3608 scalar, juniper::array<t3608, c167> v) {
        return (([&]() -> juniper::array<t3608, c167> {
            using a = t3608;
            constexpr int32_t n = c167;
            return (([&]() -> juniper::array<t3608, c167> {
                (([&]() -> juniper::unit {
                    int32_t guid166 = ((int32_t) 0);
                    int32_t guid167 = n;
                    for (int32_t i = guid166; i < guid167; i++) {
                        (([&]() -> t3608 {
                            return ((v)[i] *= scalar);
                        })());
                    }
                    return {};
                })());
                return v;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3614, int c169>
    t3614 dot(juniper::array<t3614, c169> v1, juniper::array<t3614, c169> v2) {
        return (([&]() -> t3614 {
            using a = t3614;
            constexpr int32_t n = c169;
            return (([&]() -> t3614 {
                t3614 guid168 = ((t3614) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t3614 sum = guid168;
                
                (([&]() -> juniper::unit {
                    int32_t guid169 = ((int32_t) 0);
                    int32_t guid170 = n;
                    for (int32_t i = guid169; i < guid170; i++) {
                        (([&]() -> t3614 {
                            return (sum += ((t3614) ((v1)[i] * (v2)[i])));
                        })());
                    }
                    return {};
                })());
                return sum;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3620, int c172>
    t3620 magnitude2(juniper::array<t3620, c172> v) {
        return (([&]() -> t3620 {
            using a = t3620;
            constexpr int32_t n = c172;
            return (([&]() -> t3620 {
                t3620 guid171 = ((t3620) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t3620 sum = guid171;
                
                (([&]() -> juniper::unit {
                    int32_t guid172 = ((int32_t) 0);
                    int32_t guid173 = n;
                    for (int32_t i = guid172; i < guid173; i++) {
                        (([&]() -> t3620 {
                            return (sum += ((t3620) ((v)[i] * (v)[i])));
                        })());
                    }
                    return {};
                })());
                return sum;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3622, int c175>
    double magnitude(juniper::array<t3622, c175> v) {
        return (([&]() -> double {
            using a = t3622;
            constexpr int32_t n = c175;
            return sqrt_(toDouble<t3622>(magnitude2<t3622, c175>(v)));
        })());
    }
}

namespace Vector {
    template<typename t3638, int c177>
    juniper::array<t3638, c177> multiply(juniper::array<t3638, c177> u, juniper::array<t3638, c177> v) {
        return (([&]() -> juniper::array<t3638, c177> {
            using a = t3638;
            constexpr int32_t n = c177;
            return (([&]() -> juniper::array<t3638, c177> {
                (([&]() -> juniper::unit {
                    int32_t guid174 = ((int32_t) 0);
                    int32_t guid175 = n;
                    for (int32_t i = guid174; i < guid175; i++) {
                        (([&]() -> t3638 {
                            return ((u)[i] *= (v)[i]);
                        })());
                    }
                    return {};
                })());
                return u;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3647, int c180>
    juniper::array<t3647, c180> normalize(juniper::array<t3647, c180> v) {
        return (([&]() -> juniper::array<t3647, c180> {
            using a = t3647;
            constexpr int32_t n = c180;
            return (([&]() -> juniper::array<t3647, c180> {
                double guid176 = magnitude<t3647, c180>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid176;
                
                (([&]() -> juniper::unit {
                    if (((bool) (mag > ((t3647) 0)))) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                int32_t guid177 = ((int32_t) 0);
                                int32_t guid178 = n;
                                for (int32_t i = guid177; i < guid178; i++) {
                                    (([&]() -> t3647 {
                                        return ((v)[i] = fromDouble<t3647>(((double) (toDouble<t3647>((v)[i]) / mag))));
                                    })());
                                }
                                return {};
                            })());
                        })());
                    }
                    return {};
                })());
                return v;
            })());
        })());
    }
}

namespace Vector {
    template<typename t3658, int c184>
    double angle(juniper::array<t3658, c184> v1, juniper::array<t3658, c184> v2) {
        return (([&]() -> double {
            using a = t3658;
            constexpr int32_t n = c184;
            return acos_(((double) (toDouble<t3658>(dot<t3658, c184>(v1, v2)) / sqrt_(toDouble<t3658>(((t3658) (magnitude2<t3658, c184>(v1) * magnitude2<t3658, c184>(v2))))))));
        })());
    }
}

namespace Vector {
    template<typename t3700>
    juniper::array<t3700, 3> cross(juniper::array<t3700, 3> u, juniper::array<t3700, 3> v) {
        return (([&]() -> juniper::array<t3700, 3> {
            using a = t3700;
            return (juniper::array<t3700, 3> { {((t3700) (((t3700) ((u)[y] * (v)[z])) - ((t3700) ((u)[z] * (v)[y])))), ((t3700) (((t3700) ((u)[z] * (v)[x])) - ((t3700) ((u)[x] * (v)[z])))), ((t3700) (((t3700) ((u)[x] * (v)[y])) - ((t3700) ((u)[y] * (v)[x]))))} });
        })());
    }
}

namespace Vector {
    template<typename t3702, int c200>
    juniper::array<t3702, c200> project(juniper::array<t3702, c200> a, juniper::array<t3702, c200> b) {
        return (([&]() -> juniper::array<t3702, c200> {
            using z = t3702;
            constexpr int32_t n = c200;
            return (([&]() -> juniper::array<t3702, c200> {
                juniper::array<t3702, c200> guid179 = normalize<t3702, c200>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t3702, c200> bn = guid179;
                
                return scale<t3702, c200>(dot<t3702, c200>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t3718, int c204>
    juniper::array<t3718, c204> projectPlane(juniper::array<t3718, c204> a, juniper::array<t3718, c204> m) {
        return (([&]() -> juniper::array<t3718, c204> {
            using z = t3718;
            constexpr int32_t n = c204;
            return subtract<t3718, c204>(a, project<t3718, c204>(a, m));
        })());
    }
}

namespace CharList {
    template<int c207>
    juniper::records::recordt_0<juniper::array<uint8_t, c207>, uint32_t> toUpper(juniper::records::recordt_0<juniper::array<uint8_t, c207>, uint32_t> str) {
        return List::map<uint8_t, void, uint8_t, c207>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 97))) && ((bool) (c <= ((uint8_t) 122))))) ? 
                ((uint8_t) (c - ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c208>
    juniper::records::recordt_0<juniper::array<uint8_t, c208>, uint32_t> toLower(juniper::records::recordt_0<juniper::array<uint8_t, c208>, uint32_t> str) {
        return List::map<uint8_t, void, uint8_t, c208>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t c) -> uint8_t { 
            return (((bool) (((bool) (c >= ((uint8_t) 65))) && ((bool) (c <= ((uint8_t) 90))))) ? 
                ((uint8_t) (c + ((uint8_t) 32)))
            :
                c);
         }), str);
    }
}

namespace CharList {
    template<int c209>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> i32ToCharList(int32_t m) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t n = c209;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> guid180 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> guid181;
                    guid181.data = array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>(((uint8_t) 0));
                    guid181.length = ((uint32_t) 0);
                    return guid181;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c209)*(-1)))*(-1)>, uint32_t> ret = guid180;
                
                (([&]() -> juniper::unit {
                    
    int charsPrinted = 1 + snprintf((char *) &ret.data[0], n + 1, "%d", m);
    ret.length = charsPrinted >= (n + 1) ? (n + 1) : charsPrinted;
    
                    return {};
                })());
                return ret;
            })());
        })());
    }
}

namespace CharList {
    template<int c211>
    uint32_t length(juniper::records::recordt_0<juniper::array<uint8_t, c211>, uint32_t> s) {
        return (([&]() -> uint32_t {
            constexpr int32_t n = c211;
            return ((uint32_t) ((s).length - ((uint32_t) 1)));
        })());
    }
}

namespace CharList {
    template<int c212, int c213, int c214>
    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> concat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c212)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c213)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c212;
            constexpr int32_t bCap = c213;
            constexpr int32_t retCap = c214;
            return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> {
                uint32_t guid182 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid182;
                
                uint32_t guid183 = length<(1)+(c212)>(sA);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenA = guid183;
                
                uint32_t guid184 = length<(1)+(c213)>(sB);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t lenB = guid184;
                
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> guid185 = (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> guid186;
                    guid186.data = array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>(((uint8_t) 0));
                    guid186.length = ((uint32_t) (((uint32_t) (lenA + lenB)) + ((uint32_t) 1)));
                    return guid186;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<uint8_t, ((-1)+((c214)*(-1)))*(-1)>, uint32_t> out = guid185;
                
                (([&]() -> juniper::unit {
                    uint32_t guid187 = ((uint32_t) 0);
                    uint32_t guid188 = lenA;
                    for (uint32_t i = guid187; i < guid188; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sA).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                (([&]() -> juniper::unit {
                    uint32_t guid189 = ((uint32_t) 0);
                    uint32_t guid190 = lenB;
                    for (uint32_t i = guid189; i < guid190; i++) {
                        (([&]() -> uint32_t {
                            (((out).data)[j] = ((sB).data)[i]);
                            return (j += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return out;
            })());
        })());
    }
}

namespace CharList {
    template<int c222, int c223>
    juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c222)*(-1)))+((c223)*(-1)))*(-1)>, uint32_t> safeConcat(juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c222)>, uint32_t> sA, juniper::records::recordt_0<juniper::array<uint8_t, (1)+(c223)>, uint32_t> sB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<uint8_t, (((-1)+((c222)*(-1)))+((c223)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t aCap = c222;
            constexpr int32_t bCap = c223;
            return concat<c222, c223, (((c222)*(-1))+((c223)*(-1)))*(-1)>(sA, sB);
        })());
    }
}

namespace Random {
    int32_t random_(int32_t low, int32_t high) {
        return (([&]() -> int32_t {
            int32_t guid191 = ((int32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            int32_t ret = guid191;
            
            (([&]() -> juniper::unit {
                ret = random(low, high);
                return {};
            })());
            return ret;
        })());
    }
}

namespace Random {
    juniper::unit seed(uint32_t n) {
        return (([&]() -> juniper::unit {
            randomSeed(n);
            return {};
        })());
    }
}

namespace Random {
    template<typename t3796, int c227>
    t3796 choice(juniper::records::recordt_0<juniper::array<t3796, c227>, uint32_t> lst) {
        return (([&]() -> t3796 {
            using t = t3796;
            constexpr int32_t n = c227;
            return (([&]() -> t3796 {
                int32_t guid192 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid192;
                
                return List::get<uint32_t, t3796, c227>(i32ToU32(i), lst);
            })());
        })());
    }
}

namespace Random {
    template<typename t3816, int c229>
    Prelude::maybe<t3816> tryChoice(juniper::records::recordt_0<juniper::array<t3816, c229>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t3816> {
            using t = t3816;
            constexpr int32_t n = c229;
            return (([&]() -> Prelude::maybe<t3816> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t3816> {
                        return nothing<t3816>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t3816> {
                        int32_t guid193 = random_(((int32_t) 0), u32ToI32((lst).length));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        int32_t i = guid193;
                        
                        return List::tryGet<uint32_t, t3816, c229>(i32ToU32(i), lst);
                    })()));
            })());
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color) {
        return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> {
            juniper::records::recordt_5<float, float, float> guid194 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float v = (guid194).v;
            float s = (guid194).s;
            float h = (guid194).h;
            
            float guid195 = ((float) (v * s));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float c = guid195;
            
            float guid196 = ((float) (c * toFloat<double>(((double) (((double) 1.0) - Math::fabs_(((double) (Math::fmod_(((double) (toDouble<float>(h) / ((double) 60))), ((double) 2.0)) - ((double) 1.0)))))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float x = guid196;
            
            float guid197 = ((float) (v - c));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float m = guid197;
            
            juniper::tuple3<float, float, float> guid198 = (((bool) (((bool) (0.0f <= h)) && ((bool) (h < 60.0f)))) ? 
                (juniper::tuple3<float,float,float>{c, x, 0.0f})
            :
                (((bool) (((bool) (60.0f <= h)) && ((bool) (h < 120.0f)))) ? 
                    (juniper::tuple3<float,float,float>{x, c, 0.0f})
                :
                    (((bool) (((bool) (120.0f <= h)) && ((bool) (h < 180.0f)))) ? 
                        (juniper::tuple3<float,float,float>{0.0f, c, x})
                    :
                        (((bool) (((bool) (180.0f <= h)) && ((bool) (h < 240.0f)))) ? 
                            (juniper::tuple3<float,float,float>{0.0f, x, c})
                        :
                            (((bool) (((bool) (240.0f <= h)) && ((bool) (h < 300.0f)))) ? 
                                (juniper::tuple3<float,float,float>{x, 0.0f, c})
                            :
                                (juniper::tuple3<float,float,float>{c, 0.0f, x}))))));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float bPrime = (guid198).e3;
            float gPrime = (guid198).e2;
            float rPrime = (guid198).e1;
            
            float guid199 = ((float) (((float) (rPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float r = guid199;
            
            float guid200 = ((float) (((float) (gPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float g = guid200;
            
            float guid201 = ((float) (((float) (bPrime + m)) * 255.0f));
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            float b = guid201;
            
            return (([&]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
                juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid202;
                guid202.r = toUInt8<float>(r);
                guid202.g = toUInt8<float>(g);
                guid202.b = toUInt8<float>(b);
                return guid202;
            })());
        })());
    }
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color) {
        return (([&]() -> uint16_t {
            juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid203 = color;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = (guid203).b;
            uint8_t g = (guid203).g;
            uint8_t r = (guid203).r;
            
            return ((uint16_t) (((uint16_t) (((uint16_t) (((uint16_t) (u8ToU16(r) & ((uint16_t) 248))) << ((uint32_t) 8))) | ((uint16_t) (((uint16_t) (u8ToU16(g) & ((uint16_t) 252))) << ((uint32_t) 3))))) | ((uint16_t) (u8ToU16(b) >> ((uint32_t) 3)))));
        })());
    }
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> red = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid204;
        guid204.r = ((uint8_t) 255);
        guid204.g = ((uint8_t) 0);
        guid204.b = ((uint8_t) 0);
        return guid204;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> green = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid205;
        guid205.r = ((uint8_t) 0);
        guid205.g = ((uint8_t) 255);
        guid205.b = ((uint8_t) 0);
        return guid205;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> blue = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid206;
        guid206.r = ((uint8_t) 0);
        guid206.g = ((uint8_t) 0);
        guid206.b = ((uint8_t) 255);
        return guid206;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> black = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid207;
        guid207.r = ((uint8_t) 0);
        guid207.g = ((uint8_t) 0);
        guid207.b = ((uint8_t) 0);
        return guid207;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> white = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid208;
        guid208.r = ((uint8_t) 255);
        guid208.g = ((uint8_t) 255);
        guid208.b = ((uint8_t) 255);
        return guid208;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> yellow = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid209;
        guid209.r = ((uint8_t) 255);
        guid209.g = ((uint8_t) 255);
        guid209.b = ((uint8_t) 0);
        return guid209;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> magenta = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid210;
        guid210.r = ((uint8_t) 255);
        guid210.g = ((uint8_t) 0);
        guid210.b = ((uint8_t) 255);
        return guid210;
    })());
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> cyan = (([]() -> juniper::records::recordt_3<uint8_t, uint8_t, uint8_t>{
        juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> guid211;
        guid211.r = ((uint8_t) 0);
        guid211.g = ((uint8_t) 255);
        guid211.b = ((uint8_t) 255);
        return guid211;
    })());
}

namespace ListExt {
    template<typename t3947, int c231>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t3947>, c231>, uint32_t> enumerated(juniper::records::recordt_0<juniper::array<t3947, c231>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t3947>, c231>, uint32_t> {
            using a = t3947;
            constexpr int32_t n = c231;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t3947>, c231>, uint32_t> {
                uint32_t guid212 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t offset = guid212;
                
                juniper::array<juniper::tuple2<uint32_t, t3947>, c231> guid213 = zeros<juniper::tuple2<uint32_t, t3947>, c231>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<juniper::tuple2<uint32_t, t3947>, c231> result = guid213;
                
                (([&]() -> juniper::unit {
                    uint32_t guid214 = ((uint32_t) 0);
                    uint32_t guid215 = cast<uint32_t, uint32_t>((lst).length);
                    for (uint32_t i = guid214; i < guid215; i++) {
                        (([&]() -> uint32_t {
                            ((result)[i] = (juniper::tuple2<uint32_t,t3947>{offset, ((lst).data)[i]}));
                            return (offset += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t3947>, c231>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t3947>, c231>, uint32_t> guid216;
                    guid216.data = result;
                    guid216.length = cast<int32_t, uint32_t>(n);
                    return guid216;
                })());
            })());
        })());
    }
}

namespace ListExt {
    template<typename t3991, int c235>
    juniper::unit rotate(int32_t step, juniper::records::recordt_0<juniper::array<t3991, c235>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using a = t3991;
            constexpr int32_t n = c235;
            return (([&]() -> juniper::unit {
                return (([&]() -> juniper::unit {
                    int32_t guid217 = n;
                    return (((bool) (((bool) (guid217 == ((int32_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return juniper::unit();
                        })())
                    :
                        (true ? 
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    int32_t guid218 = ((int32_t) (((int32_t) (((int32_t) (step % cast<int32_t, int32_t>(n))) + cast<int32_t, int32_t>(n))) % cast<int32_t, int32_t>(n)));
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    int32_t normalizedStep = guid218;
                                    
                                    juniper::array<t3991, c235> guid219 = zeros<t3991, c235>();
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    juniper::array<t3991, c235> result = guid219;
                                    
                                    (([&]() -> juniper::unit {
                                        int32_t guid220 = ((int32_t) 0);
                                        int32_t guid221 = cast<uint32_t, int32_t>((lst).length);
                                        for (int32_t i = guid220; i < guid221; i++) {
                                            (([&]() -> t3991 {
                                                return ((result)[i] = ((lst).data)[((int32_t) (((int32_t) (i + normalizedStep)) % cast<int32_t, int32_t>(n)))]);
                                            })());
                                        }
                                        return {};
                                    })());
                                    ((lst).data = result);
                                    return juniper::unit();
                                })());
                            })())
                        :
                            juniper::quit<juniper::unit>()));
                })());
            })());
        })());
    }
}

namespace ListExt {
    template<typename t4028, int c239>
    juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> rotated(int32_t step, juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> {
            using a = t4028;
            constexpr int32_t n = c239;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> {
                return (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> {
                    int32_t guid222 = n;
                    return (((bool) (((bool) (guid222 == ((int32_t) 0))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> {
                            return lst;
                        })())
                    :
                        (true ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> {
                                    int32_t guid223 = ((int32_t) (((int32_t) (((int32_t) (step % cast<int32_t, int32_t>(n))) + cast<int32_t, int32_t>(n))) % cast<int32_t, int32_t>(n)));
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    int32_t normalizedStep = guid223;
                                    
                                    juniper::array<t4028, c239> guid224 = zeros<t4028, c239>();
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    juniper::array<t4028, c239> result = guid224;
                                    
                                    (([&]() -> juniper::unit {
                                        int32_t guid225 = ((int32_t) 0);
                                        int32_t guid226 = cast<uint32_t, int32_t>((lst).length);
                                        for (int32_t i = guid225; i < guid226; i++) {
                                            (([&]() -> t4028 {
                                                return ((result)[i] = ((lst).data)[((int32_t) (((int32_t) (i + normalizedStep)) % cast<int32_t, int32_t>(n)))]);
                                            })());
                                        }
                                        return {};
                                    })());
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t> guid227;
                                        guid227.data = result;
                                        guid227.length = cast<int32_t, uint32_t>(n);
                                        return guid227;
                                    })());
                                })());
                            })())
                        :
                            juniper::quit<juniper::records::recordt_0<juniper::array<t4028, c239>, uint32_t>>()));
                })());
            })());
        })());
    }
}

namespace ListExt {
    template<typename t4049, int c243, int c244>
    juniper::records::recordt_0<juniper::array<t4049, c243>, uint32_t> replicateList(uint32_t nElements, juniper::records::recordt_0<juniper::array<t4049, c244>, uint32_t> elements) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t4049, c243>, uint32_t> {
            using t = t4049;
            constexpr int32_t m = c243;
            constexpr int32_t n = c244;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t4049, c243>, uint32_t> {
                juniper::array<t4049, c243> guid228 = zeros<t4049, c243>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t4049, c243> ret = guid228;
                
                (([&]() -> juniper::unit {
                    int32_t guid229 = ((int32_t) 0);
                    int32_t guid230 = m;
                    for (int32_t i = guid229; i < guid230; i++) {
                        (([&]() -> t4049 {
                            return ((ret)[i] = ((elements).data)[((int32_t) (i % cast<uint32_t, int32_t>((elements).length)))]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t4049, c243>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t4049, c243>, uint32_t> guid231;
                    guid231.data = ret;
                    guid231.length = nElements;
                    return guid231;
                })());
            })());
        })());
    }
}

namespace SignalExt {
    template<typename t4067>
    Prelude::sig<t4067> once(Prelude::maybe<t4067>& state) {
        return (([&]() -> Prelude::sig<t4067> {
            using t = t4067;
            return (([&]() -> Prelude::sig<t4067> {
                return (([&]() -> Prelude::sig<t4067> {
                    Prelude::maybe<t4067> guid232 = state;
                    return (((bool) (((bool) ((guid232).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> Prelude::sig<t4067> {
                            t4067 value = (guid232).just();
                            return (([&]() -> Prelude::sig<t4067> {
                                (state = nothing<t4067>());
                                return signal<t4067>(just<t4067>(value));
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid232).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::sig<t4067> {
                                return signal<t4067>(nothing<t4067>());
                            })())
                        :
                            juniper::quit<Prelude::sig<t4067>>()));
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    Prelude::maybe<NeoPixel::Action> startAction = just<NeoPixel::Action>(start());
}

namespace NeoPixel {
    Prelude::maybe<NeoPixel::Action> firstAction = just<NeoPixel::Action>(run(((uint8_t) 0), alternate(RGB(((uint8_t) 0), ((uint8_t) 0), ((uint8_t) 255)), RGB(((uint8_t) 0), ((uint8_t) 0), ((uint8_t) 0)))));
}

namespace NeoPixel {
    Prelude::sig<Prelude::maybe<NeoPixel::Action>> actions(Prelude::maybe<NeoPixel::Action>& prevAction) {
        return (([&]() -> Prelude::sig<Prelude::maybe<NeoPixel::Action>> {
            return Signal::meta<NeoPixel::Action>(Signal::mergeMany<NeoPixel::Action, 3>((([&]() -> juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Action>, 3>, uint32_t>{
                juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Action>, 3>, uint32_t> guid233;
                guid233.data = (juniper::array<Prelude::sig<NeoPixel::Action>, 3> { {SignalExt::once<NeoPixel::Action>(startAction), SignalExt::once<NeoPixel::Action>(firstAction), Signal::constant<NeoPixel::Action>(repeat(((uint8_t) 0), rotate(((int16_t) 1)), ((uint32_t) 500), nothing<uint8_t>()))} });
                guid233.length = ((uint32_t) 2);
                return guid233;
            })())));
        })());
    }
}

namespace NeoPixel {
    NeoPixel::RawDevice makeDevice(uint16_t pin, uint16_t pixels) {
        return (([&]() -> NeoPixel::RawDevice {
            void * ret;
            
            (([&]() -> juniper::unit {
                 ret = new Adafruit_NeoPixel(pixels, pin, NEO_GRB + NEO_KHZ800); 
                return {};
            })());
            return NeoPixel::device(ret);
        })());
    }
}

namespace NeoPixel {
    NeoPixel::color getPixelColor(uint16_t n, NeoPixel::RawDevice line) {
        return (([&]() -> NeoPixel::color {
            NeoPixel::RawDevice guid234 = line;
            if (!(((bool) (((bool) ((guid234).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid234).device();
            
            uint32_t guid235 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t numRep = guid235;
            
            (([&]() -> juniper::unit {
                 numRep = ((Adafruit_NeoPixel*) p)->getPixelColor(n); 
                return {};
            })());
            return RGB(toUInt8<uint32_t>(((uint32_t) (numRep >> ((uint32_t) 16)))), toUInt8<uint32_t>(((uint32_t) (numRep >> ((uint32_t) 8)))), toUInt8<uint32_t>(numRep));
        })());
    }
}

namespace NeoPixel {
    template<int c249>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c249>, uint32_t> readPixels(NeoPixel::RawDevice device) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c249>, uint32_t> {
            constexpr int32_t n = c249;
            return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c249>, uint32_t> {
                uint32_t guid236 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t color = guid236;
                
                juniper::array<NeoPixel::color, c249> guid237 = zeros<NeoPixel::color, c249>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<NeoPixel::color, c249> pixels = guid237;
                
                (([&]() -> juniper::unit {
                    uint16_t guid238 = ((uint16_t) 0);
                    uint16_t guid239 = cast<int32_t, uint16_t>(n);
                    for (uint16_t i = guid238; i < guid239; i++) {
                        (([&]() -> NeoPixel::color {
                            getPixelColor(i, device);
                            return ((pixels)[i] = RGB(toUInt8<uint32_t>(((uint32_t) (color >> ((uint32_t) 16)))), toUInt8<uint32_t>(((uint32_t) (color >> ((uint32_t) 8)))), toUInt8<uint32_t>(color)));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c249>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<NeoPixel::color, c249>, uint32_t> guid240;
                    guid240.data = pixels;
                    guid240.length = cast<int32_t, uint32_t>(n);
                    return guid240;
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c252, int c253>
    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, c253>, uint32_t>> initialState(juniper::array<juniper::records::recordt_7<uint16_t>, c253> descriptors, uint16_t nPixels) {
        return (([&]() -> juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, c253>, uint32_t>> {
            constexpr int32_t m = c252;
            constexpr int32_t nLines = c253;
            return (([&]() -> juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, c253>, uint32_t>> {
                return (([&]() -> juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, c253>, uint32_t>>{
                    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, c253>, uint32_t>> guid241;
                    guid241.lines = List::map<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>, juniper::closures::closuret_6<uint16_t>, juniper::records::recordt_7<uint16_t>, c253>(juniper::function<juniper::closures::closuret_6<uint16_t>, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>(juniper::records::recordt_7<uint16_t>)>(juniper::closures::closuret_6<uint16_t>(nPixels), [](juniper::closures::closuret_6<uint16_t>& junclosure, juniper::records::recordt_7<uint16_t> descriptor) -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>> { 
                        uint16_t& nPixels = junclosure.nPixels;
                        return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>> {
                            NeoPixel::RawDevice guid242 = makeDevice((descriptor).pin, cast<uint16_t, uint16_t>(nPixels));
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            NeoPixel::RawDevice device = guid242;
                            
                            juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t> guid243 = (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>{
                                juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t> guid244;
                                guid244.data = zeros<NeoPixel::color, c252>();
                                guid244.length = cast<uint16_t, uint32_t>(nPixels);
                                return guid244;
                            })());
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t> pixels = guid243;
                            
                            return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>>{
                                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c252>, uint32_t>> guid245;
                                guid245.previousPixels = readPixels<c252>(device);
                                guid245.pixels = pixels;
                                guid245.operation = nothing<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>();
                                guid245.pin = (descriptor).pin;
                                guid245.device = device;
                                return guid245;
                            })());
                        })());
                     }), (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_7<uint16_t>, c253>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<juniper::records::recordt_7<uint16_t>, c253>, uint32_t> guid246;
                        guid246.data = descriptors;
                        guid246.length = cast<int32_t, uint32_t>(nLines);
                        return guid246;
                    })()));
                    return guid241;
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c257>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> applyFunction(NeoPixel::Function fn, juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> pixels) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
            constexpr int32_t nPixels = c257;
            return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                    NeoPixel::Function guid247 = fn;
                    return (((bool) (((bool) ((guid247).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                            int16_t step = (guid247).rotate();
                            return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                                (([&]() -> juniper::unit {
                                     Serial.println("Rotate"); 
                                    return {};
                                })());
                                return ListExt::rotated<NeoPixel::color, c257>(cast<int16_t, int32_t>(step), pixels);
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid247).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                                NeoPixel::color color = (guid247).set();
                                return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                                    (([&]() -> juniper::unit {
                                         Serial.println("Set color"); 
                                        return {};
                                    })());
                                    return List::map<NeoPixel::color, juniper::closures::closuret_7<NeoPixel::color>, NeoPixel::color, c257>(juniper::function<juniper::closures::closuret_7<NeoPixel::color>, NeoPixel::color(NeoPixel::color)>(juniper::closures::closuret_7<NeoPixel::color>(color), [](juniper::closures::closuret_7<NeoPixel::color>& junclosure, NeoPixel::color _) -> NeoPixel::color { 
                                        NeoPixel::color& color = junclosure.color;
                                        return color;
                                     }), pixels);
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid247).id() == ((uint8_t) 2))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                                    NeoPixel::color c2 = ((guid247).alternate()).e2;
                                    NeoPixel::color c1 = ((guid247).alternate()).e1;
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> {
                                        (([&]() -> juniper::unit {
                                             Serial.println("Alternate"); 
                                            return {};
                                        })());
                                        juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> guid248 = pixels;
                                        if (!(true)) {
                                            juniper::quit<juniper::unit>();
                                        }
                                        juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t> pixels = guid248;
                                        
                                        (([&]() -> juniper::unit {
                                            int32_t guid249 = ((int32_t) 0);
                                            int32_t guid250 = nPixels;
                                            for (int32_t i = guid249; i < guid250; i++) {
                                                (([&]() -> NeoPixel::color {
                                                    return (((pixels).data)[i] = (((bool) (((int32_t) (i % ((int32_t) 2))) == ((int32_t) 0))) ? 
                                                        (([&]() -> NeoPixel::color {
                                                            return c1;
                                                        })())
                                                    :
                                                        (([&]() -> NeoPixel::color {
                                                            return c2;
                                                        })())));
                                                })());
                                            }
                                            return {};
                                        })());
                                        return pixels;
                                    })());
                                })())
                            :
                                juniper::quit<juniper::records::recordt_0<juniper::array<NeoPixel::color, c257>, uint32_t>>())));
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c261>
    juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c261>, uint32_t> diffPixels(juniper::records::recordt_0<juniper::array<NeoPixel::color, c261>, uint32_t> current, juniper::records::recordt_0<juniper::array<NeoPixel::color, c261>, uint32_t> next) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c261>, uint32_t> {
            constexpr int32_t nPixels = c261;
            return (([&]() -> juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c261>, uint32_t> {
                return List::map<Prelude::maybe<NeoPixel::color>, void, juniper::tuple2<NeoPixel::color, NeoPixel::color>, c261>(juniper::function<void, Prelude::maybe<NeoPixel::color>(juniper::tuple2<NeoPixel::color, NeoPixel::color>)>([](juniper::tuple2<NeoPixel::color, NeoPixel::color> tup) -> Prelude::maybe<NeoPixel::color> { 
                    return (([&]() -> Prelude::maybe<NeoPixel::color> {
                        juniper::tuple2<NeoPixel::color, NeoPixel::color> guid251 = tup;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        NeoPixel::color next = (guid251).e2;
                        NeoPixel::color curr = (guid251).e1;
                        
                        return (eq<NeoPixel::color>(curr, next) ? 
                            nothing<NeoPixel::color>()
                        :
                            just<NeoPixel::color>(next));
                    })());
                 }), List::zip<NeoPixel::color, NeoPixel::color, c261>(current, next));
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit setPixelColor(uint16_t n, NeoPixel::color color, NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid252 = line;
            if (!(((bool) (((bool) ((guid252).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid252).device();
            
            NeoPixel::color guid253 = color;
            if (!(((bool) (((bool) ((guid253).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = ((guid253).RGB()).e3;
            uint8_t g = ((guid253).RGB()).e2;
            uint8_t r = ((guid253).RGB()).e1;
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->setPixelColor(n, r, g, b); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit show(NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid254 = line;
            if (!(((bool) (((bool) ((guid254).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid254).device();
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->show(); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c264>
    juniper::unit updatePixels(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>> state) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c264;
            return (([&]() -> juniper::unit {
                List::iter<juniper::closures::closuret_8<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>>>, juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>>, c264>(juniper::function<juniper::closures::closuret_8<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>>>, juniper::unit(juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>>)>(juniper::closures::closuret_8<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>>>(state), [](juniper::closures::closuret_8<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>>>& junclosure, juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>> tup) -> juniper::unit { 
                    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c264>, uint32_t>>& state = junclosure.state;
                    return (([&]() -> juniper::unit {
                        juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>> guid255 = tup;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<NeoPixel::color> color = (guid255).e2;
                        uint32_t index = (guid255).e1;
                        
                        return (([&]() -> juniper::unit {
                            Prelude::maybe<NeoPixel::color> guid256 = color;
                            return (((bool) (((bool) ((guid256).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> juniper::unit {
                                    NeoPixel::color pixel = (guid256).just();
                                    return setPixelColor(cast<uint32_t, uint16_t>(index), pixel, (state).device);
                                })())
                            :
                                (((bool) (((bool) ((guid256).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> juniper::unit {
                                        return juniper::unit();
                                    })())
                                :
                                    juniper::quit<juniper::unit>()));
                        })());
                    })());
                 }), ListExt::enumerated<Prelude::maybe<NeoPixel::color>, c264>(diffPixels<c264>((state).previousPixels, (state).pixels)));
                return show((state).device);
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c268>
    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> updateLine(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> line, NeoPixel::Function fn) {
        return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> {
            constexpr int32_t nPixels = c268;
            return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> {
                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> guid257 = line;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>> line = guid257;
                
                ((line).previousPixels = (line).pixels);
                ((line).pixels = applyFunction<c268>(fn, (line).previousPixels));
                updatePixels<c268>(line);
                return line;
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c271>
    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> runOperation(juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> line) {
        return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> {
            constexpr int32_t nPixels = c271;
            return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> {
                juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> guid258 = operation;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation = guid258;
                
                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> guid259 = line;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> line = guid259;
                
                Signal::latch<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>>(line, Signal::map<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, juniper::closures::closuret_9<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>>(juniper::function<juniper::closures::closuret_9<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>(Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>)>(juniper::closures::closuret_9<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(line, operation), [](juniper::closures::closuret_9<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>& junclosure, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> _) -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> { 
                    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>>& line = junclosure.line;
                    juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>& operation = junclosure.operation;
                    return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c271>, uint32_t>> {
                        return updateLine<c271>(line, (operation).function);
                    })());
                 }), Signal::latch<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>((line).operation, Signal::map<uint32_t, juniper::closures::closuret_10<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>(juniper::function<juniper::closures::closuret_10<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(uint32_t)>(juniper::closures::closuret_10<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(operation), [](juniper::closures::closuret_10<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>& junclosure, uint32_t _) -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> { 
                    juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>& operation = junclosure.operation;
                    return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                        return just<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(operation);
                    })());
                 }), Time::every((operation).interval, (operation).timer)))));
                return line;
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c273>
    juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> updateOperation(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> line) {
        return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
            constexpr int32_t nPixels = c273;
            return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> guid260 = line;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> line = guid260;
                
                (([&]() -> juniper::unit {
                    Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> guid261 = (line).operation;
                    return (((bool) (((bool) ((guid261).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation = (guid261).just();
                            return (([&]() -> juniper::unit {
                                juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> guid262 = operation;
                                if (!(true)) {
                                    juniper::quit<juniper::unit>();
                                }
                                juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation = guid262;
                                
                                (line = (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                    Prelude::maybe<uint8_t> guid263 = (operation).endAfter;
                                    return (((bool) (((bool) ((guid263).id() == ((uint8_t) 0))) && ((bool) (((bool) ((guid263).just() == ((uint8_t) 0))) && true)))) ? 
                                        (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                            return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                                ((line).operation = nothing<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>());
                                                return line;
                                            })());
                                        })())
                                    :
                                        (((bool) (((bool) ((guid263).id() == ((uint8_t) 0))) && true)) ? 
                                            (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                                uint8_t n = (guid263).just();
                                                return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                                    ((operation).endAfter = just<uint8_t>(((uint8_t) (n - ((uint8_t) 1)))));
                                                    return runOperation<c273>(operation, line);
                                                })());
                                            })())
                                        :
                                            (((bool) (((bool) ((guid263).id() == ((uint8_t) 1))) && true)) ? 
                                                (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                                    return (([&]() -> juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>> {
                                                        return runOperation<c273>(operation, line);
                                                    })());
                                                })())
                                            :
                                                juniper::quit<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c273>, uint32_t>>>())));
                                })()));
                                return juniper::unit();
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid261).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    return juniper::unit();
                                })());
                            })())
                        :
                            juniper::quit<juniper::unit>()));
                })());
                return line;
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit begin(NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid264 = line;
            if (!(((bool) (((bool) ((guid264).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid264).device();
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->begin(); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c276, int c277>
    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> update(Prelude::maybe<NeoPixel::Action> act, juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> model) {
        return (([&]() -> juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> {
            constexpr int32_t nLines = c276;
            constexpr int32_t nPixels = c277;
            return (([&]() -> juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> {
                juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> guid265 = model;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>> model = guid265;
                
                Signal::latch<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>((model).lines, Signal::map<NeoPixel::Update, juniper::closures::closuret_11<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>>, juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>(juniper::function<juniper::closures::closuret_11<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>>, juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>(NeoPixel::Update)>(juniper::closures::closuret_11<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>>(model), [](juniper::closures::closuret_11<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>>& junclosure, NeoPixel::Update update) -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> { 
                    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>& model = junclosure.model;
                    return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> {
                        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> {
                            NeoPixel::Update guid266 = update;
                            return (((bool) (((bool) ((guid266).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> {
                                    NeoPixel::Action action = (guid266).action();
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> {
                                        juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> guid267 = (model).lines;
                                        if (!(true)) {
                                            juniper::quit<juniper::unit>();
                                        }
                                        juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> lines = guid267;
                                        
                                        (([&]() -> juniper::unit {
                                            NeoPixel::Action guid268 = action;
                                            return (((bool) (((bool) ((guid268).id() == ((uint8_t) 0))) && true)) ? 
                                                (([&]() -> juniper::unit {
                                                    return (([&]() -> juniper::unit {
                                                        (([&]() -> juniper::unit {
                                                             Serial.println("start"); 
                                                            return {};
                                                        })());
                                                        return List::iter<void, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>(juniper::function<void, juniper::unit(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>)>([](juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>> line) -> juniper::unit { 
                                                            return (([&]() -> juniper::unit {
                                                                return begin((line).device);
                                                            })());
                                                         }), (model).lines);
                                                    })());
                                                })())
                                            :
                                                (((bool) (((bool) ((guid268).id() == ((uint8_t) 1))) && true)) ? 
                                                    (([&]() -> juniper::unit {
                                                        NeoPixel::Function fn = ((guid268).run()).e2;
                                                        uint8_t line = ((guid268).run()).e1;
                                                        return (([&]() -> juniper::unit {
                                                            (([&]() -> juniper::unit {
                                                                 Serial.println("run"); 
                                                                return {};
                                                            })());
                                                            (((lines).data)[line] = updateLine<c277>((((model).lines).data)[line], fn));
                                                            return juniper::unit();
                                                        })());
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid268).id() == ((uint8_t) 2))) && true)) ? 
                                                        (([&]() -> juniper::unit {
                                                            Prelude::maybe<uint8_t> endAfter = ((guid268).repeat()).e4;
                                                            uint32_t interval = ((guid268).repeat()).e3;
                                                            NeoPixel::Function fn = ((guid268).repeat()).e2;
                                                            uint8_t line = ((guid268).repeat()).e1;
                                                            return (([&]() -> juniper::unit {
                                                                (([&]() -> juniper::unit {
                                                                     Serial.println("repeat"); 
                                                                    return {};
                                                                })());
                                                                (((lines).data)[line] = updateLine<c277>((((model).lines).data)[line], fn));
                                                                ((((lines).data)[line]).operation = just<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>((([&]() -> juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>{
                                                                    juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> guid269;
                                                                    guid269.function = fn;
                                                                    guid269.interval = interval;
                                                                    guid269.timer = Time::state;
                                                                    guid269.endAfter = endAfter;
                                                                    return guid269;
                                                                })())));
                                                                return juniper::unit();
                                                            })());
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid268).id() == ((uint8_t) 3))) && true)) ? 
                                                            (([&]() -> juniper::unit {
                                                                uint8_t line = (guid268).endRepeat();
                                                                return (([&]() -> juniper::unit {
                                                                    (([&]() -> juniper::unit {
                                                                         Serial.println("endRepeat"); 
                                                                        return {};
                                                                    })());
                                                                    ((((lines).data)[line]).operation = nothing<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>());
                                                                    return juniper::unit();
                                                                })());
                                                            })())
                                                        :
                                                            juniper::quit<juniper::unit>()))));
                                        })());
                                        return lines;
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid266).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> {
                                        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t> {
                                            return List::map<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, void, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>(juniper::function<void, juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>(juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>)>(updateOperation<c277>), (model).lines);
                                        })());
                                    })())
                                :
                                    juniper::quit<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c277>, uint32_t>>, c276>, uint32_t>>()));
                        })());
                    })());
                 }), Signal::mergeMany<NeoPixel::Update, 2>((([&]() -> juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Update>, 2>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Update>, 2>, uint32_t> guid270;
                    guid270.data = (juniper::array<Prelude::sig<NeoPixel::Update>, 2> { {Signal::map<NeoPixel::Action, void, NeoPixel::Update>(juniper::function<void, NeoPixel::Update(NeoPixel::Action)>(action), signal<NeoPixel::Action>(act)), Signal::constant<NeoPixel::Update>(operation())} });
                    guid270.length = ((uint32_t) 2);
                    return guid270;
                })()))));
                return model;
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit setBrightness(uint8_t level, NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid271 = line;
            if (!(((bool) (((bool) ((guid271).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid271).device();
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->setBrightness(level); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    uint8_t getBrightness(NeoPixel::RawDevice line) {
        return (([&]() -> uint8_t {
            uint8_t guid272 = ((uint8_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t ret = guid272;
            
            NeoPixel::RawDevice guid273 = line;
            if (!(((bool) (((bool) ((guid273).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid273).device();
            
            (([&]() -> juniper::unit {
                 ret = ((Adafruit_NeoPixel*) p)->getBrightness(); 
                return {};
            })());
            return ret;
        })());
    }
}

namespace NeoPixel {
    juniper::unit clear(NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid274 = line;
            if (!(((bool) (((bool) ((guid274).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid274).device();
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->clear(); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    bool canShow(NeoPixel::RawDevice line) {
        return (([&]() -> bool {
            NeoPixel::RawDevice guid275 = line;
            if (!(((bool) (((bool) ((guid275).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid275).device();
            
            bool guid276 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid276;
            
            (([&]() -> juniper::unit {
                 ret = ((Adafruit_NeoPixel*) p)->canShow(); 
                return {};
            })());
            return ret;
        })());
    }
}

namespace TEA {
    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>> state = initialState<150, 1>((juniper::array<juniper::records::recordt_7<uint16_t>, 1> { {(([]() -> juniper::records::recordt_7<uint16_t>{
        juniper::records::recordt_7<uint16_t> guid277;
        guid277.pin = ((uint16_t) 7);
        return guid277;
    })())} }), ((uint16_t) 150));
}

namespace TEA {
    Prelude::maybe<NeoPixel::Action> previousAction = nothing<NeoPixel::Action>();
}

namespace TEA {
    juniper::unit setup() {
        return (([&]() -> juniper::unit {
            (([&]() -> juniper::unit {
                 Serial.begin(9600); 
                return {};
            })());
            (([&]() -> juniper::unit {
                 Serial.println("Set color"); 
                return {};
            })());
            return juniper::unit();
        })());
    }
}

namespace TEA {
    Prelude::sig<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>>> loop() {
        return (([&]() -> Prelude::sig<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>>> {
            return Signal::foldP<Prelude::maybe<NeoPixel::Action>, void, juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>>>(juniper::function<void, juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>>(Prelude::maybe<NeoPixel::Action>, juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::records::recordt_9<NeoPixel::RawDevice, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>, 1>, uint32_t>>)>(update<1, 150>), state, actions(previousAction));
        })());
    }
}

void setup() {
    TEA::setup();
}
void loop() {
    TEA::loop();
}
