#include "controllers/plugins/api/Ui.hpp"

#include "controllers/plugins/SolTypes.hpp"

#include <sol/sol.hpp>

using namespace std::string_view_literals;

namespace {

template <typename Ret, typename... Args>
struct FnTraits {
    using Return = Ret;
    using Arguments = std::tuple<Args...>;

    template <typename T>
    FnTraits(T fn){};
};

template <typename Ret, typename T, typename... Args>
FnTraits(Ret (T::*)(Args...)) -> FnTraits<Ret, Args...>;

template <typename T>
using FnTraitsT = decltype(FnTraits(std::declval<T>()));

template <typename Fn>
void applyProp(auto *widget, const sol::table &tbl, auto &&key, Fn fn)
{
    using ArgType = std::remove_cvref_t<
        std::tuple_element_t<0, typename FnTraitsT<Fn>::Arguments>>;

    auto opt =
        tbl.get<sol::optional<ArgType>>(std::forward<decltype(key)>(key));
    if (opt)
    {
        (widget->*fn)(*opt);
    }
}

void applyProps(auto *widget, const sol::table &tbl, auto &&...args)
{
    static_assert(sizeof...(args) % 2 == 0);

    ([]<size_t... I, typename... Args>(std::index_sequence<I...> /* seq */,
                                       auto *widget, const sol::table &tbl,
                                       std::tuple<Args...> &&args) {
        (applyProp(widget, tbl, std::get<I * 2>(std::move(args)),
                   std::get<(I * 2) + 1>(std::move(args))),
         ...);
    })(std::make_index_sequence<sizeof...(args) / 2>(), widget, tbl,
       std::forward_as_tuple(std::forward<decltype(args)>(args)...));
}

void applyWidgetProps(QWidget *widget, const sol::table &args)
{
    applyProps(
        widget, args,                              //
        "enabled"sv, &QWidget::setEnabled,         //
        "style_sheet"sv, &QWidget::setStyleSheet,  //
        "minimum_size"sv, qOverload<const QSize &>(&QWidget::setMinimumSize),
        "maximum_size"sv, qOverload<const QSize &>(&QWidget::setMaximumSize),
        "tooltip"sv, &QWidget::setToolTip,               //
        "visible"sv, &QWidget::setVisible,               //
        "window_flags"sv, &QWidget::setWindowFlags,      //
        "window_opacity"sv, &QWidget::setWindowOpacity,  //
        "window_title"sv, &QWidget::setWindowTitle,      //
        "layout"sv, &QWidget::setLayout,                 //
        "contents_margins"sv,
        qOverload<const QMargins &>(&QWidget::setContentsMargins));
}

void applySpecificProps(QFrame *frame, const sol::table &args)
{
    applyProps(frame, args,                                //
               "frame_rect"sv, &QFrame::setFrameRect,      //
               "frame_shadow"sv, &QFrame::setFrameShadow,  //
               "frame_shape", &QFrame::setFrameShape,      //
               "line_width"sv, &QFrame::setLineWidth,      //
               "mid_line_width"sv, &QFrame::setMidLineWidth);
}

void applySpecificProps(QLabel *label, const sol::table &args)
{
    applySpecificProps(static_cast<QFrame *>(label), args);
    applyProps(label, args,                                                   //
               "alignment"sv, &QLabel::setAlignment,                          //
               "indent"sv, &QLabel::setIndent,                                //
               "margin"sv, &QLabel::setMargin,                                //
               "open_external_links"sv, &QLabel::setOpenExternalLinks,        //
               "scaled_contents"sv, &QLabel::setScaledContents,               //
               "text"sv, &QLabel::setText,                                    //
               "text_format"sv, &QLabel::setTextFormat,                       //
               "text_interaction_flags"sv, &QLabel::setTextInteractionFlags,  //
               "word_wrap"sv, &QLabel::setWordWrap,                           //
               "buddy"sv, &QLabel::setBuddy                                   //
    );
}

template <typename T>
OwnedQPointer<T> makeWidget(const sol::table &args, QObject *guard)
{
    T *widget = new T;
    applyWidgetProps(widget, args);
    if constexpr (requires { applySpecificProps(widget, args); })
    {
        applySpecificProps(widget, args);
    }

    // signals
    auto oDestroyed =
        args.get<sol::optional<sol::main_function>>("on_destroyed"sv);
    if (oDestroyed)
    {
        QObject::connect(widget, &QObject::destroyed, guard, [cb{*oDestroyed}] {
            cb();
        });
    }

    return {widget};
}

void applyLayoutProps(QLayout *layout, const sol::table &args)
{
    applyProps(layout, args,                                      //
               "enabled"sv, &QLayout::setEnabled,                 //
               "spacing"sv, &QLayout::setSpacing,                 //
               "size_constraint"sv, &QLayout::setSizeConstraint,  //
               "contents_margins"sv,
               qOverload<const QMargins &>(&QLayout::setContentsMargins));
}

void addLayoutChildren(QBoxLayout *layout, const sol::table &items)
{
    for (size_t i = 1; i <= items.size(); i++)
    {
        const auto &child = items[i];

        if (child.is<QLayout *>())
        {
            if (auto *inner = child.get<QLayout *>())
            {
                layout->addLayout(inner);
            }
            continue;
        }
        if (child.is<QWidget *>())
        {
            if (auto *inner = child.get<QWidget *>())
            {
                layout->addWidget(inner);
            }
            continue;
        }
        if (!child.is<sol::table>())
        {
            // XXX: warn/error?
            continue;
        }

        auto tbl = child.get<sol::table>();

        auto oWidget = tbl.get<sol::optional<QWidget *>>("widget"sv);
        if (oWidget)
        {
            if (!*oWidget)
            {
                // XXX: warn/error?
                continue;
            }
            auto stretch = tbl.get_or("stretch"sv, 0);
            auto alignment = tbl.get_or("alignment"sv, 0);
            layout->addWidget(*oWidget, stretch, {alignment});
            continue;
        }

        auto oLayout = tbl.get<sol::optional<QLayout *>>("layout"sv);
        if (oLayout)
        {
            if (!*oLayout)
            {
                // XXX: warn/error?
                continue;
            }
            auto stretch = tbl.get_or("stretch"sv, 0);
            layout->addLayout(*oLayout, stretch);
            continue;
        }

        auto oSpacing = tbl.get<sol::optional<int>>("spacing"sv);
        if (oSpacing)
        {
            layout->addSpacing(*oSpacing);
            continue;
        }

        auto oStretch = tbl.get<sol::optional<int>>("stretch"sv);
        if (oStretch)
        {
            layout->addStretch(*oStretch);
            continue;
        }

        auto oStrut = tbl.get<sol::optional<int>>("strut"sv);
        if (oStrut)
        {
            layout->addStrut(*oStrut);
            continue;
        }

        // XXX: warn/error
    }
}

template <typename T>
OwnedQPointer<T> makeLayout(const sol::table &args)
{
    T *layout = new T;
    applyLayoutProps(layout, args);
    if constexpr (requires { addLayoutChildren(layout, args); })
    {
        auto oChildren = args.get<sol::optional<sol::table>>("children"sv);
        if (oChildren)
        {
            addLayoutChildren(layout, *oChildren);
        }
    }

    return {layout};
}

void registerQWidget(sol::table &tbl, QObject *guard)
{
    tbl.new_usertype<QWidget>("QWidget",
                              sol::factories([guard](const sol::table &args) {
                                  return makeWidget<QWidget>(args, guard);
                              }),
                              "close", &QWidget::close,  //
                              "hide", &QWidget::hide,    //
                              "lower", &QWidget::lower,  //
                              "raise", &QWidget::raise,  //
                              "show", &QWidget::show,    //

                              sol::base_classes, sol::bases<QObject>());
}

void registerLabel(sol::table &tbl, QObject *guard)
{
    tbl.new_usertype<QLabel>("QLabel",
                             sol::factories([guard](const sol::table &args) {
                                 return makeWidget<QLabel>(args, guard);
                             }),
                             sol::base_classes, sol::bases<QWidget, QObject>());
}

void registerLayouts(sol::table &tbl)
{
    tbl.new_usertype<QLayout>("QLayout", sol::no_constructor, sol::base_classes,
                              sol::bases<QObject>());
    tbl.new_usertype<QVBoxLayout>(
        "QVBoxLayout", sol::factories(makeLayout<QVBoxLayout>),
        sol::base_classes, sol::bases<QObject, QLayout>());
    tbl.new_usertype<QHBoxLayout>(
        "QHBoxLayout", sol::factories(makeLayout<QHBoxLayout>),
        sol::base_classes, sol::bases<QObject, QLayout>());
}

}  // namespace

namespace chatterino::lua::api::ui {

void registerTypes(sol::table &c2, QObject *guard)
{
    registerQWidget(c2, guard);
    registerLabel(c2, guard);
    registerLayouts(c2);
}

}  // namespace chatterino::lua::api::ui
