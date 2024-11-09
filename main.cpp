//Compiled on 11/9/2024 10:08:36 PM
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
namespace MaybeExt {}
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

namespace MaybeExt {
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
        struct closuret_8 {
            T1 color;


            closuret_8(T1 init_color) :
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
        struct closuret_6 {
            T1 f;
            T2 operation;


            closuret_6(T1 init_f, T2 init_operation) :
                f(init_f), operation(init_operation) {}
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

        template<typename T1>
        struct closuret_9 {
            T1 line;


            closuret_9(T1 init_line) :
                line(init_line) {}
        };

        template<typename T1>
        struct closuret_10 {
            T1 model;


            closuret_10(T1 init_model) :
                model(init_model) {}
        };

        template<typename T1>
        struct closuret_7 {
            T1 operation;


            closuret_7(T1 init_operation) :
                operation(init_operation) {}
        };

        template<typename T1>
        struct closuret_5 {
            T1 pin;


            closuret_5(T1 init_pin) :
                pin(init_pin) {}
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
    using Line = juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>>;


}

namespace NeoPixel {
    template<int nLines, int nPixels>
    using Model = juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, nPixels>, uint32_t>>>, nLines>, uint32_t>>;


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
    template<typename t50, typename t51, typename t52, typename t53, typename t55>
    juniper::function<juniper::closures::closuret_0<juniper::function<t53, t51(t50)>, juniper::function<t52, t50(t55)>>, t51(t55)> compose(juniper::function<t53, t51(t50)> f, juniper::function<t52, t50(t55)> g);
}

namespace Prelude {
    template<typename t63>
    t63 id(t63 x);
}

namespace Prelude {
    template<typename t70, typename t71, typename t74, typename t75>
    juniper::function<juniper::closures::closuret_1<juniper::function<t71, t70(t74, t75)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>, t70(t75)>(t74)> curry(juniper::function<t71, t70(t74, t75)> f);
}

namespace Prelude {
    template<typename t86, typename t87, typename t88, typename t90, typename t91>
    juniper::function<juniper::closures::closuret_1<juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>>, t86(t90, t91)> uncurry(juniper::function<t87, juniper::function<t88, t86(t91)>(t90)> f);
}

namespace Prelude {
    template<typename t106, typename t110, typename t111, typename t112, typename t113>
    juniper::function<juniper::closures::closuret_1<juniper::function<t110, t106(t111, t112, t113)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>, juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(t112)>(t111)> curry3(juniper::function<t110, t106(t111, t112, t113)> f);
}

namespace Prelude {
    template<typename t127, typename t128, typename t129, typename t130, typename t132, typename t133, typename t134>
    juniper::function<juniper::closures::closuret_1<juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>>, t127(t132, t133, t134)> uncurry3(juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)> f);
}

namespace Prelude {
    template<typename t145>
    bool eq(t145 x, t145 y);
}

namespace Prelude {
    template<typename t150>
    bool neq(t150 x, t150 y);
}

namespace Prelude {
    template<typename t157, typename t158>
    bool gt(t158 x, t157 y);
}

namespace Prelude {
    template<typename t164, typename t165>
    bool geq(t165 x, t164 y);
}

namespace Prelude {
    template<typename t171, typename t172>
    bool lt(t172 x, t171 y);
}

namespace Prelude {
    template<typename t178, typename t179>
    bool leq(t179 x, t178 y);
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
    template<typename t202, typename t203, typename t204>
    t203 apply(juniper::function<t204, t203(t202)> f, t202 x);
}

namespace Prelude {
    template<typename t210, typename t211, typename t212, typename t213>
    t212 apply2(juniper::function<t213, t212(t210, t211)> f, juniper::tuple2<t210, t211> tup);
}

namespace Prelude {
    template<typename t224, typename t225, typename t226, typename t227, typename t228>
    t227 apply3(juniper::function<t228, t227(t224, t225, t226)> f, juniper::tuple3<t224, t225, t226> tup);
}

namespace Prelude {
    template<typename t242, typename t243, typename t244, typename t245, typename t246, typename t247>
    t246 apply4(juniper::function<t247, t246(t242, t243, t244, t245)> f, juniper::tuple4<t242, t243, t244, t245> tup);
}

namespace Prelude {
    template<typename t263, typename t264>
    t263 fst(juniper::tuple2<t263, t264> tup);
}

namespace Prelude {
    template<typename t268, typename t269>
    t269 snd(juniper::tuple2<t268, t269> tup);
}

namespace Prelude {
    template<typename t273>
    t273 add(t273 numA, t273 numB);
}

namespace Prelude {
    template<typename t275>
    t275 sub(t275 numA, t275 numB);
}

namespace Prelude {
    template<typename t277>
    t277 mul(t277 numA, t277 numB);
}

namespace Prelude {
    template<typename t279>
    t279 div(t279 numA, t279 numB);
}

namespace Prelude {
    template<typename t285, typename t286>
    juniper::tuple2<t286, t285> swap(juniper::tuple2<t285, t286> tup);
}

namespace Prelude {
    template<typename t292, typename t293, typename t294>
    t292 until(juniper::function<t294, bool(t292)> p, juniper::function<t293, t292(t292)> f, t292 a0);
}

namespace Prelude {
    template<typename t303>
    juniper::unit ignore(t303 val);
}

namespace Prelude {
    template<typename t306>
    juniper::unit clear(t306& val);
}

namespace Prelude {
    template<typename t308, int c7>
    juniper::array<t308, c7> array(t308 elem);
}

namespace Prelude {
    template<typename t314, int c9>
    juniper::array<t314, c9> zeros();
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
    template<typename t373>
    uint8_t toUInt8(t373 n);
}

namespace Prelude {
    template<typename t375>
    int8_t toInt8(t375 n);
}

namespace Prelude {
    template<typename t377>
    uint16_t toUInt16(t377 n);
}

namespace Prelude {
    template<typename t379>
    int16_t toInt16(t379 n);
}

namespace Prelude {
    template<typename t381>
    uint32_t toUInt32(t381 n);
}

namespace Prelude {
    template<typename t383>
    int32_t toInt32(t383 n);
}

namespace Prelude {
    template<typename t385>
    float toFloat(t385 n);
}

namespace Prelude {
    template<typename t387>
    double toDouble(t387 n);
}

namespace Prelude {
    template<typename t389>
    t389 fromUInt8(uint8_t n);
}

namespace Prelude {
    template<typename t391>
    t391 fromInt8(int8_t n);
}

namespace Prelude {
    template<typename t393>
    t393 fromUInt16(uint16_t n);
}

namespace Prelude {
    template<typename t395>
    t395 fromInt16(int16_t n);
}

namespace Prelude {
    template<typename t397>
    t397 fromUInt32(uint32_t n);
}

namespace Prelude {
    template<typename t399>
    t399 fromInt32(int32_t n);
}

namespace Prelude {
    template<typename t401>
    t401 fromFloat(float n);
}

namespace Prelude {
    template<typename t403>
    t403 fromDouble(double n);
}

namespace Prelude {
    template<typename t405, typename t406>
    t406 cast(t405 x);
}

namespace List {
    template<typename t409, int c10>
    juniper::records::recordt_0<juniper::array<t409, c10>, uint32_t> empty();
}

namespace List {
    template<typename t416, typename t417, typename t423, int c12>
    juniper::records::recordt_0<juniper::array<t416, c12>, uint32_t> map(juniper::function<t417, t416(t423)> f, juniper::records::recordt_0<juniper::array<t423, c12>, uint32_t> lst);
}

namespace List {
    template<typename t431, typename t433, typename t436, int c16>
    t431 fold(juniper::function<t433, t431(t436, t431)> f, t431 initState, juniper::records::recordt_0<juniper::array<t436, c16>, uint32_t> lst);
}

namespace List {
    template<typename t443, typename t445, typename t451, int c18>
    t443 foldBack(juniper::function<t445, t443(t451, t443)> f, t443 initState, juniper::records::recordt_0<juniper::array<t451, c18>, uint32_t> lst);
}

namespace List {
    template<typename t459, typename t461, int c20>
    t461 reduce(juniper::function<t459, t461(t461, t461)> f, juniper::records::recordt_0<juniper::array<t461, c20>, uint32_t> lst);
}

namespace List {
    template<typename t472, typename t473, int c23>
    Prelude::maybe<t472> tryReduce(juniper::function<t473, t472(t472, t472)> f, juniper::records::recordt_0<juniper::array<t472, c23>, uint32_t> lst);
}

namespace List {
    template<typename t516, typename t520, int c25>
    t520 reduceBack(juniper::function<t516, t520(t520, t520)> f, juniper::records::recordt_0<juniper::array<t520, c25>, uint32_t> lst);
}

namespace List {
    template<typename t532, typename t533, int c28>
    Prelude::maybe<t532> tryReduceBack(juniper::function<t533, t532(t532, t532)> f, juniper::records::recordt_0<juniper::array<t532, c28>, uint32_t> lst);
}

namespace List {
    template<typename t588, int c30, int c31, int c32>
    juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> concat(juniper::records::recordt_0<juniper::array<t588, c30>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t588, c31>, uint32_t> lstB);
}

namespace List {
    template<typename t593, int c38, int c39>
    juniper::records::recordt_0<juniper::array<t593, (((c38)*(-1))+((c39)*(-1)))*(-1)>, uint32_t> concatSafe(juniper::records::recordt_0<juniper::array<t593, c38>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t593, c39>, uint32_t> lstB);
}

namespace List {
    template<typename t601, typename t603, int c43>
    t603 get(t601 i, juniper::records::recordt_0<juniper::array<t603, c43>, uint32_t> lst);
}

namespace List {
    template<typename t606, typename t614, int c45>
    Prelude::maybe<t614> tryGet(t606 i, juniper::records::recordt_0<juniper::array<t614, c45>, uint32_t> lst);
}

namespace List {
    template<typename t647, int c47, int c48, int c49>
    juniper::records::recordt_0<juniper::array<t647, c49>, uint32_t> flatten(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t647, c47>, uint32_t>, c48>, uint32_t> listOfLists);
}

namespace List {
    template<typename t651, int c55, int c56>
    juniper::records::recordt_0<juniper::array<t651, (c56)*(c55)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t651, c55>, uint32_t>, c56>, uint32_t> listOfLists);
}

namespace Math {
    template<typename t657>
    t657 min_(t657 x, t657 y);
}

namespace List {
    template<typename t674, int c60, int c61>
    juniper::records::recordt_0<juniper::array<t674, c60>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t674, c61>, uint32_t> lst);
}

namespace List {
    template<typename t680, typename t683, int c65>
    bool all(juniper::function<t680, bool(t683)> pred, juniper::records::recordt_0<juniper::array<t683, c65>, uint32_t> lst);
}

namespace List {
    template<typename t690, typename t693, int c67>
    bool any(juniper::function<t690, bool(t693)> pred, juniper::records::recordt_0<juniper::array<t693, c67>, uint32_t> lst);
}

namespace List {
    template<typename t698, int c69>
    juniper::unit append(t698 elem, juniper::records::recordt_0<juniper::array<t698, c69>, uint32_t>& lst);
}

namespace List {
    template<typename t706, int c71>
    juniper::records::recordt_0<juniper::array<t706, c71>, uint32_t> appendPure(t706 elem, juniper::records::recordt_0<juniper::array<t706, c71>, uint32_t> lst);
}

namespace List {
    template<typename t714, int c73>
    juniper::records::recordt_0<juniper::array<t714, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> appendSafe(t714 elem, juniper::records::recordt_0<juniper::array<t714, c73>, uint32_t> lst);
}

namespace List {
    template<typename t729, int c77>
    juniper::unit prepend(t729 elem, juniper::records::recordt_0<juniper::array<t729, c77>, uint32_t>& lst);
}

namespace List {
    template<typename t746, int c81>
    juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> prependPure(t746 elem, juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> lst);
}

namespace List {
    template<typename t761, int c85>
    juniper::unit set(uint32_t index, t761 elem, juniper::records::recordt_0<juniper::array<t761, c85>, uint32_t>& lst);
}

namespace List {
    template<typename t766, int c87>
    juniper::records::recordt_0<juniper::array<t766, c87>, uint32_t> setPure(uint32_t index, t766 elem, juniper::records::recordt_0<juniper::array<t766, c87>, uint32_t> lst);
}

namespace List {
    template<typename t772, int c89>
    juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> replicate(uint32_t numOfElements, t772 elem);
}

namespace List {
    template<typename t793, int c92>
    juniper::unit remove(t793 elem, juniper::records::recordt_0<juniper::array<t793, c92>, uint32_t>& lst);
}

namespace List {
    template<typename t800, int c97>
    juniper::records::recordt_0<juniper::array<t800, c97>, uint32_t> removePure(t800 elem, juniper::records::recordt_0<juniper::array<t800, c97>, uint32_t> lst);
}

namespace List {
    template<typename t812, int c99>
    juniper::unit pop(juniper::records::recordt_0<juniper::array<t812, c99>, uint32_t>& lst);
}

namespace List {
    template<typename t819, int c101>
    juniper::records::recordt_0<juniper::array<t819, c101>, uint32_t> popPure(juniper::records::recordt_0<juniper::array<t819, c101>, uint32_t> lst);
}

namespace List {
    template<typename t827, typename t830, int c103>
    juniper::unit iter(juniper::function<t827, juniper::unit(t830)> f, juniper::records::recordt_0<juniper::array<t830, c103>, uint32_t> lst);
}

namespace List {
    template<typename t839, int c105>
    t839 last(juniper::records::recordt_0<juniper::array<t839, c105>, uint32_t> lst);
}

namespace List {
    template<typename t851, int c107>
    Prelude::maybe<t851> tryLast(juniper::records::recordt_0<juniper::array<t851, c107>, uint32_t> lst);
}

namespace List {
    template<typename t887, int c109>
    Prelude::maybe<t887> tryMax(juniper::records::recordt_0<juniper::array<t887, c109>, uint32_t> lst);
}

namespace List {
    template<typename t926, int c113>
    Prelude::maybe<t926> tryMin(juniper::records::recordt_0<juniper::array<t926, c113>, uint32_t> lst);
}

namespace List {
    template<typename t956, int c117>
    bool member(t956 elem, juniper::records::recordt_0<juniper::array<t956, c117>, uint32_t> lst);
}

namespace List {
    template<typename t974, typename t976, int c119>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t974, c119>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t976, c119>, uint32_t> lstB);
}

namespace List {
    template<typename t993, typename t994, int c124>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t993, t994>, c124>, uint32_t> lst);
}

namespace List {
    template<typename t1002, int c130>
    t1002 sum(juniper::records::recordt_0<juniper::array<t1002, c130>, uint32_t> lst);
}

namespace List {
    template<typename t1020, int c132>
    t1020 average(juniper::records::recordt_0<juniper::array<t1020, c132>, uint32_t> lst);
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
    template<typename t1045, typename t1047, typename t1049, typename t1080, int c134>
    juniper::unit siftDown(juniper::records::recordt_0<juniper::array<t1080, c134>, uint32_t>& lst, juniper::function<t1049, t1045(t1080)> key, uint32_t root, t1047 end);
}

namespace List {
    template<typename t1091, typename t1100, typename t1102, int c143>
    juniper::unit heapify(juniper::records::recordt_0<juniper::array<t1091, c143>, uint32_t>& lst, juniper::function<t1102, t1100(t1091)> key);
}

namespace List {
    template<typename t1113, typename t1115, typename t1128, int c145>
    juniper::unit sort(juniper::function<t1115, t1113(t1128)> key, juniper::records::recordt_0<juniper::array<t1128, c145>, uint32_t>& lst);
}

namespace List {
    template<typename t1146, typename t1147, typename t1148, int c152>
    juniper::records::recordt_0<juniper::array<t1147, c152>, uint32_t> sorted(juniper::function<t1148, t1146(t1147)> key, juniper::records::recordt_0<juniper::array<t1147, c152>, uint32_t> lst);
}

namespace Signal {
    template<typename t1158, typename t1160, typename t1175>
    Prelude::sig<t1175> map(juniper::function<t1160, t1175(t1158)> f, Prelude::sig<t1158> s);
}

namespace Signal {
    template<typename t1244, typename t1245>
    juniper::unit sink(juniper::function<t1245, juniper::unit(t1244)> f, Prelude::sig<t1244> s);
}

namespace Signal {
    template<typename t1268, typename t1282>
    Prelude::sig<t1282> filter(juniper::function<t1268, bool(t1282)> f, Prelude::sig<t1282> s);
}

namespace Signal {
    template<typename t1362>
    Prelude::sig<t1362> merge(Prelude::sig<t1362> sigA, Prelude::sig<t1362> sigB);
}

namespace Signal {
    template<typename t1398, int c154>
    Prelude::sig<t1398> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t1398>, c154>, uint32_t> sigs);
}

