#include "controllers/plugins/api/Ui.hpp"

#include "controllers/plugins/api/ChannelRef.hpp"
#include "controllers/plugins/LuaUtilities.hpp"
#include "controllers/plugins/SolTypes.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

namespace {

using namespace chatterino;

void createWidgetTypes(sol::table &c2)
{
    c2.new_usertype<BaseWidget>("BaseWidget", sol::no_constructor,        //
                                sol::base_classes, sol::bases<QWidget>()  //
    );

    c2.new_usertype<BaseWindow>("BaseWindow", sol::no_constructor,
                                sol::base_classes,
                                sol::bases<BaseWidget, QWidget>());
}

sol::table qPointerWrapped(const auto &items, sol::this_state state)
{
    auto tbl =
        sol::state_view(state).create_table(static_cast<int>(items.size()), 0);

    for (size_t idx = 0; idx < items.size(); idx++)
    {
        tbl[static_cast<int>(idx + 1)] = QPointer(items[idx]);
    }
    return tbl;
}

}  // namespace

namespace chatterino::lua::api::ui {

using SplitContainerNodePtr = QGuardedPtr<SplitContainer::Node, SplitContainer>;

void createQtTypes(sol::state_view &lua)
{
    lua.new_usertype<QWidget>("QWidget", sol::no_constructor,  //
                              "close", &QWidget::close         //
    );
}

void createUserTypes(sol::table &c2)
{
    createWidgetTypes(c2);

    c2.new_usertype<Split>("Split", sol::no_constructor,  //
                           "channel",
                           sol::readonly_property([](const Split &self) {
                               return ChannelRef(self.getChannel());
                           })  //
    );

    sol::state_view state(c2.lua_state());
    c2["SplitContainerNodeType"] =
        createEnumTable<SplitContainer::Node::Type>(state);
    c2["WindowType"] = createEnumTable<WindowType>(state);

    c2.new_usertype<SplitContainer::Node>(
        "SplitContainerNode", sol::no_constructor,  //
        "type", sol::readonly_property([](const SplitContainer::Node &self) {
            return self.getType();
        }),
        "split", sol::readonly_property([](const SplitContainer::Node &self) {
            return QPointer(self.getSplit());
        }),
        "get_children",
        [](SplitContainerNodePtr &self, sol::this_state state) {
            const auto &nodes = self.target->getChildren();
            auto tbl = sol::state_view(state).create_table(
                static_cast<int>(nodes.size()), 0);

            for (size_t idx = 0; idx < nodes.size(); idx++)
            {
                tbl[static_cast<int>(idx + 1)] =
                    SplitContainerNodePtr{self.guard, nodes[idx].get()};
            }
            return tbl;
        }  //
    );

    c2.new_usertype<SplitContainer>(
        "SplitContainer", sol::no_constructor,  //
        "selected_split",
        sol::property(
            [](const SplitContainer &self) {
                return QPointer(self.getSelectedSplit());
            },
            [](SplitContainer &self, Split &split) {
                self.setSelected(std::addressof(split));
            }),
        "base_node", sol::readonly_property([](SplitContainer &self) {
            return SplitContainerNodePtr{QPointer(std::addressof(self)),
                                         self.getBaseNode()};
        }),                               //
        "popup", &SplitContainer::popup,  //
        "get_splits", [](SplitContainer &self, sol::this_state state) {
            return qPointerWrapped(self.getSplits(), state);
        });

    c2.new_usertype<Notebook>(
        "Notebook", sol::no_constructor,                                //
        "page_count", sol::readonly_property(&Notebook::getPageCount),  //
        "get_page_at",
        [](const Notebook &self, int index) {
            // this assumes we're a split notebook
            return QPointer(
                dynamic_cast<SplitContainer *>(self.getPageAt(index)));
        }  //
    );

    c2.new_usertype<SplitNotebook>(
        "SplitNotebook", sol::no_constructor,  //
        "selected_page", sol::readonly_property([](SplitNotebook &self) {
            return QPointer(self.getSelectedPage());
        }),                                        //
        sol::base_classes, sol::bases<Notebook>()  //
    );

    c2.new_usertype<Window>(
        "Window", sol::no_constructor,  //
        "notebook", sol::readonly_property([](Window &self) {
            return QPointer(&self.getNotebook());
        }),                                                               //
        "type", sol::readonly_property(&Window::getType),                 //
        sol::base_classes, sol::bases<BaseWindow, BaseWidget, QWidget>()  //
    );

    c2.new_usertype<WindowManager>(
        "WindowManager", sol::no_constructor,  //
        "main_window", sol::readonly_property([](WindowManager &self) {
            return QPointer(&self.getMainWindow());
        }),
        "last_selected_window",
        sol::readonly_property([](const WindowManager &self) {
            return QPointer(self.getLastSelectedWindow());
        }),
        "open_in_popup",
        [](WindowManager *self, ChannelRef channel) {
            return QPointer(&self->openInPopup(channel.strong()));
        },
        "select",
        sol::overload(
            [](WindowManager &self, Split &split) {
                self.select(std::addressof(split));
            },
            [](WindowManager &self, SplitContainer &container) {
                self.select(std::addressof(container));
            }),
        "get_windows",
        [](const WindowManager &self, sol::this_state state) {
            return qPointerWrapped(self.windows(), state);
        }  //
    );
}

}  // namespace chatterino::lua::api::ui
