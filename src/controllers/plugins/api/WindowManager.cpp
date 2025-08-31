#include "controllers/plugins/api/WindowManager.hpp"

#include "controllers/plugins/api/ChannelRef.hpp"
#include "controllers/plugins/LuaUtilities.hpp"
#include "controllers/plugins/SolTypes.hpp"  // IWYU pragma: keep
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/Window.hpp"

namespace {

using namespace chatterino;

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

struct SplitContainerNodeWrap {
    SplitContainerNodeWrap(std::weak_ptr<SplitContainer::Node> ptr)
        : ptr(std::move(ptr))
    {
    }

    sol::table children(sol::this_state state) const
    {
        const auto &nodes = this->strong()->getChildren();
        auto tbl = sol::state_view(state).create_table(
            static_cast<int>(nodes.size()), 0);

        for (size_t idx = 0; idx < nodes.size(); idx++)
        {
            tbl[static_cast<int>(idx + 1)] = SplitContainerNodeWrap{nodes[idx]};
        }
        return tbl;
    }

    std::shared_ptr<SplitContainer::Node> strong() const
    {
        auto locked = this->ptr.lock();
        if (locked)
        {
            return locked;
        }
        throw std::runtime_error("Split container node does not exist anymore");
    }

    bool operator==(const SplitContainerNodeWrap &other) const
    {
        return !this->ptr.owner_before(other.ptr) &&
               !other.ptr.owner_before(this->ptr);
    }

    static std::optional<SplitContainerNodeWrap> fromPtr(
        SplitContainer::Node *ptr)
    {
        if (ptr)
        {
            return SplitContainerNodeWrap(ptr->weak_from_this());
        }
        return std::nullopt;
    }

private:
    std::weak_ptr<SplitContainer::Node> ptr;
};

}  // namespace

namespace chatterino::lua::api::windowmanager {

void createUserTypes(sol::table &c2)
{
    c2.new_usertype<Split>("Split", sol::no_constructor,  //
                           "channel",
                           sol::readonly_property([](const Split &self) {
                               return ChannelRef(self.getChannel());
                           }));

    c2.new_usertype<SplitContainerNodeWrap>(
        "SplitContainerNode", sol::no_constructor,  //
        "type", sol::readonly_property([](const SplitContainerNodeWrap &self) {
            return self.strong()->getType();
        }),
        "split", sol::readonly_property([](const SplitContainerNodeWrap &self) {
            return QPointer(self.strong()->getSplit());
        }),
        "parent",
        sol::readonly_property([](const SplitContainerNodeWrap &self) {
            return SplitContainerNodeWrap::fromPtr(self.strong()->getParent());
        }),
        "horizontal_flex",
        sol::readonly_property([](const SplitContainerNodeWrap &self) {
            return self.strong()->getHorizontalFlex();
        }),
        "vertical_flex",
        sol::readonly_property([](const SplitContainerNodeWrap &self) {
            return self.strong()->getVerticalFlex();
        }),
        "get_children", &SplitContainerNodeWrap::children);

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
            return SplitContainerNodeWrap::fromPtr(self.getBaseNode());
        }),  //
        "get_splits", [](SplitContainer &self, sol::this_state state) {
            return qPointerWrapped(self.getSplits(), state);
        });

    c2.new_usertype<SplitNotebook>(
        "SplitNotebook", sol::no_constructor,  //
        "selected_page", sol::readonly_property([](SplitNotebook &self) {
            return QPointer(self.getSelectedPage());
        }),
        "page_count", sol::readonly_property(&SplitNotebook::getPageCount),
        "get_page_at", [](const SplitNotebook &self, int index) {
            return QPointer(
                dynamic_cast<SplitContainer *>(self.getPageAt(index)));
        });

    c2.new_usertype<Window>("Window", sol::no_constructor,  //
                            "notebook",
                            sol::readonly_property([](Window &self) {
                                return QPointer(&self.getNotebook());
                            }),
                            "type", sol::readonly_property([](Window &self) {
                                return self.getType();
                            }));

    c2.new_usertype<WindowManager>(
        "WindowManager", sol::no_constructor,  //
        "main_window", sol::readonly_property([](WindowManager &self) {
            return QPointer(&self.getMainWindow());
        }),
        "last_selected_window",
        sol::readonly_property([](const WindowManager &self) {
            return QPointer(self.getLastSelectedWindow());
        }),
        "get_windows", [](const WindowManager &self, sol::this_state state) {
            return qPointerWrapped(self.windows(), state);
        });
}

}  // namespace chatterino::lua::api::windowmanager