namespace Signal {
    template<typename t1516, typename t1540>
    Prelude::sig<Prelude::either<t1540, t1516>> join(Prelude::sig<t1540> sigA, Prelude::sig<t1516> sigB);
}

namespace Signal {
    template<typename t1829>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t1829> s);
}

namespace Signal {
    template<typename t1863, typename t1865, typename t1881>
    Prelude::sig<t1881> foldP(juniper::function<t1865, t1881(t1863, t1881)> f, t1881& state0, Prelude::sig<t1863> incoming);
}

namespace Signal {
    template<typename t1958>
    Prelude::sig<t1958> dropRepeats(Prelude::maybe<t1958>& maybePrevValue, Prelude::sig<t1958> incoming);
}

namespace Signal {
    template<typename t2057>
    Prelude::sig<t2057> latch(t2057& prevValue, Prelude::sig<t2057> incoming);
}

namespace Signal {
    template<typename t2117, typename t2118, typename t2121, typename t2129>
    Prelude::sig<t2117> map2(juniper::function<t2118, t2117(t2121, t2129)> f, juniper::tuple2<t2121, t2129>& state, Prelude::sig<t2121> incomingA, Prelude::sig<t2129> incomingB);
}

namespace Signal {
    template<typename t2271, int c156>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t2271, c156>, uint32_t>> record(juniper::records::recordt_0<juniper::array<t2271, c156>, uint32_t>& pastValues, Prelude::sig<t2271> incoming);
}

namespace Signal {
    template<typename t2309>
    Prelude::sig<t2309> constant(t2309 val);
}

namespace Signal {
    template<typename t2344>
    Prelude::sig<Prelude::maybe<t2344>> meta(Prelude::sig<t2344> sigA);
}

namespace Signal {
    template<typename t2411>
    Prelude::sig<t2411> unmeta(Prelude::sig<Prelude::maybe<t2411>> sigA);
}

namespace Signal {
    template<typename t2487, typename t2488>
    Prelude::sig<juniper::tuple2<t2487, t2488>> zip(juniper::tuple2<t2487, t2488>& state, Prelude::sig<t2487> sigA, Prelude::sig<t2488> sigB);
}

namespace Signal {
    template<typename t2558, typename t2565>
    juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>> unzip(Prelude::sig<juniper::tuple2<t2558, t2565>> incoming);
}

namespace Signal {
    template<typename t2692, typename t2693>
    Prelude::sig<t2692> toggle(t2692 val1, t2692 val2, t2692& state, Prelude::sig<t2693> incoming);
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
    template<typename t2748>
    t2748 baseToInt(Io::base b);
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
    template<typename t3202, typename t3204, typename t3213>
    Prelude::maybe<t3213> map(juniper::function<t3204, t3213(t3202)> f, Prelude::maybe<t3202> maybeVal);
}

namespace Maybe {
    template<typename t3243>
    t3243 get(Prelude::maybe<t3243> maybeVal);
}

namespace Maybe {
    template<typename t3252>
    bool isJust(Prelude::maybe<t3252> maybeVal);
}

namespace Maybe {
    template<typename t3263>
    bool isNothing(Prelude::maybe<t3263> maybeVal);
}

namespace Maybe {
    template<typename t3277>
    uint8_t count(Prelude::maybe<t3277> maybeVal);
}

namespace Maybe {
    template<typename t3291, typename t3292, typename t3293>
    t3291 fold(juniper::function<t3293, t3291(t3292, t3291)> f, t3291 initState, Prelude::maybe<t3292> maybeVal);
}

namespace Maybe {
    template<typename t3309, typename t3310>
    juniper::unit iter(juniper::function<t3310, juniper::unit(t3309)> f, Prelude::maybe<t3309> maybeVal);
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
    template<typename t3454>
    t3454 abs_(t3454 x);
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
    template<typename t3465>
    t3465 max_(t3465 x, t3465 y);
}

namespace Math {
    template<typename t3467>
    t3467 mapRange(t3467 x, t3467 a1, t3467 a2, t3467 b1, t3467 b2);
}

namespace Math {
    template<typename t3469>
    t3469 clamp(t3469 x, t3469 min, t3469 max);
}

namespace Math {
    template<typename t3474>
    int8_t sign(t3474 n);
}

namespace Button {
    Prelude::sig<Io::pinState> debounceDelay(Prelude::sig<Io::pinState> incoming, uint16_t delay, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState);
}

namespace Button {
    Prelude::sig<Io::pinState> debounce(Prelude::sig<Io::pinState> incoming, juniper::records::recordt_2<Io::pinState, uint32_t, Io::pinState>& buttonState);
}

namespace Vector {
    template<typename t3594, int c159>
    juniper::array<t3594, c159> add(juniper::array<t3594, c159> v1, juniper::array<t3594, c159> v2);
}

namespace Vector {
    template<typename t3598, int c162>
    juniper::array<t3598, c162> zero();
}

namespace Vector {
    template<typename t3606, int c164>
    juniper::array<t3606, c164> subtract(juniper::array<t3606, c164> v1, juniper::array<t3606, c164> v2);
}

namespace Vector {
    template<typename t3610, int c167>
    juniper::array<t3610, c167> scale(t3610 scalar, juniper::array<t3610, c167> v);
}

namespace Vector {
    template<typename t3616, int c169>
    t3616 dot(juniper::array<t3616, c169> v1, juniper::array<t3616, c169> v2);
}

namespace Vector {
    template<typename t3622, int c172>
    t3622 magnitude2(juniper::array<t3622, c172> v);
}

namespace Vector {
    template<typename t3624, int c175>
    double magnitude(juniper::array<t3624, c175> v);
}

namespace Vector {
    template<typename t3640, int c177>
    juniper::array<t3640, c177> multiply(juniper::array<t3640, c177> u, juniper::array<t3640, c177> v);
}

namespace Vector {
    template<typename t3649, int c180>
    juniper::array<t3649, c180> normalize(juniper::array<t3649, c180> v);
}

namespace Vector {
    template<typename t3660, int c184>
    double angle(juniper::array<t3660, c184> v1, juniper::array<t3660, c184> v2);
}

namespace Vector {
    template<typename t3702>
    juniper::array<t3702, 3> cross(juniper::array<t3702, 3> u, juniper::array<t3702, 3> v);
}

namespace Vector {
    template<typename t3704, int c200>
    juniper::array<t3704, c200> project(juniper::array<t3704, c200> a, juniper::array<t3704, c200> b);
}

namespace Vector {
    template<typename t3720, int c204>
    juniper::array<t3720, c204> projectPlane(juniper::array<t3720, c204> a, juniper::array<t3720, c204> m);
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
    template<typename t3798, int c227>
    t3798 choice(juniper::records::recordt_0<juniper::array<t3798, c227>, uint32_t> lst);
}

namespace Random {
    template<typename t3818, int c229>
    Prelude::maybe<t3818> tryChoice(juniper::records::recordt_0<juniper::array<t3818, c229>, uint32_t> lst);
}

namespace Color {
    juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> hsvToRgb(juniper::records::recordt_5<float, float, float> color);
}

namespace Color {
    uint16_t rgbToRgb565(juniper::records::recordt_3<uint8_t, uint8_t, uint8_t> color);
}

namespace MaybeExt {
    template<typename t3938, typename t3940, typename t3945>
    Prelude::maybe<t3945> flatMap(juniper::function<t3940, Prelude::maybe<t3945>(t3938)> f, Prelude::maybe<t3938> maybeVal);
}

namespace MaybeExt {
    template<typename t3977>
    Prelude::maybe<t3977> flatten(Prelude::maybe<Prelude::maybe<t3977>> maybeVal);
}

namespace ListExt {
    template<int c231, int c232>
    juniper::records::recordt_0<juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)>, uint32_t> range(int32_t lowerBound, int32_t upperBound);
}

namespace ListExt {
    template<typename t4064, int c235>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t4064>, c235>, uint32_t> enumerated(juniper::records::recordt_0<juniper::array<t4064, c235>, uint32_t> lst);
}

namespace ListExt {
    template<typename t4108, int c239>
    juniper::unit rotate(int32_t step, juniper::records::recordt_0<juniper::array<t4108, c239>, uint32_t>& lst);
}

namespace ListExt {
    template<typename t4145, int c243>
    juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> rotated(int32_t step, juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> lst);
}

namespace ListExt {
    template<typename t4166, int c247, int c248>
    juniper::records::recordt_0<juniper::array<t4166, c247>, uint32_t> replicateList(uint32_t nElements, juniper::records::recordt_0<juniper::array<t4166, c248>, uint32_t> elements);
}

namespace SignalExt {
    template<typename t4184>
    Prelude::sig<t4184> once(Prelude::maybe<t4184>& state);
}

namespace NeoPixel {
    Prelude::sig<Prelude::maybe<NeoPixel::Action>> actions(Prelude::maybe<NeoPixel::Action>& prevAction);
}

namespace NeoPixel {
    NeoPixel::color getPixelColor(uint16_t n, NeoPixel::RawDevice line);
}

namespace NeoPixel {
    template<int c253>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c253>, uint32_t> readPixels(Prelude::maybe<NeoPixel::RawDevice> device);
}

namespace NeoPixel {
    template<int c256, int c257>
    juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>>> initialState(juniper::array<juniper::records::recordt_7<uint16_t>, c257> descriptors, uint16_t nPixels);
}

namespace NeoPixel {
    NeoPixel::RawDevice makeDevice(uint16_t pin, uint16_t pixels);
}

namespace NeoPixel {
    template<typename t4552>
    Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> runOperation(juniper::function<t4552, juniper::unit(NeoPixel::Function)> f, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> operation);
}

namespace NeoPixel {
    Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> updateOperation(Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> operation);
}

namespace NeoPixel {
    template<int c260>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> applyFunction(NeoPixel::Function fn, juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> pixels);
}

namespace NeoPixel {
    template<int c265>
    juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c265>, uint32_t> diffPixels(juniper::records::recordt_0<juniper::array<NeoPixel::color, c265>, uint32_t> current, juniper::records::recordt_0<juniper::array<NeoPixel::color, c265>, uint32_t> next);
}

namespace NeoPixel {
    juniper::unit setPixelColor(uint16_t n, NeoPixel::color color, NeoPixel::RawDevice line);
}

namespace NeoPixel {
    juniper::unit show(NeoPixel::RawDevice line);
}

namespace NeoPixel {
    template<int c268>
    juniper::unit writePixels(juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>> line);
}

namespace NeoPixel {
    template<int c272>
    juniper::unit updateLine(juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c272>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c272>, uint32_t>>> line, NeoPixel::Function fn);
}

namespace NeoPixel {
    juniper::unit begin(NeoPixel::RawDevice line);
}

