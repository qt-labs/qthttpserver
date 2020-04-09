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

namespace QtPrivate {

template<typename T>
struct RemoveCVRef
{
    using Type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};


template<bool classMember, typename ReturnT, typename ... Args>
struct FunctionTraitsHelper
{
    static constexpr const int ArgumentCount = sizeof ... (Args);
    static constexpr const int ArgumentIndexMax = ArgumentCount - 1;
    static constexpr const bool IsClassMember = classMember;
    using ReturnType = ReturnT;

    template <int I>
    struct Arg {
        using Type = typename std::tuple_element<I, std::tuple<Args...>>::type;

        using CleanType = typename QtPrivate::RemoveCVRef<Type>::Type;

        static constexpr bool Defined = QMetaTypeId2<CleanType>::Defined;
    };
};

template<bool classMember, typename ReturnT>
struct FunctionTraitsHelper<classMember, ReturnT>
{
    static constexpr const int ArgumentCount = 0;
    static constexpr const int ArgumentIndexMax = -1;
    static constexpr const bool IsClassMember = classMember;
    using ReturnType = ReturnT;

    template <int I>
    struct Arg {
        using Type = std::false_type;
        using CleanType = Type;
        static constexpr bool Defined = QMetaTypeId2<CleanType>::Defined;
    };
};

template<typename T>
struct FunctionTraits;

template<typename T>
struct FunctionTraits : public FunctionTraits<decltype(&T::operator())>{};

template<typename ReturnT, typename ... Args>
struct FunctionTraits<ReturnT (*)(Args...)>
    : public FunctionTraitsHelper<false, ReturnT, Args...>
{
};

template<class ReturnT, class ClassT, class ...Args>
struct FunctionTraits<ReturnT (ClassT::*)(Args...) const>
    : public FunctionTraitsHelper<true, ReturnT, Args...>
{
    using classType = ClassT;
};

template<typename ViewHandler, bool DisableStaticAssert>
struct ViewTraitsHelper {
    using FunctionTraits = typename QtPrivate::FunctionTraits<ViewHandler>;
    using ArgumentIndexes = typename QtPrivate::Indexes<FunctionTraits::ArgumentCount>::Value;

    struct StaticMath {
        template <template<typename> class Predicate, bool defaultValue>
        struct Loop {
            static constexpr bool eval() noexcept {
                return defaultValue;
            }

            template<typename T, typename ... N>
            static constexpr T eval(const T it, N ...n) noexcept {
                return Predicate<T>::eval(it, eval(n...));
            }
        };

        template<typename T>
        struct SumPredicate {
            static constexpr T eval(const T rs, const T ls) noexcept
            {
                return rs + ls;
            }
        };

        template<typename T>
        struct AndPredicate {
            static constexpr T eval(const T rs, const T ls) noexcept
            {
                return rs && ls;
            }
        };

        using Sum = Loop<SumPredicate, false>;
        using And = Loop<AndPredicate, true>;
        using Or = Sum;
    };

    struct Arguments {
        template<int I>
        struct StaticCheck {
            using Arg = typename FunctionTraits::template Arg<I>;
            using CleanType = typename Arg::CleanType;

            template<typename T, bool Clean = false>
            static constexpr bool isType() noexcept
            {
                using SelectedType =
                    typename std::conditional<
                        Clean,
                        CleanType,
                        typename Arg::Type
                    >::type;
                return std::is_same<SelectedType, T>::value;
            }

            template<typename T>
            struct SpecialHelper {
                using CleanTypeT = typename QtPrivate::RemoveCVRef<T>::Type;

                static constexpr bool TypeMatched = isType<CleanTypeT, true>();
                static constexpr bool TypeCVRefMatched = isType<T>();

                static constexpr bool ValidPosition =
                    (I == FunctionTraits::ArgumentIndexMax ||
                     I == FunctionTraits::ArgumentIndexMax - 1);
                static constexpr bool ValidAll = TypeCVRefMatched && ValidPosition;

                static constexpr bool assertCondition =
                    DisableStaticAssert || !TypeMatched || TypeCVRefMatched;

                static constexpr bool assertConditionOrder =
                    DisableStaticAssert || !TypeMatched || ValidPosition;

                static constexpr bool staticAssert() noexcept
                {
                    static_assert(assertConditionOrder,
                                  "ViewHandler arguments error: "
                                  "QHttpServerRequest or QHttpServerResponder"
                                  " can only be the last argument");
                    return true;
                }
            };

            template<typename ... T>
            struct CheckAny {
                static constexpr bool Value = StaticMath::Or::eval(T::Value...);
                static constexpr bool Valid = StaticMath::Or::eval(T::Valid...);
                static constexpr bool staticAssert() noexcept
                {
                    return StaticMath::Or::eval(T::staticAssert()...);
                }
            };

