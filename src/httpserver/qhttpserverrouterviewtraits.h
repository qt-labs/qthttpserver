/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtHttpServer module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QHTTPSERVERROUTERVIEWTRAITS_H
#define QHTTPSERVERROUTERVIEWTRAITS_H

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobjectdefs.h>

#include <functional>
#include <tuple>
#include <type_traits>

QT_BEGIN_NAMESPACE

class QHttpServerRequest;
class QHttpServerResponder;

template <typename ViewHandler>
struct QHttpServerRouterViewTraits : QHttpServerRouterViewTraits<decltype(&ViewHandler::operator())> {};

template <typename ClassType, typename Ret, typename ... Args>
struct QHttpServerRouterViewTraits<Ret(ClassType::*)(Args...) const>
{
    static constexpr const int ArgumentCount = sizeof ... (Args);

    template <int I>
    struct Arg {
        using OriginalType = typename std::tuple_element<I, std::tuple<Args...>>::type;
        using Type = typename QtPrivate::RemoveConstRef<OriginalType>::Type;

        static constexpr int metaTypeId() noexcept {
            return qMetaTypeId<
                typename std::conditional<
                    !QMetaTypeId2<Type>::Defined,
                    void,
                    Type>::type>();
        }
    };

private:
    // Tools used to check position of special arguments (QHttpServerResponder, QHttpServerRequest)
    // and unsupported types
    template<bool Last, typename Arg>
    static constexpr bool checkArgument() noexcept
    {
            static_assert(Last || !std::is_same<Arg, const QHttpServerRequest &>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerRequest can only be the last argument");
            static_assert(Last || !std::is_same<Arg, QHttpServerResponder &&>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerResponder can only be the last argument");

            static_assert(!std::is_same<Arg, QHttpServerRequest &&>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerRequest can only be passed as a const reference");
            static_assert(!std::is_same<Arg, QHttpServerRequest &>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerRequest can only be passed as a const reference");
            static_assert(!std::is_same<Arg, const QHttpServerRequest *>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerRequest can only be passed as a const reference");
            static_assert(!std::is_same<Arg, QHttpServerRequest const*>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerRequest can only be passed as a const reference");
            static_assert(!std::is_same<Arg, QHttpServerRequest *>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerRequest can only be passed as a const reference");

            static_assert(!std::is_same<Arg, QHttpServerResponder &>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerResponder can only be passed as a universal reference");
            static_assert(!std::is_same<Arg, const QHttpServerResponder *const>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerResponder can only be passed as a universal reference");
            static_assert(!std::is_same<Arg, const QHttpServerResponder *>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerResponder can only be passed as a universal reference");
            static_assert(!std::is_same<Arg, QHttpServerResponder const*>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerResponder can only be passed as a universal reference");
            static_assert(!std::is_same<Arg, QHttpServerResponder *>::value,
                          "ViewHandler arguments error: "
                          "QHttpServerResponder can only be passed as a universal reference");

            using Type = typename std::remove_cv<typename std::remove_reference<Arg>::type>::type;

            static_assert(QMetaTypeId2<Type>::Defined
                          || std::is_same<Type, QHttpServerResponder>::value
                          || std::is_same<Type, QHttpServerRequest>::value,
                          "ViewHandler arguments error: "
                          "Type is not registered, please use the Q_DECLARE_METATYPE macro "
                          "to make it known to Qt's meta-object system");

            return true;
    }

public:
    template<typename Arg, typename ... ArgX>
    struct ArgumentsCheck {
        static constexpr bool compileCheck()
        {
            return checkArgument<false, Arg>() && ArgumentsCheck<ArgX...>::compileCheck();
        }
    };

    template<typename Arg>
    struct ArgumentsCheck<Arg> {
        static constexpr bool compileCheck()
        {
            return checkArgument<true, Arg>();
        }
    };

    using Arguments = ArgumentsCheck<Args...>;

private:
    // Tools used to compute ArgumentCapturableCount
    template<typename T>
    static constexpr typename std::enable_if<QMetaTypeId2<T>::Defined, int>::type
            capturable()
    { return 1; }

    template<typename T>
    static constexpr typename std::enable_if<!QMetaTypeId2<T>::Defined, int>::type
            capturable()
    { return 0; }

    static constexpr std::size_t sum() noexcept { return 0; }

    template<typename ... N>
    static constexpr std::size_t sum(const std::size_t it, N ... n) noexcept
    { return it + sum(n...); }

public:
    static constexpr const std::size_t ArgumentCapturableCount =
        sum(capturable<typename QtPrivate::RemoveConstRef<Args>::Type>()...);
    static constexpr const std::size_t ArgumentPlaceholdersCount = ArgumentCount
            - ArgumentCapturableCount;

private:
    // Tools used to get BindableType
    template<typename Return, typename ... ArgsX>
    struct BindTypeHelper {
        using Type = std::function<Return(ArgsX...)>;
    };

    template<int ... Idx>
    static constexpr typename BindTypeHelper<
                Ret, typename Arg<ArgumentCapturableCount + Idx>::OriginalType...>::Type
            bindTypeHelper(QtPrivate::IndexesList<Idx...>)
    {
        return BindTypeHelper<Ret,
                              typename Arg<ArgumentCapturableCount + Idx>::OriginalType...>::Type();
    }

public:
    using BindableType = decltype(bindTypeHelper(typename QtPrivate::Indexes<
        ArgumentPlaceholdersCount>::Value{}));

    static constexpr bool IsLastArgRequest = std::is_same<
        typename Arg<ArgumentCount - 1>::Type, QHttpServerRequest>::value;

    static constexpr bool IsLastArgResponder = std::is_same<
        typename Arg<ArgumentCount - 1>::Type, QHttpServerResponder&&>::value;

    static constexpr bool IsLastArgNonSpecial = !(IsLastArgRequest || IsLastArgResponder);
};

template <typename ClassType, typename Ret>
struct QHttpServerRouterViewTraits<Ret(ClassType::*)() const>
{
    static constexpr const int ArgumentCount = 0;

    template <int I>
    struct Arg {
        using Type = void;
    };

    static constexpr const std::size_t ArgumentCapturableCount = 0u;
    static constexpr const std::size_t ArgumentPlaceholdersCount = 0u;

    using BindableType = decltype(std::function<Ret()>{});

    static constexpr bool IsLastArgRequest = false;
    static constexpr bool IsLastArgResponder = false;
    static constexpr bool IsLastArgNonSpecial = true;

    struct ArgumentsCheck {
        static constexpr bool compileCheck() { return true; }
    };

    using Arguments = ArgumentsCheck;
};

QT_END_NAMESPACE

#endif  // QHTTPSERVERROUTERVIEWTRAITS_H