namespace NeoPixel {
    template<int c275, int c276>
    juniper::unit update(Prelude::maybe<NeoPixel::Action> act, juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>> model);
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
    juniper::unit loop();
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

namespace TEA {
    extern juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>>, 1>, uint32_t>>> state;
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
    template<typename t50, typename t51, typename t52, typename t53, typename t55>
    juniper::function<juniper::closures::closuret_0<juniper::function<t53, t51(t50)>, juniper::function<t52, t50(t55)>>, t51(t55)> compose(juniper::function<t53, t51(t50)> f, juniper::function<t52, t50(t55)> g) {
        return (([&]() -> juniper::function<juniper::closures::closuret_0<juniper::function<t53, t51(t50)>, juniper::function<t52, t50(t55)>>, t51(t55)> {
            using a = t55;
            using b = t50;
            using c = t51;
            return juniper::function<juniper::closures::closuret_0<juniper::function<t53, t51(t50)>, juniper::function<t52, t50(t55)>>, t51(t55)>(juniper::closures::closuret_0<juniper::function<t53, t51(t50)>, juniper::function<t52, t50(t55)>>(f, g), [](juniper::closures::closuret_0<juniper::function<t53, t51(t50)>, juniper::function<t52, t50(t55)>>& junclosure, t55 x) -> t51 { 
                juniper::function<t53, t51(t50)>& f = junclosure.f;
                juniper::function<t52, t50(t55)>& g = junclosure.g;
                return f(g(x));
             });
        })());
    }
}

namespace Prelude {
    template<typename t63>
    t63 id(t63 x) {
        return (([&]() -> t63 {
            using a = t63;
            return x;
        })());
    }
}

namespace Prelude {
    template<typename t70, typename t71, typename t74, typename t75>
    juniper::function<juniper::closures::closuret_1<juniper::function<t71, t70(t74, t75)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>, t70(t75)>(t74)> curry(juniper::function<t71, t70(t74, t75)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t71, t70(t74, t75)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>, t70(t75)>(t74)> {
            using a = t74;
            using b = t75;
            using c = t70;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t71, t70(t74, t75)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>, t70(t75)>(t74)>(juniper::closures::closuret_1<juniper::function<t71, t70(t74, t75)>>(f), [](juniper::closures::closuret_1<juniper::function<t71, t70(t74, t75)>>& junclosure, t74 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>, t70(t75)> { 
                juniper::function<t71, t70(t74, t75)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>, t70(t75)>(juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t71, t70(t74, t75)>, t74>& junclosure, t75 valueB) -> t70 { 
                    juniper::function<t71, t70(t74, t75)>& f = junclosure.f;
                    t74& valueA = junclosure.valueA;
                    return f(valueA, valueB);
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t86, typename t87, typename t88, typename t90, typename t91>
    juniper::function<juniper::closures::closuret_1<juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>>, t86(t90, t91)> uncurry(juniper::function<t87, juniper::function<t88, t86(t91)>(t90)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>>, t86(t90, t91)> {
            using a = t90;
            using b = t91;
            using c = t86;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>>, t86(t90,t91)>(juniper::closures::closuret_1<juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>>(f), [](juniper::closures::closuret_1<juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>>& junclosure, t90 valueA, t91 valueB) -> t86 { 
                juniper::function<t87, juniper::function<t88, t86(t91)>(t90)>& f = junclosure.f;
                return f(valueA)(valueB);
             });
        })());
    }
}

namespace Prelude {
    template<typename t106, typename t110, typename t111, typename t112, typename t113>
    juniper::function<juniper::closures::closuret_1<juniper::function<t110, t106(t111, t112, t113)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>, juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(t112)>(t111)> curry3(juniper::function<t110, t106(t111, t112, t113)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t110, t106(t111, t112, t113)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>, juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(t112)>(t111)> {
            using a = t111;
            using b = t112;
            using c = t113;
            using d = t106;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t110, t106(t111, t112, t113)>>, juniper::function<juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>, juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(t112)>(t111)>(juniper::closures::closuret_1<juniper::function<t110, t106(t111, t112, t113)>>(f), [](juniper::closures::closuret_1<juniper::function<t110, t106(t111, t112, t113)>>& junclosure, t111 valueA) -> juniper::function<juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>, juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(t112)> { 
                juniper::function<t110, t106(t111, t112, t113)>& f = junclosure.f;
                return juniper::function<juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>, juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(t112)>(juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>(f, valueA), [](juniper::closures::closuret_2<juniper::function<t110, t106(t111, t112, t113)>, t111>& junclosure, t112 valueB) -> juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)> { 
                    juniper::function<t110, t106(t111, t112, t113)>& f = junclosure.f;
                    t111& valueA = junclosure.valueA;
                    return juniper::function<juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>, t106(t113)>(juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>(f, valueA, valueB), [](juniper::closures::closuret_3<juniper::function<t110, t106(t111, t112, t113)>, t111, t112>& junclosure, t113 valueC) -> t106 { 
                        juniper::function<t110, t106(t111, t112, t113)>& f = junclosure.f;
                        t111& valueA = junclosure.valueA;
                        t112& valueB = junclosure.valueB;
                        return f(valueA, valueB, valueC);
                     });
                 });
             });
        })());
    }
}

namespace Prelude {
    template<typename t127, typename t128, typename t129, typename t130, typename t132, typename t133, typename t134>
    juniper::function<juniper::closures::closuret_1<juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>>, t127(t132, t133, t134)> uncurry3(juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)> f) {
        return (([&]() -> juniper::function<juniper::closures::closuret_1<juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>>, t127(t132, t133, t134)> {
            using a = t132;
            using b = t133;
            using c = t134;
            using d = t127;
            return juniper::function<juniper::closures::closuret_1<juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>>, t127(t132,t133,t134)>(juniper::closures::closuret_1<juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>>(f), [](juniper::closures::closuret_1<juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>>& junclosure, t132 valueA, t133 valueB, t134 valueC) -> t127 { 
                juniper::function<t128, juniper::function<t129, juniper::function<t130, t127(t134)>(t133)>(t132)>& f = junclosure.f;
                return f(valueA)(valueB)(valueC);
             });
        })());
    }
}

namespace Prelude {
    template<typename t145>
    bool eq(t145 x, t145 y) {
        return (([&]() -> bool {
            using a = t145;
            return ((bool) (x == y));
        })());
    }
}

namespace Prelude {
    template<typename t150>
    bool neq(t150 x, t150 y) {
        return ((bool) (x != y));
    }
}

namespace Prelude {
    template<typename t157, typename t158>
    bool gt(t158 x, t157 y) {
        return ((bool) (x > y));
    }
}

namespace Prelude {
    template<typename t164, typename t165>
    bool geq(t165 x, t164 y) {
        return ((bool) (x >= y));
    }
}

namespace Prelude {
    template<typename t171, typename t172>
    bool lt(t172 x, t171 y) {
        return ((bool) (x < y));
    }
}

namespace Prelude {
    template<typename t178, typename t179>
    bool leq(t179 x, t178 y) {
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
    template<typename t202, typename t203, typename t204>
    t203 apply(juniper::function<t204, t203(t202)> f, t202 x) {
        return (([&]() -> t203 {
            using a = t202;
            using b = t203;
            return f(x);
        })());
    }
}

namespace Prelude {
    template<typename t210, typename t211, typename t212, typename t213>
    t212 apply2(juniper::function<t213, t212(t210, t211)> f, juniper::tuple2<t210, t211> tup) {
        return (([&]() -> t212 {
            using a = t210;
            using b = t211;
            using c = t212;
            return (([&]() -> t212 {
                juniper::tuple2<t210, t211> guid0 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t211 b = (guid0).e2;
                t210 a = (guid0).e1;
                
                return f(a, b);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t224, typename t225, typename t226, typename t227, typename t228>
    t227 apply3(juniper::function<t228, t227(t224, t225, t226)> f, juniper::tuple3<t224, t225, t226> tup) {
        return (([&]() -> t227 {
            using a = t224;
            using b = t225;
            using c = t226;
            using d = t227;
            return (([&]() -> t227 {
                juniper::tuple3<t224, t225, t226> guid1 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t226 c = (guid1).e3;
                t225 b = (guid1).e2;
                t224 a = (guid1).e1;
                
                return f(a, b, c);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t242, typename t243, typename t244, typename t245, typename t246, typename t247>
    t246 apply4(juniper::function<t247, t246(t242, t243, t244, t245)> f, juniper::tuple4<t242, t243, t244, t245> tup) {
        return (([&]() -> t246 {
            using a = t242;
            using b = t243;
            using c = t244;
            using d = t245;
            using e = t246;
            return (([&]() -> t246 {
                juniper::tuple4<t242, t243, t244, t245> guid2 = tup;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t245 d = (guid2).e4;
                t244 c = (guid2).e3;
                t243 b = (guid2).e2;
                t242 a = (guid2).e1;
                
                return f(a, b, c, d);
            })());
        })());
    }
}

namespace Prelude {
    template<typename t263, typename t264>
    t263 fst(juniper::tuple2<t263, t264> tup) {
        return (([&]() -> t263 {
            using a = t263;
            using b = t264;
            return (([&]() -> t263 {
                juniper::tuple2<t263, t264> guid3 = tup;
                return (true ? 
                    (([&]() -> t263 {
                        t263 x = (guid3).e1;
                        return x;
                    })())
                :
                    juniper::quit<t263>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t268, typename t269>
    t269 snd(juniper::tuple2<t268, t269> tup) {
        return (([&]() -> t269 {
            using a = t268;
            using b = t269;
            return (([&]() -> t269 {
                juniper::tuple2<t268, t269> guid4 = tup;
                return (true ? 
                    (([&]() -> t269 {
                        t269 x = (guid4).e2;
                        return x;
                    })())
                :
                    juniper::quit<t269>());
            })());
        })());
    }
}

namespace Prelude {
    template<typename t273>
    t273 add(t273 numA, t273 numB) {
        return (([&]() -> t273 {
            using a = t273;
            return ((t273) (numA + numB));
        })());
    }
}

namespace Prelude {
    template<typename t275>
    t275 sub(t275 numA, t275 numB) {
        return (([&]() -> t275 {
            using a = t275;
            return ((t275) (numA - numB));
        })());
    }
}

namespace Prelude {
    template<typename t277>
    t277 mul(t277 numA, t277 numB) {
        return (([&]() -> t277 {
            using a = t277;
            return ((t277) (numA * numB));
        })());
    }
}

namespace Prelude {
    template<typename t279>
    t279 div(t279 numA, t279 numB) {
        return (([&]() -> t279 {
            using a = t279;
            return ((t279) (numA / numB));
        })());
    }
}

namespace Prelude {
    template<typename t285, typename t286>
    juniper::tuple2<t286, t285> swap(juniper::tuple2<t285, t286> tup) {
        return (([&]() -> juniper::tuple2<t286, t285> {
            juniper::tuple2<t285, t286> guid5 = tup;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            t286 beta = (guid5).e2;
            t285 alpha = (guid5).e1;
            
            return (juniper::tuple2<t286,t285>{beta, alpha});
        })());
    }
}

namespace Prelude {
    template<typename t292, typename t293, typename t294>
    t292 until(juniper::function<t294, bool(t292)> p, juniper::function<t293, t292(t292)> f, t292 a0) {
        return (([&]() -> t292 {
            using a = t292;
            return (([&]() -> t292 {
                t292 guid6 = a0;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t292 a = guid6;
                
                (([&]() -> juniper::unit {
                    while (!(p(a))) {
                        (([&]() -> t292 {
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
    template<typename t303>
    juniper::unit ignore(t303 val) {
        return (([&]() -> juniper::unit {
            using a = t303;
            return juniper::unit();
        })());
    }
}

namespace Prelude {
    template<typename t306>
    juniper::unit clear(t306& val) {
        return (([&]() -> juniper::unit {
            using a = t306;
            return (([&]() -> juniper::unit {
                
    val.~a();
    memset(&val, 0, sizeof(val));
    
                return {};
            })());
        })());
    }
}

namespace Prelude {
    template<typename t308, int c7>
    juniper::array<t308, c7> array(t308 elem) {
        return (([&]() -> juniper::array<t308, c7> {
            using a = t308;
            constexpr int32_t n = c7;
            return (([&]() -> juniper::array<t308, c7> {
                juniper::array<t308, c7> ret;
                
                (([&]() -> juniper::unit {
                    int32_t guid7 = ((int32_t) 0);
                    int32_t guid8 = n;
                    for (int32_t i = guid7; i < guid8; i++) {
                        (([&]() -> t308 {
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
    template<typename t314, int c9>
    juniper::array<t314, c9> zeros() {
        return (([&]() -> juniper::array<t314, c9> {
            using a = t314;
            constexpr int32_t n = c9;
            return (([&]() -> juniper::array<t314, c9> {
                juniper::array<t314, c9> ret;
                
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
    template<typename t373>
    uint8_t toUInt8(t373 n) {
        return (([&]() -> uint8_t {
            using t = t373;
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
    template<typename t375>
    int8_t toInt8(t375 n) {
        return (([&]() -> int8_t {
            using t = t375;
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
    template<typename t377>
    uint16_t toUInt16(t377 n) {
        return (([&]() -> uint16_t {
            using t = t377;
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
    template<typename t379>
    int16_t toInt16(t379 n) {
        return (([&]() -> int16_t {
            using t = t379;
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
    template<typename t381>
    uint32_t toUInt32(t381 n) {
        return (([&]() -> uint32_t {
            using t = t381;
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
    template<typename t383>
    int32_t toInt32(t383 n) {
        return (([&]() -> int32_t {
            using t = t383;
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
    template<typename t385>
    float toFloat(t385 n) {
        return (([&]() -> float {
            using t = t385;
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
    template<typename t387>
    double toDouble(t387 n) {
        return (([&]() -> double {
            using t = t387;
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
    template<typename t389>
    t389 fromUInt8(uint8_t n) {
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
    t391 fromInt8(int8_t n) {
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
    t393 fromUInt16(uint16_t n) {
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
    t395 fromInt16(int16_t n) {
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
    t397 fromUInt32(uint32_t n) {
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
    t399 fromInt32(int32_t n) {
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
    t401 fromFloat(float n) {
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
    template<typename t403>
    t403 fromDouble(double n) {
        return (([&]() -> t403 {
            using t = t403;
            return (([&]() -> t403 {
                t403 ret;
                
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
    template<typename t405, typename t406>
    t406 cast(t405 x) {
        return (([&]() -> t406 {
            using a = t405;
            using b = t406;
            return (([&]() -> t406 {
                t406 ret;
                
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
    template<typename t409, int c10>
    juniper::records::recordt_0<juniper::array<t409, c10>, uint32_t> empty() {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t409, c10>, uint32_t> {
            using a = t409;
            constexpr int32_t n = c10;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t409, c10>, uint32_t>{
                juniper::records::recordt_0<juniper::array<t409, c10>, uint32_t> guid9;
                guid9.data = zeros<t409, c10>();
                guid9.length = ((uint32_t) 0);
                return guid9;
            })());
        })());
    }
}

namespace List {
    template<typename t416, typename t417, typename t423, int c12>
    juniper::records::recordt_0<juniper::array<t416, c12>, uint32_t> map(juniper::function<t417, t416(t423)> f, juniper::records::recordt_0<juniper::array<t423, c12>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t416, c12>, uint32_t> {
            using a = t423;
            using b = t416;
            constexpr int32_t n = c12;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t416, c12>, uint32_t> {
                juniper::array<t416, c12> guid10 = zeros<t416, c12>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t416, c12> ret = guid10;
                
                (([&]() -> juniper::unit {
                    uint32_t guid11 = ((uint32_t) 0);
                    uint32_t guid12 = (lst).length;
                    for (uint32_t i = guid11; i < guid12; i++) {
                        (([&]() -> t416 {
                            return ((ret)[i] = f(((lst).data)[i]));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t416, c12>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t416, c12>, uint32_t> guid13;
                    guid13.data = ret;
                    guid13.length = (lst).length;
                    return guid13;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t431, typename t433, typename t436, int c16>
    t431 fold(juniper::function<t433, t431(t436, t431)> f, t431 initState, juniper::records::recordt_0<juniper::array<t436, c16>, uint32_t> lst) {
        return (([&]() -> t431 {
            using state = t431;
            using t = t436;
            constexpr int32_t n = c16;
            return (([&]() -> t431 {
                t431 guid14 = initState;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t431 s = guid14;
                
                (([&]() -> juniper::unit {
                    uint32_t guid15 = ((uint32_t) 0);
                    uint32_t guid16 = (lst).length;
                    for (uint32_t i = guid15; i < guid16; i++) {
                        (([&]() -> t431 {
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
    template<typename t443, typename t445, typename t451, int c18>
    t443 foldBack(juniper::function<t445, t443(t451, t443)> f, t443 initState, juniper::records::recordt_0<juniper::array<t451, c18>, uint32_t> lst) {
        return (([&]() -> t443 {
            using state = t443;
            using t = t451;
            constexpr int32_t n = c18;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> t443 {
                    return initState;
                })())
            :
                (([&]() -> t443 {
                    t443 guid17 = initState;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t443 s = guid17;
                    
                    uint32_t guid18 = (lst).length;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    uint32_t i = guid18;
                    
                    (([&]() -> juniper::unit {
                        do {
                            (([&]() -> t443 {
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
    template<typename t459, typename t461, int c20>
    t461 reduce(juniper::function<t459, t461(t461, t461)> f, juniper::records::recordt_0<juniper::array<t461, c20>, uint32_t> lst) {
        return (([&]() -> t461 {
            using t = t461;
            constexpr int32_t n = c20;
            return (([&]() -> t461 {
                t461 guid19 = ((lst).data)[((uint32_t) 0)];
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t461 s = guid19;
                
                (([&]() -> juniper::unit {
                    uint32_t guid20 = ((uint32_t) 1);
                    uint32_t guid21 = (lst).length;
                    for (uint32_t i = guid20; i < guid21; i++) {
                        (([&]() -> t461 {
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
    template<typename t472, typename t473, int c23>
    Prelude::maybe<t472> tryReduce(juniper::function<t473, t472(t472, t472)> f, juniper::records::recordt_0<juniper::array<t472, c23>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t472> {
            using t = t472;
            constexpr int32_t n = c23;
            return (([&]() -> Prelude::maybe<t472> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t472> {
                        return nothing<t472>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t472> {
                        return just<t472>(reduce<t473, t472, c23>(f, lst));
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t516, typename t520, int c25>
    t520 reduceBack(juniper::function<t516, t520(t520, t520)> f, juniper::records::recordt_0<juniper::array<t520, c25>, uint32_t> lst) {
        return (([&]() -> t520 {
            using t = t520;
            constexpr int32_t n = c25;
            return (([&]() -> t520 {
                t520 guid22 = ((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))];
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t520 s = guid22;
                
                uint32_t guid23 = ((uint32_t) ((lst).length - ((uint32_t) 1)));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t i = guid23;
                
                (([&]() -> juniper::unit {
                    do {
                        (([&]() -> t520 {
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
    template<typename t532, typename t533, int c28>
    Prelude::maybe<t532> tryReduceBack(juniper::function<t533, t532(t532, t532)> f, juniper::records::recordt_0<juniper::array<t532, c28>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t532> {
            using t = t532;
            constexpr int32_t n = c28;
            return (([&]() -> Prelude::maybe<t532> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t532> {
                        return nothing<t532>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t532> {
                        return just<t532>(reduceBack<t533, t532, c28>(f, lst));
                    })()));
            })());
        })());
    }
}

namespace List {
    template<typename t588, int c30, int c31, int c32>
    juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> concat(juniper::records::recordt_0<juniper::array<t588, c30>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t588, c31>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> {
            using t = t588;
            constexpr int32_t aCap = c30;
            constexpr int32_t bCap = c31;
            constexpr int32_t retCap = c32;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> {
                uint32_t guid24 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t j = guid24;
                
                juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> guid25 = (([&]() -> juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> guid26;
                    guid26.data = zeros<t588, c32>();
                    guid26.length = ((uint32_t) ((lstA).length + (lstB).length));
                    return guid26;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t588, c32>, uint32_t> out = guid25;
                
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
    template<typename t593, int c38, int c39>
    juniper::records::recordt_0<juniper::array<t593, (((c38)*(-1))+((c39)*(-1)))*(-1)>, uint32_t> concatSafe(juniper::records::recordt_0<juniper::array<t593, c38>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t593, c39>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t593, (((c38)*(-1))+((c39)*(-1)))*(-1)>, uint32_t> {
            using t = t593;
            constexpr int32_t aCap = c38;
            constexpr int32_t bCap = c39;
            return concat<t593, c38, c39, (((c38)*(-1))+((c39)*(-1)))*(-1)>(lstA, lstB);
        })());
    }
}

namespace List {
    template<typename t601, typename t603, int c43>
    t603 get(t601 i, juniper::records::recordt_0<juniper::array<t603, c43>, uint32_t> lst) {
        return (([&]() -> t603 {
            using t = t603;
            using u = t601;
            constexpr int32_t n = c43;
            return ((lst).data)[i];
        })());
    }
}

namespace List {
    template<typename t606, typename t614, int c45>
    Prelude::maybe<t614> tryGet(t606 i, juniper::records::recordt_0<juniper::array<t614, c45>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t614> {
            using t = t614;
            using u = t606;
            constexpr int32_t n = c45;
            return (((bool) (i < (lst).length)) ? 
                just<t614>(((lst).data)[i])
            :
                nothing<t614>());
        })());
    }
}

namespace List {
    template<typename t647, int c47, int c48, int c49>
    juniper::records::recordt_0<juniper::array<t647, c49>, uint32_t> flatten(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t647, c47>, uint32_t>, c48>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t647, c49>, uint32_t> {
            using t = t647;
            constexpr int32_t m = c47;
            constexpr int32_t n = c48;
            constexpr int32_t retCap = c49;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t647, c49>, uint32_t> {
                juniper::array<t647, c49> guid31 = zeros<t647, c49>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t647, c49> ret = guid31;
                
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
                return (([&]() -> juniper::records::recordt_0<juniper::array<t647, c49>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t647, c49>, uint32_t> guid37;
                    guid37.data = ret;
                    guid37.length = index;
                    return guid37;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t651, int c55, int c56>
    juniper::records::recordt_0<juniper::array<t651, (c56)*(c55)>, uint32_t> flattenSafe(juniper::records::recordt_0<juniper::array<juniper::records::recordt_0<juniper::array<t651, c55>, uint32_t>, c56>, uint32_t> listOfLists) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t651, (c56)*(c55)>, uint32_t> {
            using t = t651;
            constexpr int32_t m = c55;
            constexpr int32_t n = c56;
            return flatten<t651, c55, c56, (c56)*(c55)>(listOfLists);
        })());
    }
}

namespace Math {
    template<typename t657>
    t657 min_(t657 x, t657 y) {
        return (([&]() -> t657 {
            using a = t657;
            return (((bool) (x > y)) ? 
                (([&]() -> t657 {
                    return y;
                })())
            :
                (([&]() -> t657 {
                    return x;
                })()));
        })());
    }
}

namespace List {
    template<typename t674, int c60, int c61>
    juniper::records::recordt_0<juniper::array<t674, c60>, uint32_t> resize(juniper::records::recordt_0<juniper::array<t674, c61>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t674, c60>, uint32_t> {
            using t = t674;
            constexpr int32_t m = c60;
            constexpr int32_t n = c61;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t674, c60>, uint32_t> {
                juniper::array<t674, c60> guid38 = zeros<t674, c60>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t674, c60> ret = guid38;
                
                (([&]() -> juniper::unit {
                    uint32_t guid39 = ((uint32_t) 0);
                    uint32_t guid40 = Math::min_<uint32_t>((lst).length, toUInt32<int32_t>(m));
                    for (uint32_t i = guid39; i < guid40; i++) {
                        (([&]() -> t674 {
                            return ((ret)[i] = ((lst).data)[i]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t674, c60>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t674, c60>, uint32_t> guid41;
                    guid41.data = ret;
                    guid41.length = (lst).length;
                    return guid41;
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t680, typename t683, int c65>
    bool all(juniper::function<t680, bool(t683)> pred, juniper::records::recordt_0<juniper::array<t683, c65>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t683;
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
    template<typename t690, typename t693, int c67>
    bool any(juniper::function<t690, bool(t693)> pred, juniper::records::recordt_0<juniper::array<t693, c67>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t693;
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
    template<typename t698, int c69>
    juniper::unit append(t698 elem, juniper::records::recordt_0<juniper::array<t698, c69>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t698;
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
    template<typename t706, int c71>
    juniper::records::recordt_0<juniper::array<t706, c71>, uint32_t> appendPure(t706 elem, juniper::records::recordt_0<juniper::array<t706, c71>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t706, c71>, uint32_t> {
            using t = t706;
            constexpr int32_t n = c71;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t706, c71>, uint32_t> {
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
    template<typename t714, int c73>
    juniper::records::recordt_0<juniper::array<t714, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> appendSafe(t714 elem, juniper::records::recordt_0<juniper::array<t714, c73>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t714, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> {
            using t = t714;
            constexpr int32_t n = c73;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t714, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t714, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> guid48 = resize<t714, ((-1)+((c73)*(-1)))*(-1), c73>(lst);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t714, ((-1)+((c73)*(-1)))*(-1)>, uint32_t> ret = guid48;
                
                (((ret).data)[(lst).length] = elem);
                ((ret).length += ((uint32_t) 1));
                return ret;
            })());
        })());
    }
}

namespace List {
    template<typename t729, int c77>
    juniper::unit prepend(t729 elem, juniper::records::recordt_0<juniper::array<t729, c77>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t729;
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
                                            (([&]() -> t729 {
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
    template<typename t746, int c81>
    juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> prependPure(t746 elem, juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> {
            using t = t746;
            constexpr int32_t n = c81;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> {
                return (((bool) (n <= ((int32_t) 0))) ? 
                    (([&]() -> juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> {
                        return lst;
                    })())
                :
                    (([&]() -> juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> {
                        juniper::records::recordt_0<juniper::array<t746, c81>, uint32_t> ret;
                        
                        (((ret).data)[((uint32_t) 0)] = elem);
                        (([&]() -> juniper::unit {
                            uint32_t guid50 = ((uint32_t) 0);
                            uint32_t guid51 = (lst).length;
                            for (uint32_t i = guid50; i < guid51; i++) {
                                (([&]() -> juniper::unit {
                                    return (([&]() -> juniper::unit {
                                        if (((bool) (((uint32_t) (i + ((uint32_t) 1))) < n))) {
                                            (([&]() -> t746 {
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
    template<typename t761, int c85>
    juniper::unit set(uint32_t index, t761 elem, juniper::records::recordt_0<juniper::array<t761, c85>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t761;
            constexpr int32_t n = c85;
            return (([&]() -> juniper::unit {
                if (((bool) (index < (lst).length))) {
                    (([&]() -> t761 {
                        return (((lst).data)[index] = elem);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t766, int c87>
    juniper::records::recordt_0<juniper::array<t766, c87>, uint32_t> setPure(uint32_t index, t766 elem, juniper::records::recordt_0<juniper::array<t766, c87>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t766, c87>, uint32_t> {
            using t = t766;
            constexpr int32_t n = c87;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t766, c87>, uint32_t> {
                (([&]() -> juniper::unit {
                    if (((bool) (index < (lst).length))) {
                        (([&]() -> t766 {
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
    template<typename t772, int c89>
    juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> replicate(uint32_t numOfElements, t772 elem) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> {
            using t = t772;
            constexpr int32_t n = c89;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> {
                juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> guid52 = (([&]() -> juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> guid53;
                    guid53.data = zeros<t772, c89>();
                    guid53.length = numOfElements;
                    return guid53;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t772, c89>, uint32_t> ret = guid52;
                
                (([&]() -> juniper::unit {
                    uint32_t guid54 = ((uint32_t) 0);
                    uint32_t guid55 = numOfElements;
                    for (uint32_t i = guid54; i < guid55; i++) {
                        (([&]() -> t772 {
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
    template<typename t793, int c92>
    juniper::unit remove(t793 elem, juniper::records::recordt_0<juniper::array<t793, c92>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t793;
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
                                    (([&]() -> t793 {
                                        return (((lst).data)[i] = ((lst).data)[((uint32_t) (i + ((uint32_t) 1)))]);
                                    })());
                                }
                                return {};
                            })());
                            return clear<t793>(((lst).data)[(lst).length]);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t800, int c97>
    juniper::records::recordt_0<juniper::array<t800, c97>, uint32_t> removePure(t800 elem, juniper::records::recordt_0<juniper::array<t800, c97>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t800, c97>, uint32_t> {
            using t = t800;
            constexpr int32_t n = c97;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t800, c97>, uint32_t> {
                remove<t800, c97>(elem, lst);
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t812, int c99>
    juniper::unit pop(juniper::records::recordt_0<juniper::array<t812, c99>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using t = t812;
            constexpr int32_t n = c99;
            return (([&]() -> juniper::unit {
                if (((bool) ((lst).length > ((uint32_t) 0)))) {
                    (([&]() -> juniper::unit {
                        ((lst).length -= ((uint32_t) 1));
                        return clear<t812>(((lst).data)[(lst).length]);
                    })());
                }
                return {};
            })());
        })());
    }
}

namespace List {
    template<typename t819, int c101>
    juniper::records::recordt_0<juniper::array<t819, c101>, uint32_t> popPure(juniper::records::recordt_0<juniper::array<t819, c101>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t819, c101>, uint32_t> {
            using t = t819;
            constexpr int32_t n = c101;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t819, c101>, uint32_t> {
                pop<t819, c101>(lst);
                return lst;
            })());
        })());
    }
}

namespace List {
    template<typename t827, typename t830, int c103>
    juniper::unit iter(juniper::function<t827, juniper::unit(t830)> f, juniper::records::recordt_0<juniper::array<t830, c103>, uint32_t> lst) {
        return (([&]() -> juniper::unit {
            using t = t830;
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
    template<typename t839, int c105>
    t839 last(juniper::records::recordt_0<juniper::array<t839, c105>, uint32_t> lst) {
        return (([&]() -> t839 {
            using t = t839;
            constexpr int32_t n = c105;
            return ((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))];
        })());
    }
}

namespace List {
    template<typename t851, int c107>
    Prelude::maybe<t851> tryLast(juniper::records::recordt_0<juniper::array<t851, c107>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t851> {
            using t = t851;
            constexpr int32_t n = c107;
            return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                (([&]() -> Prelude::maybe<t851> {
                    return nothing<t851>();
                })())
            :
                (([&]() -> Prelude::maybe<t851> {
                    return just<t851>(((lst).data)[((uint32_t) ((lst).length - ((uint32_t) 1)))]);
                })()));
        })());
    }
}

namespace List {
    template<typename t887, int c109>
    Prelude::maybe<t887> tryMax(juniper::records::recordt_0<juniper::array<t887, c109>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t887> {
            using t = t887;
            constexpr int32_t n = c109;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t887> {
                    return nothing<t887>();
                })())
            :
                (([&]() -> Prelude::maybe<t887> {
                    t887 guid64 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t887 maxVal = guid64;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid65 = ((uint32_t) 1);
                        uint32_t guid66 = (lst).length;
                        for (uint32_t i = guid65; i < guid66; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] > maxVal))) {
                                        (([&]() -> t887 {
                                            return (maxVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t887>(maxVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t926, int c113>
    Prelude::maybe<t926> tryMin(juniper::records::recordt_0<juniper::array<t926, c113>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t926> {
            using t = t926;
            constexpr int32_t n = c113;
            return (((bool) (((bool) ((lst).length == ((uint32_t) 0))) || ((bool) (n == ((int32_t) 0))))) ? 
                (([&]() -> Prelude::maybe<t926> {
                    return nothing<t926>();
                })())
            :
                (([&]() -> Prelude::maybe<t926> {
                    t926 guid67 = ((lst).data)[((uint32_t) 0)];
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    t926 minVal = guid67;
                    
                    (([&]() -> juniper::unit {
                        uint32_t guid68 = ((uint32_t) 1);
                        uint32_t guid69 = (lst).length;
                        for (uint32_t i = guid68; i < guid69; i++) {
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    if (((bool) (((lst).data)[i] < minVal))) {
                                        (([&]() -> t926 {
                                            return (minVal = ((lst).data)[i]);
                                        })());
                                    }
                                    return {};
                                })());
                            })());
                        }
                        return {};
                    })());
                    return just<t926>(minVal);
                })()));
        })());
    }
}

namespace List {
    template<typename t956, int c117>
    bool member(t956 elem, juniper::records::recordt_0<juniper::array<t956, c117>, uint32_t> lst) {
        return (([&]() -> bool {
            using t = t956;
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
    template<typename t974, typename t976, int c119>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> zip(juniper::records::recordt_0<juniper::array<t974, c119>, uint32_t> lstA, juniper::records::recordt_0<juniper::array<t976, c119>, uint32_t> lstB) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> {
            using a = t974;
            using b = t976;
            constexpr int32_t n = c119;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> {
                uint32_t guid73 = Math::min_<uint32_t>((lstA).length, (lstB).length);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t outLen = guid73;
                
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> guid74 = (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> guid75;
                    guid75.data = zeros<juniper::tuple2<t974, t976>, c119>();
                    guid75.length = outLen;
                    return guid75;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<juniper::tuple2<t974, t976>, c119>, uint32_t> ret = guid74;
                
                (([&]() -> juniper::unit {
                    uint32_t guid76 = ((uint32_t) 0);
                    uint32_t guid77 = outLen;
                    for (uint32_t i = guid76; i < guid77; i++) {
                        (([&]() -> juniper::tuple2<t974, t976> {
                            return (((ret).data)[i] = (juniper::tuple2<t974,t976>{((lstA).data)[i], ((lstB).data)[i]}));
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
    template<typename t993, typename t994, int c124>
    juniper::tuple2<juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t>> unzip(juniper::records::recordt_0<juniper::array<juniper::tuple2<t993, t994>, c124>, uint32_t> lst) {
        return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t>> {
            using a = t993;
            using b = t994;
            constexpr int32_t n = c124;
            return (([&]() -> juniper::tuple2<juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t>, juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t>> {
                juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t> guid78 = (([&]() -> juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t> guid79;
                    guid79.data = zeros<t993, c124>();
                    guid79.length = (lst).length;
                    return guid79;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t> retA = guid78;
                
                juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t> guid80 = (([&]() -> juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t> guid81;
                    guid81.data = zeros<t994, c124>();
                    guid81.length = (lst).length;
                    return guid81;
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t> retB = guid80;
                
                (([&]() -> juniper::unit {
                    uint32_t guid82 = ((uint32_t) 0);
                    uint32_t guid83 = (lst).length;
                    for (uint32_t i = guid82; i < guid83; i++) {
                        (([&]() -> t994 {
                            juniper::tuple2<t993, t994> guid84 = ((lst).data)[i];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t994 b = (guid84).e2;
                            t993 a = (guid84).e1;
                            
                            (((retA).data)[i] = a);
                            return (((retB).data)[i] = b);
                        })());
                    }
                    return {};
                })());
                return (juniper::tuple2<juniper::records::recordt_0<juniper::array<t993, c124>, uint32_t>,juniper::records::recordt_0<juniper::array<t994, c124>, uint32_t>>{retA, retB});
            })());
        })());
    }
}

namespace List {
    template<typename t1002, int c130>
    t1002 sum(juniper::records::recordt_0<juniper::array<t1002, c130>, uint32_t> lst) {
        return (([&]() -> t1002 {
            using a = t1002;
            constexpr int32_t n = c130;
            return List::fold<t1002, void, t1002, c130>(Prelude::add<t1002>, ((t1002) 0), lst);
        })());
    }
}

namespace List {
    template<typename t1020, int c132>
    t1020 average(juniper::records::recordt_0<juniper::array<t1020, c132>, uint32_t> lst) {
        return (([&]() -> t1020 {
            using a = t1020;
            constexpr int32_t n = c132;
            return ((t1020) (sum<t1020, c132>(lst) / cast<uint32_t, t1020>((lst).length)));
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
    template<typename t1045, typename t1047, typename t1049, typename t1080, int c134>
    juniper::unit siftDown(juniper::records::recordt_0<juniper::array<t1080, c134>, uint32_t>& lst, juniper::function<t1049, t1045(t1080)> key, uint32_t root, t1047 end) {
        return (([&]() -> juniper::unit {
            using m = t1045;
            using t = t1080;
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
                                    t1080 guid87 = ((lst).data)[root];
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    t1080 tmp = guid87;
                                    
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
    template<typename t1091, typename t1100, typename t1102, int c143>
    juniper::unit heapify(juniper::records::recordt_0<juniper::array<t1091, c143>, uint32_t>& lst, juniper::function<t1102, t1100(t1091)> key) {
        return (([&]() -> juniper::unit {
            using t = t1091;
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
                            return siftDown<t1100, uint32_t, t1102, t1091, c143>(lst, key, start, (lst).length);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1113, typename t1115, typename t1128, int c145>
    juniper::unit sort(juniper::function<t1115, t1113(t1128)> key, juniper::records::recordt_0<juniper::array<t1128, c145>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using m = t1113;
            using t = t1128;
            constexpr int32_t n = c145;
            return (([&]() -> juniper::unit {
                heapify<t1128, t1113, t1115, c145>(lst, key);
                uint32_t guid89 = (lst).length;
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t end = guid89;
                
                return (([&]() -> juniper::unit {
                    while (((bool) (end > ((uint32_t) 1)))) {
                        (([&]() -> juniper::unit {
                            (end -= ((uint32_t) 1));
                            t1128 guid90 = ((lst).data)[((uint32_t) 0)];
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t1128 tmp = guid90;
                            
                            (((lst).data)[((uint32_t) 0)] = ((lst).data)[end]);
                            (((lst).data)[end] = tmp);
                            return siftDown<t1113, uint32_t, t1115, t1128, c145>(lst, key, ((uint32_t) 0), end);
                        })());
                    }
                    return {};
                })());
            })());
        })());
    }
}

namespace List {
    template<typename t1146, typename t1147, typename t1148, int c152>
    juniper::records::recordt_0<juniper::array<t1147, c152>, uint32_t> sorted(juniper::function<t1148, t1146(t1147)> key, juniper::records::recordt_0<juniper::array<t1147, c152>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t1147, c152>, uint32_t> {
            using m = t1146;
            using t = t1147;
            constexpr int32_t n = c152;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t1147, c152>, uint32_t> {
                sort<t1146, t1148, t1147, c152>(key, lst);
                return lst;
            })());
        })());
    }
}

namespace Signal {
    template<typename t1158, typename t1160, typename t1175>
    Prelude::sig<t1175> map(juniper::function<t1160, t1175(t1158)> f, Prelude::sig<t1158> s) {
        return (([&]() -> Prelude::sig<t1175> {
            using a = t1158;
            using b = t1175;
            return (([&]() -> Prelude::sig<t1175> {
                Prelude::sig<t1158> guid91 = s;
                return (((bool) (((bool) ((guid91).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid91).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1175> {
                        t1158 val = ((guid91).signal()).just();
                        return signal<t1175>(just<t1175>(f(val)));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1175> {
                            return signal<t1175>(nothing<t1175>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1175>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1244, typename t1245>
    juniper::unit sink(juniper::function<t1245, juniper::unit(t1244)> f, Prelude::sig<t1244> s) {
        return (([&]() -> juniper::unit {
            using a = t1244;
            return (([&]() -> juniper::unit {
                Prelude::sig<t1244> guid92 = s;
                return (((bool) (((bool) ((guid92).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid92).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::unit {
                        t1244 val = ((guid92).signal()).just();
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
    template<typename t1268, typename t1282>
    Prelude::sig<t1282> filter(juniper::function<t1268, bool(t1282)> f, Prelude::sig<t1282> s) {
        return (([&]() -> Prelude::sig<t1282> {
            using a = t1282;
            return (([&]() -> Prelude::sig<t1282> {
                Prelude::sig<t1282> guid93 = s;
                return (((bool) (((bool) ((guid93).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid93).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1282> {
                        t1282 val = ((guid93).signal()).just();
                        return (f(val) ? 
                            (([&]() -> Prelude::sig<t1282> {
                                return signal<t1282>(nothing<t1282>());
                            })())
                        :
                            (([&]() -> Prelude::sig<t1282> {
                                return s;
                            })()));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1282> {
                            return signal<t1282>(nothing<t1282>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1282>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1362>
    Prelude::sig<t1362> merge(Prelude::sig<t1362> sigA, Prelude::sig<t1362> sigB) {
        return (([&]() -> Prelude::sig<t1362> {
            using a = t1362;
            return (([&]() -> Prelude::sig<t1362> {
                Prelude::sig<t1362> guid94 = sigA;
                return (((bool) (((bool) ((guid94).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid94).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1362> {
                        return sigA;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1362> {
                            return sigB;
                        })())
                    :
                        juniper::quit<Prelude::sig<t1362>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1398, int c154>
    Prelude::sig<t1398> mergeMany(juniper::records::recordt_0<juniper::array<Prelude::sig<t1398>, c154>, uint32_t> sigs) {
        return (([&]() -> Prelude::sig<t1398> {
            using a = t1398;
            constexpr int32_t n = c154;
            return (([&]() -> Prelude::sig<t1398> {
                Prelude::maybe<t1398> guid95 = List::fold<Prelude::maybe<t1398>, void, Prelude::sig<t1398>, c154>(juniper::function<void, Prelude::maybe<t1398>(Prelude::sig<t1398>,Prelude::maybe<t1398>)>([](Prelude::sig<t1398> sig, Prelude::maybe<t1398> accum) -> Prelude::maybe<t1398> { 
                    return (([&]() -> Prelude::maybe<t1398> {
                        Prelude::maybe<t1398> guid96 = accum;
                        return (((bool) (((bool) ((guid96).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::maybe<t1398> {
                                return (([&]() -> Prelude::maybe<t1398> {
                                    Prelude::sig<t1398> guid97 = sig;
                                    if (!(((bool) (((bool) ((guid97).id() == ((uint8_t) 0))) && true)))) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    Prelude::maybe<t1398> heldValue = (guid97).signal();
                                    
                                    return heldValue;
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<t1398> {
                                    return accum;
                                })())
                            :
                                juniper::quit<Prelude::maybe<t1398>>()));
                    })());
                 }), nothing<t1398>(), sigs);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t1398> ret = guid95;
                
                return signal<t1398>(ret);
            })());
        })());
    }
}

namespace Signal {
    template<typename t1516, typename t1540>
    Prelude::sig<Prelude::either<t1540, t1516>> join(Prelude::sig<t1540> sigA, Prelude::sig<t1516> sigB) {
        return (([&]() -> Prelude::sig<Prelude::either<t1540, t1516>> {
            using a = t1540;
            using b = t1516;
            return (([&]() -> Prelude::sig<Prelude::either<t1540, t1516>> {
                juniper::tuple2<Prelude::sig<t1540>, Prelude::sig<t1516>> guid98 = (juniper::tuple2<Prelude::sig<t1540>,Prelude::sig<t1516>>{sigA, sigB});
                return (((bool) (((bool) (((guid98).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid98).e1).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<Prelude::either<t1540, t1516>> {
                        t1540 value = (((guid98).e1).signal()).just();
                        return signal<Prelude::either<t1540, t1516>>(just<Prelude::either<t1540, t1516>>(left<t1540, t1516>(value)));
                    })())
                :
                    (((bool) (((bool) (((guid98).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid98).e2).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> Prelude::sig<Prelude::either<t1540, t1516>> {
                            t1516 value = (((guid98).e2).signal()).just();
                            return signal<Prelude::either<t1540, t1516>>(just<Prelude::either<t1540, t1516>>(right<t1540, t1516>(value)));
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<Prelude::either<t1540, t1516>> {
                                return signal<Prelude::either<t1540, t1516>>(nothing<Prelude::either<t1540, t1516>>());
                            })())
                        :
                            juniper::quit<Prelude::sig<Prelude::either<t1540, t1516>>>())));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1829>
    Prelude::sig<juniper::unit> toUnit(Prelude::sig<t1829> s) {
        return (([&]() -> Prelude::sig<juniper::unit> {
            using a = t1829;
            return map<t1829, void, juniper::unit>(juniper::function<void, juniper::unit(t1829)>([](t1829 x) -> juniper::unit { 
                return juniper::unit();
             }), s);
        })());
    }
}

namespace Signal {
    template<typename t1863, typename t1865, typename t1881>
    Prelude::sig<t1881> foldP(juniper::function<t1865, t1881(t1863, t1881)> f, t1881& state0, Prelude::sig<t1863> incoming) {
        return (([&]() -> Prelude::sig<t1881> {
            using a = t1863;
            using state = t1881;
            return (([&]() -> Prelude::sig<t1881> {
                Prelude::sig<t1863> guid99 = incoming;
                return (((bool) (((bool) ((guid99).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid99).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1881> {
                        t1863 val = ((guid99).signal()).just();
                        return (([&]() -> Prelude::sig<t1881> {
                            t1881 guid100 = f(val, state0);
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            t1881 state1 = guid100;
                            
                            (state0 = state1);
                            return signal<t1881>(just<t1881>(state1));
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1881> {
                            return signal<t1881>(nothing<t1881>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t1881>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t1958>
    Prelude::sig<t1958> dropRepeats(Prelude::maybe<t1958>& maybePrevValue, Prelude::sig<t1958> incoming) {
        return (([&]() -> Prelude::sig<t1958> {
            using a = t1958;
            return (([&]() -> Prelude::sig<t1958> {
                Prelude::sig<t1958> guid101 = incoming;
                return (((bool) (((bool) ((guid101).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid101).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t1958> {
                        t1958 value = ((guid101).signal()).just();
                        return (([&]() -> Prelude::sig<t1958> {
                            Prelude::sig<t1958> guid102 = (([&]() -> Prelude::sig<t1958> {
                                Prelude::maybe<t1958> guid103 = maybePrevValue;
                                return (((bool) (((bool) ((guid103).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> Prelude::sig<t1958> {
                                        return incoming;
                                    })())
                                :
                                    (((bool) (((bool) ((guid103).id() == ((uint8_t) 0))) && true)) ? 
                                        (([&]() -> Prelude::sig<t1958> {
                                            t1958 prevValue = (guid103).just();
                                            return (((bool) (value == prevValue)) ? 
                                                signal<t1958>(nothing<t1958>())
                                            :
                                                incoming);
                                        })())
                                    :
                                        juniper::quit<Prelude::sig<t1958>>()));
                            })());
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            Prelude::sig<t1958> ret = guid102;
                            
                            (maybePrevValue = just<t1958>(value));
                            return ret;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t1958> {
                            return incoming;
                        })())
                    :
                        juniper::quit<Prelude::sig<t1958>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2057>
    Prelude::sig<t2057> latch(t2057& prevValue, Prelude::sig<t2057> incoming) {
        return (([&]() -> Prelude::sig<t2057> {
            using a = t2057;
            return (([&]() -> Prelude::sig<t2057> {
                Prelude::sig<t2057> guid104 = incoming;
                return (((bool) (((bool) ((guid104).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid104).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> Prelude::sig<t2057> {
                        t2057 val = ((guid104).signal()).just();
                        return (([&]() -> Prelude::sig<t2057> {
                            (prevValue = val);
                            return incoming;
                        })());
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t2057> {
                            return signal<t2057>(just<t2057>(prevValue));
                        })())
                    :
                        juniper::quit<Prelude::sig<t2057>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2117, typename t2118, typename t2121, typename t2129>
    Prelude::sig<t2117> map2(juniper::function<t2118, t2117(t2121, t2129)> f, juniper::tuple2<t2121, t2129>& state, Prelude::sig<t2121> incomingA, Prelude::sig<t2129> incomingB) {
        return (([&]() -> Prelude::sig<t2117> {
            using a = t2121;
            using b = t2129;
            using c = t2117;
            return (([&]() -> Prelude::sig<t2117> {
                t2121 guid105 = (([&]() -> t2121 {
                    Prelude::sig<t2121> guid106 = incomingA;
                    return (((bool) (((bool) ((guid106).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid106).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t2121 {
                            t2121 val1 = ((guid106).signal()).just();
                            return val1;
                        })())
                    :
                        (true ? 
                            (([&]() -> t2121 {
                                return fst<t2121, t2129>(state);
                            })())
                        :
                            juniper::quit<t2121>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t2121 valA = guid105;
                
                t2129 guid107 = (([&]() -> t2129 {
                    Prelude::sig<t2129> guid108 = incomingB;
                    return (((bool) (((bool) ((guid108).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid108).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                        (([&]() -> t2129 {
                            t2129 val2 = ((guid108).signal()).just();
                            return val2;
                        })())
                    :
                        (true ? 
                            (([&]() -> t2129 {
                                return snd<t2121, t2129>(state);
                            })())
                        :
                            juniper::quit<t2129>()));
                })());
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t2129 valB = guid107;
                
                (state = (juniper::tuple2<t2121,t2129>{valA, valB}));
                return (([&]() -> Prelude::sig<t2117> {
                    juniper::tuple2<Prelude::sig<t2121>, Prelude::sig<t2129>> guid109 = (juniper::tuple2<Prelude::sig<t2121>,Prelude::sig<t2129>>{incomingA, incomingB});
                    return (((bool) (((bool) (((guid109).e2).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid109).e2).signal()).id() == ((uint8_t) 1))) && ((bool) (((bool) (((guid109).e1).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid109).e1).signal()).id() == ((uint8_t) 1))) && true)))))))) ? 
                        (([&]() -> Prelude::sig<t2117> {
                            return signal<t2117>(nothing<t2117>());
                        })())
                    :
                        (true ? 
                            (([&]() -> Prelude::sig<t2117> {
                                return signal<t2117>(just<t2117>(f(valA, valB)));
                            })())
                        :
                            juniper::quit<Prelude::sig<t2117>>()));
                })());
            })());
        })());
    }
}

namespace Signal {
    template<typename t2271, int c156>
    Prelude::sig<juniper::records::recordt_0<juniper::array<t2271, c156>, uint32_t>> record(juniper::records::recordt_0<juniper::array<t2271, c156>, uint32_t>& pastValues, Prelude::sig<t2271> incoming) {
        return (([&]() -> Prelude::sig<juniper::records::recordt_0<juniper::array<t2271, c156>, uint32_t>> {
            using a = t2271;
            constexpr int32_t n = c156;
            return foldP<t2271, void, juniper::records::recordt_0<juniper::array<t2271, c156>, uint32_t>>(List::prependPure<t2271, c156>, pastValues, incoming);
        })());
    }
}

namespace Signal {
    template<typename t2309>
    Prelude::sig<t2309> constant(t2309 val) {
        return (([&]() -> Prelude::sig<t2309> {
            using a = t2309;
            return signal<t2309>(just<t2309>(val));
        })());
    }
}

namespace Signal {
    template<typename t2344>
    Prelude::sig<Prelude::maybe<t2344>> meta(Prelude::sig<t2344> sigA) {
        return (([&]() -> Prelude::sig<Prelude::maybe<t2344>> {
            using a = t2344;
            return (([&]() -> Prelude::sig<Prelude::maybe<t2344>> {
                Prelude::sig<t2344> guid110 = sigA;
                if (!(((bool) (((bool) ((guid110).id() == ((uint8_t) 0))) && true)))) {
                    juniper::quit<juniper::unit>();
                }
                Prelude::maybe<t2344> val = (guid110).signal();
                
                return constant<Prelude::maybe<t2344>>(val);
            })());
        })());
    }
}

namespace Signal {
    template<typename t2411>
    Prelude::sig<t2411> unmeta(Prelude::sig<Prelude::maybe<t2411>> sigA) {
        return (([&]() -> Prelude::sig<t2411> {
            using a = t2411;
            return (([&]() -> Prelude::sig<t2411> {
                Prelude::sig<Prelude::maybe<t2411>> guid111 = sigA;
                return (((bool) (((bool) ((guid111).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid111).signal()).id() == ((uint8_t) 0))) && ((bool) (((bool) ((((guid111).signal()).just()).id() == ((uint8_t) 0))) && true)))))) ? 
                    (([&]() -> Prelude::sig<t2411> {
                        t2411 val = (((guid111).signal()).just()).just();
                        return constant<t2411>(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::sig<t2411> {
                            return signal<t2411>(nothing<t2411>());
                        })())
                    :
                        juniper::quit<Prelude::sig<t2411>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2487, typename t2488>
    Prelude::sig<juniper::tuple2<t2487, t2488>> zip(juniper::tuple2<t2487, t2488>& state, Prelude::sig<t2487> sigA, Prelude::sig<t2488> sigB) {
        return (([&]() -> Prelude::sig<juniper::tuple2<t2487, t2488>> {
            using a = t2487;
            using b = t2488;
            return map2<juniper::tuple2<t2487, t2488>, void, t2487, t2488>(juniper::function<void, juniper::tuple2<t2487, t2488>(t2487,t2488)>([](t2487 valA, t2488 valB) -> juniper::tuple2<t2487, t2488> { 
                return (juniper::tuple2<t2487,t2488>{valA, valB});
             }), state, sigA, sigB);
        })());
    }
}

namespace Signal {
    template<typename t2558, typename t2565>
    juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>> unzip(Prelude::sig<juniper::tuple2<t2558, t2565>> incoming) {
        return (([&]() -> juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>> {
            using a = t2558;
            using b = t2565;
            return (([&]() -> juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>> {
                Prelude::sig<juniper::tuple2<t2558, t2565>> guid112 = incoming;
                return (((bool) (((bool) ((guid112).id() == ((uint8_t) 0))) && ((bool) (((bool) (((guid112).signal()).id() == ((uint8_t) 0))) && true)))) ? 
                    (([&]() -> juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>> {
                        t2565 y = (((guid112).signal()).just()).e2;
                        t2558 x = (((guid112).signal()).just()).e1;
                        return (juniper::tuple2<Prelude::sig<t2558>,Prelude::sig<t2565>>{signal<t2558>(just<t2558>(x)), signal<t2565>(just<t2565>(y))});
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>> {
                            return (juniper::tuple2<Prelude::sig<t2558>,Prelude::sig<t2565>>{signal<t2558>(nothing<t2558>()), signal<t2565>(nothing<t2565>())});
                        })())
                    :
                        juniper::quit<juniper::tuple2<Prelude::sig<t2558>, Prelude::sig<t2565>>>()));
            })());
        })());
    }
}

namespace Signal {
    template<typename t2692, typename t2693>
    Prelude::sig<t2692> toggle(t2692 val1, t2692 val2, t2692& state, Prelude::sig<t2693> incoming) {
        return (([&]() -> Prelude::sig<t2692> {
            using a = t2692;
            using b = t2693;
            return foldP<t2693, juniper::closures::closuret_4<t2692, t2692>, t2692>(juniper::function<juniper::closures::closuret_4<t2692, t2692>, t2692(t2693,t2692)>(juniper::closures::closuret_4<t2692, t2692>(val1, val2), [](juniper::closures::closuret_4<t2692, t2692>& junclosure, t2693 event, t2692 prevVal) -> t2692 { 
                t2692& val1 = junclosure.val1;
                t2692& val2 = junclosure.val2;
                return (((bool) (prevVal == val1)) ? 
                    (([&]() -> t2692 {
                        return val2;
                    })())
                :
                    (([&]() -> t2692 {
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
    template<typename t2748>
    t2748 baseToInt(Io::base b) {
        return (([&]() -> t2748 {
            Io::base guid114 = b;
            return (((bool) (((bool) ((guid114).id() == ((uint8_t) 0))) && true)) ? 
                (([&]() -> t2748 {
                    return ((t2748) 2);
                })())
            :
                (((bool) (((bool) ((guid114).id() == ((uint8_t) 1))) && true)) ? 
                    (([&]() -> t2748 {
                        return ((t2748) 8);
                    })())
                :
                    (((bool) (((bool) ((guid114).id() == ((uint8_t) 2))) && true)) ? 
                        (([&]() -> t2748 {
                            return ((t2748) 10);
                        })())
                    :
                        (((bool) (((bool) ((guid114).id() == ((uint8_t) 3))) && true)) ? 
                            (([&]() -> t2748 {
                                return ((t2748) 16);
                            })())
                        :
                            juniper::quit<t2748>()))));
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
    template<typename t3202, typename t3204, typename t3213>
    Prelude::maybe<t3213> map(juniper::function<t3204, t3213(t3202)> f, Prelude::maybe<t3202> maybeVal) {
        return (([&]() -> Prelude::maybe<t3213> {
            using a = t3202;
            using b = t3213;
            return (([&]() -> Prelude::maybe<t3213> {
                Prelude::maybe<t3202> guid130 = maybeVal;
                return (((bool) (((bool) ((guid130).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t3213> {
                        t3202 val = (guid130).just();
                        return just<t3213>(f(val));
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t3213> {
                            return nothing<t3213>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t3213>>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3243>
    t3243 get(Prelude::maybe<t3243> maybeVal) {
        return (([&]() -> t3243 {
            using a = t3243;
            return (([&]() -> t3243 {
                Prelude::maybe<t3243> guid131 = maybeVal;
                return (((bool) (((bool) ((guid131).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t3243 {
                        t3243 val = (guid131).just();
                        return val;
                    })())
                :
                    juniper::quit<t3243>());
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3252>
    bool isJust(Prelude::maybe<t3252> maybeVal) {
        return (([&]() -> bool {
            using a = t3252;
            return (([&]() -> bool {
                Prelude::maybe<t3252> guid132 = maybeVal;
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
    template<typename t3263>
    bool isNothing(Prelude::maybe<t3263> maybeVal) {
        return (([&]() -> bool {
            using a = t3263;
            return !(isJust<t3263>(maybeVal));
        })());
    }
}

namespace Maybe {
    template<typename t3277>
    uint8_t count(Prelude::maybe<t3277> maybeVal) {
        return (([&]() -> uint8_t {
            using a = t3277;
            return (([&]() -> uint8_t {
                Prelude::maybe<t3277> guid133 = maybeVal;
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
    template<typename t3291, typename t3292, typename t3293>
    t3291 fold(juniper::function<t3293, t3291(t3292, t3291)> f, t3291 initState, Prelude::maybe<t3292> maybeVal) {
        return (([&]() -> t3291 {
            using state = t3291;
            using t = t3292;
            return (([&]() -> t3291 {
                Prelude::maybe<t3292> guid134 = maybeVal;
                return (((bool) (((bool) ((guid134).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> t3291 {
                        t3292 val = (guid134).just();
                        return f(val, initState);
                    })())
                :
                    (true ? 
                        (([&]() -> t3291 {
                            return initState;
                        })())
                    :
                        juniper::quit<t3291>()));
            })());
        })());
    }
}

namespace Maybe {
    template<typename t3309, typename t3310>
    juniper::unit iter(juniper::function<t3310, juniper::unit(t3309)> f, Prelude::maybe<t3309> maybeVal) {
        return (([&]() -> juniper::unit {
            using a = t3309;
            return (([&]() -> juniper::unit {
                Prelude::maybe<t3309> guid135 = maybeVal;
                return (((bool) (((bool) ((guid135).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> juniper::unit {
                        t3309 val = (guid135).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> juniper::unit {
                            Prelude::maybe<t3309> nothing = guid135;
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
    template<typename t3454>
    t3454 abs_(t3454 x) {
        return (([&]() -> t3454 {
            using t = t3454;
            return (([&]() -> t3454 {
                return (((bool) (x < ((t3454) 0))) ? 
                    (([&]() -> t3454 {
                        return -(x);
                    })())
                :
                    (([&]() -> t3454 {
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
    template<typename t3465>
    t3465 max_(t3465 x, t3465 y) {
        return (([&]() -> t3465 {
            using a = t3465;
            return (((bool) (x > y)) ? 
                (([&]() -> t3465 {
                    return x;
                })())
            :
                (([&]() -> t3465 {
                    return y;
                })()));
        })());
    }
}

namespace Math {
    template<typename t3467>
    t3467 mapRange(t3467 x, t3467 a1, t3467 a2, t3467 b1, t3467 b2) {
        return (([&]() -> t3467 {
            using t = t3467;
            return ((t3467) (b1 + ((t3467) (((t3467) (((t3467) (x - a1)) * ((t3467) (b2 - b1)))) / ((t3467) (a2 - a1))))));
        })());
    }
}

namespace Math {
    template<typename t3469>
    t3469 clamp(t3469 x, t3469 min, t3469 max) {
        return (([&]() -> t3469 {
            using a = t3469;
            return (((bool) (min > x)) ? 
                (([&]() -> t3469 {
                    return min;
                })())
            :
                (((bool) (x > max)) ? 
                    (([&]() -> t3469 {
                        return max;
                    })())
                :
                    (([&]() -> t3469 {
                        return x;
                    })())));
        })());
    }
}

namespace Math {
    template<typename t3474>
    int8_t sign(t3474 n) {
        return (([&]() -> int8_t {
            using a = t3474;
            return (((bool) (n == ((t3474) 0))) ? 
                (([&]() -> int8_t {
                    return ((int8_t) 0);
                })())
            :
                (((bool) (n > ((t3474) 0))) ? 
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
    template<typename t3594, int c159>
    juniper::array<t3594, c159> add(juniper::array<t3594, c159> v1, juniper::array<t3594, c159> v2) {
        return (([&]() -> juniper::array<t3594, c159> {
            using a = t3594;
            constexpr int32_t n = c159;
            return (([&]() -> juniper::array<t3594, c159> {
                (([&]() -> juniper::unit {
                    int32_t guid162 = ((int32_t) 0);
                    int32_t guid163 = n;
                    for (int32_t i = guid162; i < guid163; i++) {
                        (([&]() -> t3594 {
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
    template<typename t3598, int c162>
    juniper::array<t3598, c162> zero() {
        return (([&]() -> juniper::array<t3598, c162> {
            using a = t3598;
            constexpr int32_t n = c162;
            return array<t3598, c162>(((t3598) 0));
        })());
    }
}

namespace Vector {
    template<typename t3606, int c164>
    juniper::array<t3606, c164> subtract(juniper::array<t3606, c164> v1, juniper::array<t3606, c164> v2) {
        return (([&]() -> juniper::array<t3606, c164> {
            using a = t3606;
            constexpr int32_t n = c164;
            return (([&]() -> juniper::array<t3606, c164> {
                (([&]() -> juniper::unit {
                    int32_t guid164 = ((int32_t) 0);
                    int32_t guid165 = n;
                    for (int32_t i = guid164; i < guid165; i++) {
                        (([&]() -> t3606 {
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
    template<typename t3610, int c167>
    juniper::array<t3610, c167> scale(t3610 scalar, juniper::array<t3610, c167> v) {
        return (([&]() -> juniper::array<t3610, c167> {
            using a = t3610;
            constexpr int32_t n = c167;
            return (([&]() -> juniper::array<t3610, c167> {
                (([&]() -> juniper::unit {
                    int32_t guid166 = ((int32_t) 0);
                    int32_t guid167 = n;
                    for (int32_t i = guid166; i < guid167; i++) {
                        (([&]() -> t3610 {
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
    template<typename t3616, int c169>
    t3616 dot(juniper::array<t3616, c169> v1, juniper::array<t3616, c169> v2) {
        return (([&]() -> t3616 {
            using a = t3616;
            constexpr int32_t n = c169;
            return (([&]() -> t3616 {
                t3616 guid168 = ((t3616) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t3616 sum = guid168;
                
                (([&]() -> juniper::unit {
                    int32_t guid169 = ((int32_t) 0);
                    int32_t guid170 = n;
                    for (int32_t i = guid169; i < guid170; i++) {
                        (([&]() -> t3616 {
                            return (sum += ((t3616) ((v1)[i] * (v2)[i])));
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
    template<typename t3622, int c172>
    t3622 magnitude2(juniper::array<t3622, c172> v) {
        return (([&]() -> t3622 {
            using a = t3622;
            constexpr int32_t n = c172;
            return (([&]() -> t3622 {
                t3622 guid171 = ((t3622) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                t3622 sum = guid171;
                
                (([&]() -> juniper::unit {
                    int32_t guid172 = ((int32_t) 0);
                    int32_t guid173 = n;
                    for (int32_t i = guid172; i < guid173; i++) {
                        (([&]() -> t3622 {
                            return (sum += ((t3622) ((v)[i] * (v)[i])));
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
    template<typename t3624, int c175>
    double magnitude(juniper::array<t3624, c175> v) {
        return (([&]() -> double {
            using a = t3624;
            constexpr int32_t n = c175;
            return sqrt_(toDouble<t3624>(magnitude2<t3624, c175>(v)));
        })());
    }
}

namespace Vector {
    template<typename t3640, int c177>
    juniper::array<t3640, c177> multiply(juniper::array<t3640, c177> u, juniper::array<t3640, c177> v) {
        return (([&]() -> juniper::array<t3640, c177> {
            using a = t3640;
            constexpr int32_t n = c177;
            return (([&]() -> juniper::array<t3640, c177> {
                (([&]() -> juniper::unit {
                    int32_t guid174 = ((int32_t) 0);
                    int32_t guid175 = n;
                    for (int32_t i = guid174; i < guid175; i++) {
                        (([&]() -> t3640 {
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
    template<typename t3649, int c180>
    juniper::array<t3649, c180> normalize(juniper::array<t3649, c180> v) {
        return (([&]() -> juniper::array<t3649, c180> {
            using a = t3649;
            constexpr int32_t n = c180;
            return (([&]() -> juniper::array<t3649, c180> {
                double guid176 = magnitude<t3649, c180>(v);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                double mag = guid176;
                
                (([&]() -> juniper::unit {
                    if (((bool) (mag > ((t3649) 0)))) {
                        (([&]() -> juniper::unit {
                            return (([&]() -> juniper::unit {
                                int32_t guid177 = ((int32_t) 0);
                                int32_t guid178 = n;
                                for (int32_t i = guid177; i < guid178; i++) {
                                    (([&]() -> t3649 {
                                        return ((v)[i] = fromDouble<t3649>(((double) (toDouble<t3649>((v)[i]) / mag))));
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
    template<typename t3660, int c184>
    double angle(juniper::array<t3660, c184> v1, juniper::array<t3660, c184> v2) {
        return (([&]() -> double {
            using a = t3660;
            constexpr int32_t n = c184;
            return acos_(((double) (toDouble<t3660>(dot<t3660, c184>(v1, v2)) / sqrt_(toDouble<t3660>(((t3660) (magnitude2<t3660, c184>(v1) * magnitude2<t3660, c184>(v2))))))));
        })());
    }
}

namespace Vector {
    template<typename t3702>
    juniper::array<t3702, 3> cross(juniper::array<t3702, 3> u, juniper::array<t3702, 3> v) {
        return (([&]() -> juniper::array<t3702, 3> {
            using a = t3702;
            return (juniper::array<t3702, 3> { {((t3702) (((t3702) ((u)[y] * (v)[z])) - ((t3702) ((u)[z] * (v)[y])))), ((t3702) (((t3702) ((u)[z] * (v)[x])) - ((t3702) ((u)[x] * (v)[z])))), ((t3702) (((t3702) ((u)[x] * (v)[y])) - ((t3702) ((u)[y] * (v)[x]))))} });
        })());
    }
}

namespace Vector {
    template<typename t3704, int c200>
    juniper::array<t3704, c200> project(juniper::array<t3704, c200> a, juniper::array<t3704, c200> b) {
        return (([&]() -> juniper::array<t3704, c200> {
            using z = t3704;
            constexpr int32_t n = c200;
            return (([&]() -> juniper::array<t3704, c200> {
                juniper::array<t3704, c200> guid179 = normalize<t3704, c200>(b);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t3704, c200> bn = guid179;
                
                return scale<t3704, c200>(dot<t3704, c200>(a, bn), bn);
            })());
        })());
    }
}

namespace Vector {
    template<typename t3720, int c204>
    juniper::array<t3720, c204> projectPlane(juniper::array<t3720, c204> a, juniper::array<t3720, c204> m) {
        return (([&]() -> juniper::array<t3720, c204> {
            using z = t3720;
            constexpr int32_t n = c204;
            return subtract<t3720, c204>(a, project<t3720, c204>(a, m));
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
    template<typename t3798, int c227>
    t3798 choice(juniper::records::recordt_0<juniper::array<t3798, c227>, uint32_t> lst) {
        return (([&]() -> t3798 {
            using t = t3798;
            constexpr int32_t n = c227;
            return (([&]() -> t3798 {
                int32_t guid192 = random_(((int32_t) 0), u32ToI32((lst).length));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t i = guid192;
                
                return List::get<uint32_t, t3798, c227>(i32ToU32(i), lst);
            })());
        })());
    }
}

namespace Random {
    template<typename t3818, int c229>
    Prelude::maybe<t3818> tryChoice(juniper::records::recordt_0<juniper::array<t3818, c229>, uint32_t> lst) {
        return (([&]() -> Prelude::maybe<t3818> {
            using t = t3818;
            constexpr int32_t n = c229;
            return (([&]() -> Prelude::maybe<t3818> {
                return (((bool) ((lst).length == ((uint32_t) 0))) ? 
                    (([&]() -> Prelude::maybe<t3818> {
                        return nothing<t3818>();
                    })())
                :
                    (([&]() -> Prelude::maybe<t3818> {
                        int32_t guid193 = random_(((int32_t) 0), u32ToI32((lst).length));
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        int32_t i = guid193;
                        
                        return List::tryGet<uint32_t, t3818, c229>(i32ToU32(i), lst);
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

namespace MaybeExt {
    template<typename t3938, typename t3940, typename t3945>
    Prelude::maybe<t3945> flatMap(juniper::function<t3940, Prelude::maybe<t3945>(t3938)> f, Prelude::maybe<t3938> maybeVal) {
        return (([&]() -> Prelude::maybe<t3945> {
            using a = t3938;
            using b = t3945;
            return (([&]() -> Prelude::maybe<t3945> {
                Prelude::maybe<t3938> guid212 = maybeVal;
                return (((bool) (((bool) ((guid212).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t3945> {
                        t3938 val = (guid212).just();
                        return f(val);
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t3945> {
                            return nothing<t3945>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t3945>>()));
            })());
        })());
    }
}

namespace MaybeExt {
    template<typename t3977>
    Prelude::maybe<t3977> flatten(Prelude::maybe<Prelude::maybe<t3977>> maybeVal) {
        return (([&]() -> Prelude::maybe<t3977> {
            using a = t3977;
            return (([&]() -> Prelude::maybe<t3977> {
                Prelude::maybe<Prelude::maybe<t3977>> guid213 = maybeVal;
                return (((bool) (((bool) ((guid213).id() == ((uint8_t) 0))) && true)) ? 
                    (([&]() -> Prelude::maybe<t3977> {
                        Prelude::maybe<t3977> val = (guid213).just();
                        return val;
                    })())
                :
                    (true ? 
                        (([&]() -> Prelude::maybe<t3977> {
                            return nothing<t3977>();
                        })())
                    :
                        juniper::quit<Prelude::maybe<t3977>>()));
            })());
        })());
    }
}

namespace ListExt {
    template<int c231, int c232>
    juniper::records::recordt_0<juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)>, uint32_t> range(int32_t lowerBound, int32_t upperBound) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)>, uint32_t> {
            constexpr int32_t from = c231;
            constexpr int32_t upTo = c232;
            return (([&]() -> juniper::records::recordt_0<juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)>, uint32_t> {
                int32_t guid214 = ((int32_t) (upTo - from));
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                int32_t length = guid214;
                
                juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)> guid215 = zeros<int32_t, ((c231)+((c232)*(-1)))*(-1)>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)> result = guid215;
                
                (([&]() -> juniper::unit {
                    int32_t guid216 = ((int32_t) 0);
                    int32_t guid217 = length;
                    for (int32_t i = guid216; i < guid217; i++) {
                        (([&]() -> int32_t {
                            return ((result)[i] = toInt32<float>(Math::mapRange<float>(toFloat<int32_t>(i), toFloat<int32_t>(from), toFloat<int32_t>(upTo), ((float) 0.0), toFloat<int32_t>(length))));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<int32_t, ((c231)+((c232)*(-1)))*(-1)>, uint32_t> guid218;
                    guid218.data = result;
                    guid218.length = cast<int32_t, uint32_t>(length);
                    return guid218;
                })());
            })());
        })());
    }
}

namespace ListExt {
    template<typename t4064, int c235>
    juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t4064>, c235>, uint32_t> enumerated(juniper::records::recordt_0<juniper::array<t4064, c235>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t4064>, c235>, uint32_t> {
            using a = t4064;
            constexpr int32_t n = c235;
            return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t4064>, c235>, uint32_t> {
                uint32_t guid219 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t offset = guid219;
                
                juniper::array<juniper::tuple2<uint32_t, t4064>, c235> guid220 = zeros<juniper::tuple2<uint32_t, t4064>, c235>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<juniper::tuple2<uint32_t, t4064>, c235> result = guid220;
                
                (([&]() -> juniper::unit {
                    uint32_t guid221 = ((uint32_t) 0);
                    uint32_t guid222 = cast<uint32_t, uint32_t>((lst).length);
                    for (uint32_t i = guid221; i < guid222; i++) {
                        (([&]() -> uint32_t {
                            ((result)[i] = (juniper::tuple2<uint32_t,t4064>{offset, ((lst).data)[i]}));
                            return (offset += ((uint32_t) 1));
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t4064>, c235>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<juniper::tuple2<uint32_t, t4064>, c235>, uint32_t> guid223;
                    guid223.data = result;
                    guid223.length = cast<int32_t, uint32_t>(n);
                    return guid223;
                })());
            })());
        })());
    }
}

namespace ListExt {
    template<typename t4108, int c239>
    juniper::unit rotate(int32_t step, juniper::records::recordt_0<juniper::array<t4108, c239>, uint32_t>& lst) {
        return (([&]() -> juniper::unit {
            using a = t4108;
            constexpr int32_t n = c239;
            return (([&]() -> juniper::unit {
                return (([&]() -> juniper::unit {
                    int32_t guid224 = n;
                    return (((bool) (((bool) (guid224 == ((int32_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            return juniper::unit();
                        })())
                    :
                        (true ? 
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                    int32_t guid225 = ((int32_t) (((int32_t) (((int32_t) (step % cast<int32_t, int32_t>(n))) + cast<int32_t, int32_t>(n))) % cast<int32_t, int32_t>(n)));
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    int32_t normalizedStep = guid225;
                                    
                                    juniper::array<t4108, c239> guid226 = zeros<t4108, c239>();
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    juniper::array<t4108, c239> result = guid226;
                                    
                                    (([&]() -> juniper::unit {
                                        int32_t guid227 = ((int32_t) 0);
                                        int32_t guid228 = cast<uint32_t, int32_t>((lst).length);
                                        for (int32_t i = guid227; i < guid228; i++) {
                                            (([&]() -> t4108 {
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
    template<typename t4145, int c243>
    juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> rotated(int32_t step, juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> lst) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> {
            using a = t4145;
            constexpr int32_t n = c243;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> {
                return (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> {
                    int32_t guid229 = n;
                    return (((bool) (((bool) (guid229 == ((int32_t) 0))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> {
                            return lst;
                        })())
                    :
                        (true ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> {
                                return (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> {
                                    int32_t guid230 = ((int32_t) (((int32_t) (((int32_t) (step % cast<int32_t, int32_t>(n))) + cast<int32_t, int32_t>(n))) % cast<int32_t, int32_t>(n)));
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    int32_t normalizedStep = guid230;
                                    
                                    juniper::array<t4145, c243> guid231 = zeros<t4145, c243>();
                                    if (!(true)) {
                                        juniper::quit<juniper::unit>();
                                    }
                                    juniper::array<t4145, c243> result = guid231;
                                    
                                    (([&]() -> juniper::unit {
                                        int32_t guid232 = ((int32_t) 0);
                                        int32_t guid233 = cast<uint32_t, int32_t>((lst).length);
                                        for (int32_t i = guid232; i < guid233; i++) {
                                            (([&]() -> t4145 {
                                                return ((result)[i] = ((lst).data)[((int32_t) (((int32_t) (i + normalizedStep)) % cast<int32_t, int32_t>(n)))]);
                                            })());
                                        }
                                        return {};
                                    })());
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t>{
                                        juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t> guid234;
                                        guid234.data = result;
                                        guid234.length = cast<int32_t, uint32_t>(n);
                                        return guid234;
                                    })());
                                })());
                            })())
                        :
                            juniper::quit<juniper::records::recordt_0<juniper::array<t4145, c243>, uint32_t>>()));
                })());
            })());
        })());
    }
}

namespace ListExt {
    template<typename t4166, int c247, int c248>
    juniper::records::recordt_0<juniper::array<t4166, c247>, uint32_t> replicateList(uint32_t nElements, juniper::records::recordt_0<juniper::array<t4166, c248>, uint32_t> elements) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<t4166, c247>, uint32_t> {
            using t = t4166;
            constexpr int32_t m = c247;
            constexpr int32_t n = c248;
            return (([&]() -> juniper::records::recordt_0<juniper::array<t4166, c247>, uint32_t> {
                juniper::array<t4166, c247> guid235 = zeros<t4166, c247>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<t4166, c247> ret = guid235;
                
                (([&]() -> juniper::unit {
                    int32_t guid236 = ((int32_t) 0);
                    int32_t guid237 = m;
                    for (int32_t i = guid236; i < guid237; i++) {
                        (([&]() -> t4166 {
                            return ((ret)[i] = ((elements).data)[((int32_t) (i % cast<uint32_t, int32_t>((elements).length)))]);
                        })());
                    }
                    return {};
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<t4166, c247>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<t4166, c247>, uint32_t> guid238;
                    guid238.data = ret;
                    guid238.length = nElements;
                    return guid238;
                })());
            })());
        })());
    }
}

namespace SignalExt {
    template<typename t4184>
    Prelude::sig<t4184> once(Prelude::maybe<t4184>& state) {
        return (([&]() -> Prelude::sig<t4184> {
            using t = t4184;
            return (([&]() -> Prelude::sig<t4184> {
                return (([&]() -> Prelude::sig<t4184> {
                    Prelude::maybe<t4184> guid239 = state;
                    return (((bool) (((bool) ((guid239).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> Prelude::sig<t4184> {
                            t4184 value = (guid239).just();
                            return (([&]() -> Prelude::sig<t4184> {
                                (state = nothing<t4184>());
                                return signal<t4184>(just<t4184>(value));
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid239).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> Prelude::sig<t4184> {
                                return signal<t4184>(nothing<t4184>());
                            })())
                        :
                            juniper::quit<Prelude::sig<t4184>>()));
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    Prelude::maybe<NeoPixel::Action> startAction = just<NeoPixel::Action>(start());
}

namespace NeoPixel {
    Prelude::sig<Prelude::maybe<NeoPixel::Action>> actions(Prelude::maybe<NeoPixel::Action>& prevAction) {
        return (([&]() -> Prelude::sig<Prelude::maybe<NeoPixel::Action>> {
            return Signal::meta<NeoPixel::Action>(Signal::mergeMany<NeoPixel::Action, 2>((([&]() -> juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Action>, 2>, uint32_t>{
                juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Action>, 2>, uint32_t> guid240;
                guid240.data = (juniper::array<Prelude::sig<NeoPixel::Action>, 2> { {SignalExt::once<NeoPixel::Action>(startAction), SignalExt::once<NeoPixel::Action>(prevAction)} });
                guid240.length = ((uint32_t) 2);
                return guid240;
            })())));
        })());
    }
}

namespace NeoPixel {
    NeoPixel::color getPixelColor(uint16_t n, NeoPixel::RawDevice line) {
        return (([&]() -> NeoPixel::color {
            NeoPixel::RawDevice guid241 = line;
            if (!(((bool) (((bool) ((guid241).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid241).device();
            
            uint32_t guid242 = ((uint32_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint32_t numRep = guid242;
            
            (([&]() -> juniper::unit {
                 numRep = ((Adafruit_NeoPixel*) p)->getPixelColor(n); 
                return {};
            })());
            return RGB(toUInt8<uint32_t>(((uint32_t) (numRep >> ((uint32_t) 16)))), toUInt8<uint32_t>(((uint32_t) (numRep >> ((uint32_t) 8)))), toUInt8<uint32_t>(numRep));
        })());
    }
}

namespace NeoPixel {
    template<int c253>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c253>, uint32_t> readPixels(Prelude::maybe<NeoPixel::RawDevice> device) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c253>, uint32_t> {
            constexpr int32_t n = c253;
            return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c253>, uint32_t> {
                uint32_t guid243 = ((uint32_t) 0);
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                uint32_t color = guid243;
                
                juniper::array<NeoPixel::color, c253> guid244 = zeros<NeoPixel::color, c253>();
                if (!(true)) {
                    juniper::quit<juniper::unit>();
                }
                juniper::array<NeoPixel::color, c253> pixels = guid244;
                
                (([&]() -> juniper::unit {
                    Prelude::maybe<NeoPixel::RawDevice> guid245 = device;
                    return (((bool) (((bool) ((guid245).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            NeoPixel::RawDevice device = (guid245).just();
                            return (([&]() -> juniper::unit {
                                (([&]() -> juniper::unit {
                                    uint16_t guid246 = ((uint16_t) 0);
                                    uint16_t guid247 = cast<int32_t, uint16_t>(n);
                                    for (uint16_t i = guid246; i < guid247; i++) {
                                        (([&]() -> NeoPixel::color {
                                            getPixelColor(i, device);
                                            return ((pixels)[i] = RGB(toUInt8<uint32_t>(((uint32_t) (color >> ((uint32_t) 16)))), toUInt8<uint32_t>(((uint32_t) (color >> ((uint32_t) 8)))), toUInt8<uint32_t>(color)));
                                        })());
                                    }
                                    return {};
                                })());
                                return juniper::unit();
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid245).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return juniper::unit();
                            })())
                        :
                            juniper::quit<juniper::unit>()));
                })());
                return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c253>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<NeoPixel::color, c253>, uint32_t> guid248;
                    guid248.data = pixels;
                    guid248.length = cast<int32_t, uint32_t>(n);
                    return guid248;
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c256, int c257>
    juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>>> initialState(juniper::array<juniper::records::recordt_7<uint16_t>, c257> descriptors, uint16_t nPixels) {
        return (([&]() -> juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>>> {
            constexpr int32_t m = c256;
            constexpr int32_t nLines = c257;
            return (([&]() -> juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>>> {
                return (juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>>>((([&]() -> juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>>{
                    juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, c257>, uint32_t>> guid249;
                    guid249.lines = List::map<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>, void, juniper::records::recordt_7<uint16_t>, c257>(juniper::function<void, juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>(juniper::records::recordt_7<uint16_t>)>([](juniper::records::recordt_7<uint16_t> descriptor) -> juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>> { 
                        return (([&]() -> juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>> {
                            Prelude::maybe<NeoPixel::RawDevice> guid250 = nothing<NeoPixel::RawDevice>();
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            Prelude::maybe<NeoPixel::RawDevice> device = guid250;
                            
                            juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t> guid251 = readPixels<c256>(device);
                            if (!(true)) {
                                juniper::quit<juniper::unit>();
                            }
                            juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t> pixels = guid251;
                            
                            return (juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>>((([&]() -> juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>>{
                                juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c256>, uint32_t>> guid252;
                                guid252.previousPixels = pixels;
                                guid252.pixels = pixels;
                                guid252.operation = nothing<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>();
                                guid252.pin = (descriptor).pin;
                                guid252.device = device;
                                return guid252;
                            })())));
                        })());
                     }), (([&]() -> juniper::records::recordt_0<juniper::array<juniper::records::recordt_7<uint16_t>, c257>, uint32_t>{
                        juniper::records::recordt_0<juniper::array<juniper::records::recordt_7<uint16_t>, c257>, uint32_t> guid253;
                        guid253.data = descriptors;
                        guid253.length = cast<int32_t, uint32_t>(nLines);
                        return guid253;
                    })()));
                    return guid249;
                })())));
            })());
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
    template<typename t4552>
    Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> runOperation(juniper::function<t4552, juniper::unit(NeoPixel::Function)> f, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> operation) {
        return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
            return Maybe::map<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>, juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(juniper::function<juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>(juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>)>(juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>(f, operation), [](juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>& junclosure, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation) -> juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> { 
                juniper::function<t4552, juniper::unit(NeoPixel::Function)>& f = junclosure.f;
                Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>& operation = junclosure.operation;
                return (([&]() -> juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> {
                    juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> guid254 = operation;
                    if (!(true)) {
                        juniper::quit<juniper::unit>();
                    }
                    juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation = guid254;
                    
                    Signal::sink<uint32_t, juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>(juniper::function<juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, juniper::unit(uint32_t)>(juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(f, operation), [](juniper::closures::closuret_6<juniper::function<t4552, juniper::unit(NeoPixel::Function)>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>& junclosure, uint32_t _) -> juniper::unit { 
                        juniper::function<t4552, juniper::unit(NeoPixel::Function)>& f = junclosure.f;
                        juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>& operation = junclosure.operation;
                        return (([&]() -> juniper::unit {
                            return f((operation).function);
                        })());
                     }), Time::every((operation).interval, (operation).timer));
                    return operation;
                })());
             }), operation);
        })());
    }
}

namespace NeoPixel {
    Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> updateOperation(Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> operation) {
        return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
            (([&]() -> juniper::unit {
                 Serial.println("updateOperation"); 
                return {};
            })());
            return MaybeExt::flatMap<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>, juniper::closures::closuret_7<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(juniper::function<juniper::closures::closuret_7<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>(juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>)>(juniper::closures::closuret_7<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>(operation), [](juniper::closures::closuret_7<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>& junclosure, juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> operation) -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> { 
                Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>& operation = junclosure.operation;
                return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                    return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                        Prelude::maybe<uint8_t> guid255 = (operation).endAfter;
                        return (((bool) (((bool) ((guid255).id() == ((uint8_t) 0))) && ((bool) (((bool) ((guid255).just() == ((uint8_t) 0))) && true)))) ? 
                            (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                                return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                                    return nothing<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>();
                                })());
                            })())
                        :
                            (true ? 
                                (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                                    return (([&]() -> Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>> {
                                        return just<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>((([&]() -> juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>{
                                            juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> guid256;
                                            guid256.function = (operation).function;
                                            guid256.interval = (operation).interval;
                                            guid256.timer = (operation).timer;
                                            guid256.endAfter = Maybe::map<uint8_t, void, uint8_t>(juniper::function<void, uint8_t(uint8_t)>([](uint8_t n) -> uint8_t { 
                                                return (([&]() -> uint8_t {
                                                    return Math::max_<uint8_t>(((uint8_t) 0), ((uint8_t) (n - ((uint8_t) 1))));
                                                })());
                                             }), (operation).endAfter);
                                            return guid256;
                                        })()));
                                    })());
                                })())
                            :
                                juniper::quit<Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>>()));
                    })());
                })());
             }), operation);
        })());
    }
}

namespace NeoPixel {
    template<int c260>
    juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> applyFunction(NeoPixel::Function fn, juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> pixels) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
            constexpr int32_t nPixels = c260;
            return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                    NeoPixel::Function guid257 = fn;
                    return (((bool) (((bool) ((guid257).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                            int16_t step = (guid257).rotate();
                            return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                                (([&]() -> juniper::unit {
                                     Serial.println("Rotate"); 
                                    return {};
                                })());
                                return ListExt::rotated<NeoPixel::color, c260>(cast<int16_t, int32_t>(step), pixels);
                            })());
                        })())
                    :
                        (((bool) (((bool) ((guid257).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                                NeoPixel::color color = (guid257).set();
                                return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                                    (([&]() -> juniper::unit {
                                         Serial.println("Set color"); 
                                        return {};
                                    })());
                                    return List::map<NeoPixel::color, juniper::closures::closuret_8<NeoPixel::color>, NeoPixel::color, c260>(juniper::function<juniper::closures::closuret_8<NeoPixel::color>, NeoPixel::color(NeoPixel::color)>(juniper::closures::closuret_8<NeoPixel::color>(color), [](juniper::closures::closuret_8<NeoPixel::color>& junclosure, NeoPixel::color _) -> NeoPixel::color { 
                                        NeoPixel::color& color = junclosure.color;
                                        return color;
                                     }), pixels);
                                })());
                            })())
                        :
                            (((bool) (((bool) ((guid257).id() == ((uint8_t) 2))) && true)) ? 
                                (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                                    NeoPixel::color c2 = ((guid257).alternate()).e2;
                                    NeoPixel::color c1 = ((guid257).alternate()).e1;
                                    return (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t> {
                                        (([&]() -> juniper::unit {
                                             Serial.println("Alternate"); 
                                            return {};
                                        })());
                                        return ListExt::replicateList<NeoPixel::color, c260, 2>(cast<int32_t, uint32_t>(nPixels), (([&]() -> juniper::records::recordt_0<juniper::array<NeoPixel::color, 2>, uint32_t>{
                                            juniper::records::recordt_0<juniper::array<NeoPixel::color, 2>, uint32_t> guid258;
                                            guid258.data = (juniper::array<NeoPixel::color, 2> { {c1, c2} });
                                            guid258.length = ((uint32_t) 2);
                                            return guid258;
                                        })()));
                                    })());
                                })())
                            :
                                juniper::quit<juniper::records::recordt_0<juniper::array<NeoPixel::color, c260>, uint32_t>>())));
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c265>
    juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c265>, uint32_t> diffPixels(juniper::records::recordt_0<juniper::array<NeoPixel::color, c265>, uint32_t> current, juniper::records::recordt_0<juniper::array<NeoPixel::color, c265>, uint32_t> next) {
        return (([&]() -> juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c265>, uint32_t> {
            constexpr int32_t nPixels = c265;
            return (([&]() -> juniper::records::recordt_0<juniper::array<Prelude::maybe<NeoPixel::color>, c265>, uint32_t> {
                return List::map<Prelude::maybe<NeoPixel::color>, void, juniper::tuple2<NeoPixel::color, NeoPixel::color>, c265>(juniper::function<void, Prelude::maybe<NeoPixel::color>(juniper::tuple2<NeoPixel::color, NeoPixel::color>)>([](juniper::tuple2<NeoPixel::color, NeoPixel::color> tup) -> Prelude::maybe<NeoPixel::color> { 
                    return (([&]() -> Prelude::maybe<NeoPixel::color> {
                        juniper::tuple2<NeoPixel::color, NeoPixel::color> guid259 = tup;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        NeoPixel::color next = (guid259).e2;
                        NeoPixel::color curr = (guid259).e1;
                        
                        return (eq<NeoPixel::color>(curr, next) ? 
                            nothing<NeoPixel::color>()
                        :
                            just<NeoPixel::color>(next));
                    })());
                 }), List::zip<NeoPixel::color, NeoPixel::color, c265>(current, next));
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit setPixelColor(uint16_t n, NeoPixel::color color, NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid260 = line;
            if (!(((bool) (((bool) ((guid260).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid260).device();
            
            NeoPixel::color guid261 = color;
            if (!(((bool) (((bool) ((guid261).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            uint8_t b = ((guid261).RGB()).e3;
            uint8_t g = ((guid261).RGB()).e2;
            uint8_t r = ((guid261).RGB()).e1;
            
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
            NeoPixel::RawDevice guid262 = line;
            if (!(((bool) (((bool) ((guid262).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid262).device();
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->show(); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c268>
    juniper::unit writePixels(juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>> line) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c268;
            return (([&]() -> juniper::unit {
                List::iter<juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>>>, juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>>, c268>(juniper::function<juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>>>, juniper::unit(juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>>)>(juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>>>(line), [](juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>>>& junclosure, juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>> tup) -> juniper::unit { 
                    juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c268>, uint32_t>>>& line = junclosure.line;
                    return (([&]() -> juniper::unit {
                        juniper::tuple2<uint32_t, Prelude::maybe<NeoPixel::color>> guid263 = tup;
                        if (!(true)) {
                            juniper::quit<juniper::unit>();
                        }
                        Prelude::maybe<NeoPixel::color> color = (guid263).e2;
                        uint32_t index = (guid263).e1;
                        
                        return (([&]() -> juniper::unit {
                            Prelude::maybe<NeoPixel::color> guid264 = color;
                            return (((bool) (((bool) ((guid264).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> juniper::unit {
                                    NeoPixel::color pixel = (guid264).just();
                                    return (([&]() -> juniper::unit {
                                        Prelude::maybe<NeoPixel::RawDevice> guid265 = ((line).get())->device;
                                        return (((bool) (((bool) ((guid265).id() == ((uint8_t) 0))) && true)) ? 
                                            (([&]() -> juniper::unit {
                                                NeoPixel::RawDevice device = (guid265).just();
                                                return setPixelColor(cast<uint32_t, uint16_t>(index), pixel, device);
                                            })())
                                        :
                                            (((bool) (((bool) ((guid265).id() == ((uint8_t) 1))) && true)) ? 
                                                (([&]() -> juniper::unit {
                                                    return (([&]() -> juniper::unit {
                                                         Serial.println("setPixelColor"); 
                                                        return {};
                                                    })());
                                                })())
                                            :
                                                juniper::quit<juniper::unit>()));
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid264).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> juniper::unit {
                                        return juniper::unit();
                                    })())
                                :
                                    juniper::quit<juniper::unit>()));
                        })());
                    })());
                 }), ListExt::enumerated<Prelude::maybe<NeoPixel::color>, c268>(diffPixels<c268>(((line).get())->previousPixels, ((line).get())->pixels)));
                return (([&]() -> juniper::unit {
                    Prelude::maybe<NeoPixel::RawDevice> guid266 = ((line).get())->device;
                    return (((bool) (((bool) ((guid266).id() == ((uint8_t) 0))) && true)) ? 
                        (([&]() -> juniper::unit {
                            NeoPixel::RawDevice device = (guid266).just();
                            return show(device);
                        })())
                    :
                        (((bool) (((bool) ((guid266).id() == ((uint8_t) 1))) && true)) ? 
                            (([&]() -> juniper::unit {
                                return (([&]() -> juniper::unit {
                                     Serial.println("show"); 
                                    return {};
                                })());
                            })())
                        :
                            juniper::quit<juniper::unit>()));
                })());
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c272>
    juniper::unit updateLine(juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c272>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c272>, uint32_t>>> line, NeoPixel::Function fn) {
        return (([&]() -> juniper::unit {
            constexpr int32_t n = c272;
            return (([&]() -> juniper::unit {
                (((line).get())->previousPixels = ((line).get())->pixels);
                (((line).get())->pixels = applyFunction<c272>(fn, ((line).get())->previousPixels));
                return writePixels<c272>(line);
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit begin(NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid267 = line;
            if (!(((bool) (((bool) ((guid267).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid267).device();
            
            return (([&]() -> juniper::unit {
                 ((Adafruit_NeoPixel*) p)->begin(); 
                return {};
            })());
        })());
    }
}

namespace NeoPixel {
    template<int c275, int c276>
    juniper::unit update(Prelude::maybe<NeoPixel::Action> act, juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>> model) {
        return (([&]() -> juniper::unit {
            constexpr int32_t nLines = c275;
            constexpr int32_t nPixels = c276;
            return (([&]() -> juniper::unit {
                (([&]() -> juniper::unit {
                     Serial.println("update"); 
                    return {};
                })());
                return Signal::sink<NeoPixel::Update, juniper::closures::closuret_10<juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>>>>(juniper::function<juniper::closures::closuret_10<juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>>>, juniper::unit(NeoPixel::Update)>(juniper::closures::closuret_10<juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>>>(model), [](juniper::closures::closuret_10<juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>>>& junclosure, NeoPixel::Update update) -> juniper::unit { 
                    juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>, uint32_t>>>& model = junclosure.model;
                    return (([&]() -> juniper::unit {
                        return (([&]() -> juniper::unit {
                            NeoPixel::Update guid268 = update;
                            return (((bool) (((bool) ((guid268).id() == ((uint8_t) 0))) && true)) ? 
                                (([&]() -> juniper::unit {
                                    NeoPixel::Action action = (guid268).action();
                                    return (([&]() -> juniper::unit {
                                        (([&]() -> juniper::unit {
                                             Serial.println("action"); 
                                            return {};
                                        })());
                                        return (([&]() -> juniper::unit {
                                            NeoPixel::Action guid269 = action;
                                            return (((bool) (((bool) ((guid269).id() == ((uint8_t) 0))) && true)) ? 
                                                (([&]() -> juniper::unit {
                                                    return (([&]() -> juniper::unit {
                                                        (([&]() -> juniper::unit {
                                                             Serial.println("start"); 
                                                            return {};
                                                        })());
                                                        return List::iter<void, juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>(juniper::function<void, juniper::unit(juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>)>([](juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>> line) -> juniper::unit { 
                                                            return (([&]() -> juniper::unit {
                                                                return (([&]() -> juniper::unit {
                                                                    Prelude::maybe<NeoPixel::RawDevice> guid270 = ((line).get())->device;
                                                                    return (((bool) (((bool) ((guid270).id() == ((uint8_t) 0))) && true)) ? 
                                                                        (([&]() -> juniper::unit {
                                                                            NeoPixel::RawDevice device = (guid270).just();
                                                                            return begin(device);
                                                                        })())
                                                                    :
                                                                        (((bool) (((bool) ((guid270).id() == ((uint8_t) 1))) && true)) ? 
                                                                            (([&]() -> juniper::unit {
                                                                                return (([&]() -> juniper::unit {
                                                                                     Serial.println("begin"); 
                                                                                    return {};
                                                                                })());
                                                                            })())
                                                                        :
                                                                            juniper::quit<juniper::unit>()));
                                                                })());
                                                            })());
                                                         }), ((model).get())->lines);
                                                    })());
                                                })())
                                            :
                                                (((bool) (((bool) ((guid269).id() == ((uint8_t) 1))) && true)) ? 
                                                    (([&]() -> juniper::unit {
                                                        NeoPixel::Function fn = ((guid269).run()).e2;
                                                        uint8_t line = ((guid269).run()).e1;
                                                        return (([&]() -> juniper::unit {
                                                            (([&]() -> juniper::unit {
                                                                 Serial.println("run"); 
                                                                return {};
                                                            })());
                                                            return updateLine<c276>(((((model).get())->lines).data)[line], fn);
                                                        })());
                                                    })())
                                                :
                                                    (((bool) (((bool) ((guid269).id() == ((uint8_t) 2))) && true)) ? 
                                                        (([&]() -> juniper::unit {
                                                            Prelude::maybe<uint8_t> endAfter = ((guid269).repeat()).e4;
                                                            uint32_t interval = ((guid269).repeat()).e3;
                                                            NeoPixel::Function fn = ((guid269).repeat()).e2;
                                                            uint8_t line = ((guid269).repeat()).e1;
                                                            return (([&]() -> juniper::unit {
                                                                (([&]() -> juniper::unit {
                                                                     Serial.println("repeat"); 
                                                                    return {};
                                                                })());
                                                                updateLine<c276>(((((model).get())->lines).data)[line], fn);
                                                                (((((((model).get())->lines).data)[line]).get())->operation = just<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>((([&]() -> juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>{
                                                                    juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>> guid271;
                                                                    guid271.function = fn;
                                                                    guid271.interval = interval;
                                                                    guid271.timer = Time::state;
                                                                    guid271.endAfter = endAfter;
                                                                    return guid271;
                                                                })())));
                                                                return juniper::unit();
                                                            })());
                                                        })())
                                                    :
                                                        (((bool) (((bool) ((guid269).id() == ((uint8_t) 3))) && true)) ? 
                                                            (([&]() -> juniper::unit {
                                                                uint8_t line = (guid269).endRepeat();
                                                                return (([&]() -> juniper::unit {
                                                                    (([&]() -> juniper::unit {
                                                                         Serial.println("endRepeat"); 
                                                                        return {};
                                                                    })());
                                                                    (((((((model).get())->lines).data)[line]).get())->operation = nothing<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>());
                                                                    return juniper::unit();
                                                                })());
                                                            })())
                                                        :
                                                            juniper::quit<juniper::unit>()))));
                                        })());
                                    })());
                                })())
                            :
                                (((bool) (((bool) ((guid268).id() == ((uint8_t) 1))) && true)) ? 
                                    (([&]() -> juniper::unit {
                                        return (([&]() -> juniper::unit {
                                            (([&]() -> juniper::unit {
                                                 Serial.println("operation"); 
                                                return {};
                                            })());
                                            return List::iter<void, juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>, c275>(juniper::function<void, juniper::unit(juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>)>([](juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>> line) -> juniper::unit { 
                                                return (([&]() -> juniper::unit {
                                                    (((line).get())->operation = runOperation<juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>>>(juniper::function<juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>>, juniper::unit(NeoPixel::Function)>(juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>>(line), [](juniper::closures::closuret_9<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>>& junclosure, NeoPixel::Function function) -> juniper::unit { 
                                                        juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, c276>, uint32_t>>>& line = junclosure.line;
                                                        return (([&]() -> juniper::unit {
                                                            return updateLine<c276>(line, function);
                                                        })());
                                                     }), updateOperation(((line).get())->operation)));
                                                    return juniper::unit();
                                                })());
                                             }), ((model).get())->lines);
                                        })());
                                    })())
                                :
                                    juniper::quit<juniper::unit>()));
                        })());
                    })());
                 }), Signal::mergeMany<NeoPixel::Update, 2>((([&]() -> juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Update>, 2>, uint32_t>{
                    juniper::records::recordt_0<juniper::array<Prelude::sig<NeoPixel::Update>, 2>, uint32_t> guid272;
                    guid272.data = (juniper::array<Prelude::sig<NeoPixel::Update>, 2> { {Signal::map<NeoPixel::Action, void, NeoPixel::Update>(juniper::function<void, NeoPixel::Update(NeoPixel::Action)>(action), signal<NeoPixel::Action>(act)), Signal::constant<NeoPixel::Update>(operation())} });
                    guid272.length = ((uint32_t) 2);
                    return guid272;
                })())));
            })());
        })());
    }
}

namespace NeoPixel {
    juniper::unit setBrightness(uint8_t level, NeoPixel::RawDevice line) {
        return (([&]() -> juniper::unit {
            NeoPixel::RawDevice guid273 = line;
            if (!(((bool) (((bool) ((guid273).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid273).device();
            
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
            uint8_t guid274 = ((uint8_t) 0);
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            uint8_t ret = guid274;
            
            NeoPixel::RawDevice guid275 = line;
            if (!(((bool) (((bool) ((guid275).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid275).device();
            
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
            NeoPixel::RawDevice guid276 = line;
            if (!(((bool) (((bool) ((guid276).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid276).device();
            
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
            bool guid277 = false;
            if (!(true)) {
                juniper::quit<juniper::unit>();
            }
            bool ret = guid277;
            
            NeoPixel::RawDevice guid278 = line;
            if (!(((bool) (((bool) ((guid278).id() == ((uint8_t) 0))) && true)))) {
                juniper::quit<juniper::unit>();
            }
            void * p = (guid278).device();
            
            (([&]() -> juniper::unit {
                 ret = ((Adafruit_NeoPixel*) p)->canShow(); 
                return {};
            })());
            return ret;
        })());
    }
}

namespace TEA {
    juniper::refcell<juniper::records::recordt_10<juniper::records::recordt_0<juniper::array<juniper::refcell<juniper::records::recordt_9<Prelude::maybe<NeoPixel::RawDevice>, Prelude::maybe<juniper::records::recordt_8<Prelude::maybe<uint8_t>, NeoPixel::Function, uint32_t, juniper::records::recordt_1<uint32_t>>>, uint16_t, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>, juniper::records::recordt_0<juniper::array<NeoPixel::color, 150>, uint32_t>>>, 1>, uint32_t>>> state = initialState<150, 1>((juniper::array<juniper::records::recordt_7<uint16_t>, 1> { {(([]() -> juniper::records::recordt_7<uint16_t>{
        juniper::records::recordt_7<uint16_t> guid279;
        guid279.pin = ((uint16_t) 7);
        return guid279;
    })())} }), ((uint16_t) 150));
}

namespace TEA {
    Prelude::maybe<NeoPixel::Action> previousAction = just<NeoPixel::Action>(repeat(((uint8_t) 0), rotate(((int16_t) 1)), ((uint32_t) 3000), nothing<uint8_t>()));
}

namespace TEA {
    juniper::unit setup() {
        return (([&]() -> juniper::unit {
            (([&]() -> juniper::unit {
                 Serial.begin(115200); 
                return {};
            })());
            (([&]() -> juniper::unit {
                 while (!Serial) {} 
                return {};
            })());
            (([&]() -> juniper::unit {
                 Serial.println("Setup complete"); 
                return {};
            })());
            return juniper::unit();
        })());
    }
}

namespace TEA {
    juniper::unit loop() {
        return (([&]() -> juniper::unit {
            (([&]() -> juniper::unit {
                 delay(3000); 
                return {};
            })());
            (([&]() -> juniper::unit {
                 Serial.println("_____"); 
                return {};
            })());
            return Signal::sink<Prelude::maybe<NeoPixel::Action>, void>(juniper::function<void, juniper::unit(Prelude::maybe<NeoPixel::Action>)>([](Prelude::maybe<NeoPixel::Action> action) -> juniper::unit { 
                return (([&]() -> juniper::unit {
                    return update<1, 150>(action, state);
                })());
             }), actions(previousAction));
        })());
    }
}

void setup() {
    TEA::setup();
}
void loop() {
    TEA::loop();
}