            struct IsRequest {
                using Helper = SpecialHelper<const QHttpServerRequest &>;
                static constexpr bool Value = Helper::TypeMatched;
                static constexpr bool Valid = Helper::ValidAll;

                static constexpr bool staticAssert() noexcept
                {
                    static_assert(Helper::assertCondition,
                                  "ViewHandler arguments error: "
                                  "QHttpServerRequest can only be passed as a const reference");
                    return Helper::staticAssert();
                }
            };

            struct IsResponder {
                using Helper = SpecialHelper<QHttpServerResponder &&>;
                static constexpr bool Value = Helper::TypeMatched;
                static constexpr bool Valid = Helper::ValidAll;

                static constexpr bool staticAssert() noexcept
                {
                    static_assert(Helper::assertCondition,
                                  "ViewHandler arguments error: "
                                  "QHttpServerResponder can only be passed as a universal reference");
                    return Helper::staticAssert();
                }
            };

            using IsSpecial = CheckAny<IsRequest, IsResponder>;

            struct IsSimple {
                static constexpr bool Value = !IsSpecial::Value &&
                                               I < FunctionTraits::ArgumentCount &&
                                               FunctionTraits::ArgumentIndexMax != -1;
                static constexpr bool Valid = Arg::Defined;

                static constexpr bool assertCondition =
                    DisableStaticAssert || !Value || Valid;

                static constexpr bool staticAssert() noexcept
                {
                    static_assert(assertCondition,
                                  "ViewHandler arguments error: "
                                  "Type is not registered, please use the Q_DECLARE_METATYPE macro "
                                  "to make it known to Qt's meta-object system");
                    return true;
                }
            };

            using CheckOk = CheckAny<IsSimple, IsSpecial>;

            static constexpr bool Valid = CheckOk::Valid;
            static constexpr bool StaticAssert = CheckOk::staticAssert();
        };

        template<int ... I>
        struct ArgumentsReturn {
            template<int Idx>
            using Arg = StaticCheck<Idx>;

            template<int Idx>
            static constexpr int metaTypeId() noexcept
            {
                using Type = typename FunctionTraits::template Arg<Idx>::CleanType;

                return qMetaTypeId<
                    typename std::conditional<
                        QMetaTypeId2<Type>::Defined,
                        Type,
                        void>::type>();
            }

            static constexpr std::size_t Count = FunctionTraits::ArgumentCount;
            static constexpr std::size_t CapturableCount =
                StaticMath::Sum::eval(
                    static_cast<std::size_t>(FunctionTraits::template Arg<I>::Defined)...);
            static constexpr std::size_t PlaceholdersCount = Count - CapturableCount;

            static constexpr bool Valid = StaticMath::And::eval(StaticCheck<I>::Valid...);
            static constexpr bool StaticAssert =
                StaticMath::And::eval(StaticCheck<I>::StaticAssert...);

            using Indexes = typename QtPrivate::IndexesList<I...>;

            using CapturableIndexes =
                typename QtPrivate::Indexes<CapturableCount>::Value;

            using PlaceholdersIndexes =
                typename QtPrivate::Indexes<PlaceholdersCount>::Value;

            using Last = Arg<FunctionTraits::ArgumentIndexMax>;
        };

        template<int ... I>
        static constexpr ArgumentsReturn<I...> eval(QtPrivate::IndexesList<I...>) noexcept
        {
            return ArgumentsReturn<I...>{};
        }
    };

    template<int CaptureOffset>
    struct BindType {
        template<typename ... Args>
        struct FunctionWrapper {
            using Type = std::function<typename FunctionTraits::ReturnType (Args...)>;
        };

        template<int Id>
        using OffsetArg = typename FunctionTraits::template Arg<CaptureOffset + Id>::Type;

        template<int ... Idx>
        static constexpr typename FunctionWrapper<OffsetArg<Idx>...>::Type
                eval(QtPrivate::IndexesList<Idx...>) noexcept;
    };
};

} // namespace QtPrivate

template <typename ViewHandler, bool DisableStaticAssert = false>
struct QHttpServerRouterViewTraits
{
    using Helpers = typename QtPrivate::ViewTraitsHelper<ViewHandler, DisableStaticAssert>;
    using Arguments = decltype(Helpers::Arguments::eval(typename Helpers::ArgumentIndexes{}));
    using BindableType = decltype(
            Helpers::template BindType<Arguments::CapturableCount>::eval(
                typename Arguments::PlaceholdersIndexes{}));
};

QT_END_NAMESPACE

#endif  // QHTTPSERVERROUTERVIEWTRAITS_H
