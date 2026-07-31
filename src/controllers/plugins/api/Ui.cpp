// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#ifdef CHATTERINO_HAVE_PLUGINS

#    include "controllers/plugins/api/Ui.hpp"

#    include "controllers/plugins/DialogGuard.hpp"
#    include "controllers/plugins/LuaUtilities.hpp"
#    include "controllers/plugins/Plugin.hpp"
#    include "controllers/plugins/SignalCallback.hpp"
#    include "controllers/plugins/SolTypes.hpp"
#    include "util/Helpers.hpp"

#    include <QInputDialog>
#    include <QMessageBox>

template <>
struct magic_enum::customize::enum_range<QMessageBox::StandardButton> {
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr bool is_flags = true;
};

namespace {

using namespace chatterino;
using namespace chatterino::lua;

Qt::TextFormat textFormatFromString(std::string_view sv)
{
    if (sv == "markdown")
    {
        return Qt::MarkdownText;
    }
    if (sv == "qt-html")
    {
        return Qt::RichText;
    }
    return Qt::PlainText;
}

QString getTitleForPlugin(const Plugin &plugin, QString title)
{
    if (title.isEmpty())
    {
        return plugin.meta.name;
    }

    title += u" (";
    title += plugin.meta.name;
    title += ')';
    return title;
}

void showMessageBox(ThisPluginState pl, const sol::table &init,
                    const sol::main_protected_function &cb)
{
    auto guard = pl.plugin()->openDialog();
    if (!guard)
    {
        throw std::runtime_error("Too many dialogs.");
    }

    auto *box = new QMessageBox;
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setWindowTitle(
        getTitleForPlugin(*pl.plugin(), init.get_or("title", QString{})));
    // Don't block input to other windows.
    box->setWindowModality(Qt::NonModal);

    box->setText(requiredGet<QString>(init, "text"));
    box->setStandardButtons(
        init.get_or("buttons", QMessageBox::StandardButton::Ok));
    box->setIcon(init.get_or("icon", QMessageBox::Icon::Information));
    box->setTextFormat(
        textFormatFromString(init.get_or("text_format", std::string_view{})));
    if (init.get_or("clickable_links", false))
    {
        box->setTextInteractionFlags(Qt::TextBrowserInteraction);
    }
    if (auto defaultBtn = init.get<std::optional<QMessageBox::StandardButton>>(
            "default_button"))
    {
        box->setDefaultButton(*defaultBtn);
    }
    if (auto escapeBtn = init.get<std::optional<QMessageBox::StandardButton>>(
            "escape_button"))
    {
        box->setDefaultButton(*escapeBtn);
    }
    QObject::connect(
        box, &QMessageBox::finished,
        [guard = *std::move(guard),
         cb = SignalCallback(pl.plugin()->weakRef(), cb)](int code) {
            cb(code);
        });
    box->show();
}

void applyCommonInputOptions(QInputDialog *diag, const Plugin &plugin,
                             const sol::table &init)
{
    diag->setWindowTitle(
        getTitleForPlugin(plugin, init.get_or("title", QString{})));
    diag->setLabelText(requiredGet<QString>(init, "label"));
    if (auto txt = init.get<std::optional<QString>>("ok_button_text"))
    {
        diag->setOkButtonText(*txt);
    }
    if (auto txt = init.get<std::optional<QString>>("cancel_button_text"))
    {
        diag->setCancelButtonText(*txt);
    }
}

void showNumberInput(ThisPluginState pl, const sol::table &init,
                     const sol::main_protected_function &cb)
{
    auto guard = pl.plugin()->openDialog();
    if (!guard)
    {
        throw std::runtime_error("Too many dialogs.");
    }

    auto *diag = new QInputDialog;
    diag->setAttribute(Qt::WA_DeleteOnClose);
    applyCommonInputOptions(diag, *pl.plugin(), init);
    int decimals = init.get_or("decimals", 0);
    if (decimals == 0)
    {
        diag->setInputMode(QInputDialog::IntInput);
        if (auto v = init.get<std::optional<int>>("min"))
        {
            diag->setIntMinimum(*v);
        }
        if (auto v = init.get<std::optional<int>>("max"))
        {
            diag->setIntMaximum(*v);
        }
        if (auto v = init.get<std::optional<int>>("init_value"))
        {
            diag->setIntValue(*v);
        }
        if (auto v = init.get<std::optional<int>>("step"))
        {
            diag->setIntStep(*v);
        }
    }
    else
    {
        diag->setInputMode(QInputDialog::DoubleInput);
        diag->setDoubleDecimals(decimals);
        if (auto v = init.get<std::optional<double>>("min"))
        {
            diag->setDoubleMinimum(*v);
        }
        if (auto v = init.get<std::optional<double>>("max"))
        {
            diag->setDoubleMaximum(*v);
        }
        if (auto v = init.get<std::optional<double>>("init_value"))
        {
            diag->setDoubleValue(*v);
        }
        if (auto v = init.get<std::optional<double>>("step"))
        {
            diag->setDoubleStep(*v);
        }
    }

    QObject::connect(
        diag, &QInputDialog::finished,
        [diag, guard = *std::move(guard),
         cb = SignalCallback(pl.plugin()->weakRef(), cb)](int ret) {
            if (diag->inputMode() == QInputDialog::IntInput)
            {
                cb(makeConditionedOptional(ret != 0, diag->intValue()));
            }
            else
            {
                cb(makeConditionedOptional(ret != 0, diag->doubleValue()));
            }
        });
    diag->show();
}

void showTextInput(ThisPluginState pl, const sol::table &init,
                   const sol::main_protected_function &cb)
{
    auto guard = pl.plugin()->openDialog();
    if (!guard)
    {
        throw std::runtime_error("Too many dialogs.");
    }

    auto *diag = new QInputDialog;
    diag->setAttribute(Qt::WA_DeleteOnClose);
    applyCommonInputOptions(diag, *pl.plugin(), init);
    diag->setInputMode(QInputDialog::TextInput);
    if (auto v = init.get<std::optional<QString>>("init_value"))
    {
        diag->setTextValue(*v);
    }
    if (init.get_or("is_password", false))
    {
        diag->setTextEchoMode(QLineEdit::Password);
    }
    if (init.get_or("is_multiline", false))
    {
        diag->setOptions(QInputDialog::UsePlainTextEditForTextInput);
    }

    QObject::connect(
        diag, &QInputDialog::finished,
        [diag, guard = *std::move(guard),
         cb = SignalCallback(pl.plugin()->weakRef(), cb)](int ret) {
            cb(makeConditionedOptional(ret != 0, diag->textValue()));
        });
    diag->show();
}

void showItemInput(ThisPluginState pl, const sol::table &init,
                   const sol::main_protected_function &cb)
{
    auto guard = pl.plugin()->openDialog();
    if (!guard)
    {
        throw std::runtime_error("Too many dialogs.");
    }

    auto *diag = new QInputDialog;
    diag->setAttribute(Qt::WA_DeleteOnClose);
    applyCommonInputOptions(diag, *pl.plugin(), init);
    diag->setInputMode(QInputDialog::TextInput);
    auto items = requiredGet<QStringList>(init, "items");
    diag->setComboBoxItems(items);
    // 1-based index.
    diag->setTextValue(items.value(init.get_or("init_index", 1) - 1));
    diag->setComboBoxEditable(init.get_or("editable", true));

    QObject::connect(
        diag, &QInputDialog::finished,
        [diag, guard = *std::move(guard),
         cb = SignalCallback(pl.plugin()->weakRef(), cb)](int ret) {
            cb(makeConditionedOptional(ret != 0, diag->textValue()));
        });
    diag->show();
}

}  // namespace

namespace chatterino::lua::api {

sol::object loadUi(sol::state_view lua)
{
    sol::table ui = lua.create_table();

    sol::table dialogs = ui.create();
    ui["dialogs"] = dialogs;
    dialogs["MessageBoxButton"] =
        createEnumTable<QMessageBox::StandardButton, QMessageBox::NoButton>(
            lua);
    dialogs["MessageBoxIcon"] = createEnumTable<QMessageBox::Icon>(lua);
    dialogs.set_function("message_box", &showMessageBox);
    dialogs.set_function("number_input", &showNumberInput);
    dialogs.set_function("text_input", &showTextInput);
    dialogs.set_function("item_input", &showItemInput);

    return ui;
}

}  // namespace chatterino::lua::api

#endif
