#include <sol/forward.hpp>

/**
 * @includefile widgets/splits/SplitContainer.hpp
 * @includefile widgets/Window.hpp
 */

/* @lua-fragment

---@class c2.Split
---@field channel c2.Channel The channel open in this split (might be empty)
c2.Split = {}

---@class c2.SplitContainerNode A node in a split container
---@field type c2.SplitContainerNodeType The type of this node
---@field split c2.Split|nil The split contained in this code (if this is a split node)
c2.SplitContainerNode = {}

---Gets all children of this node.
---@return c2.SplitContainerNode[] children
function c2.SplitContainerNode:get_children() end

---@class c2.SplitContainer A container with potentially multiple splits
---@field selected_split c2.Split The currently selected split.
---@field base_node c2.SplitContainerNode The top level node.
c2.SplitContainer = {}

---Create a new popup window with this split container set as the only tab
function c2.SplitContainer:popup() end

---Gets all splits contained in this container
---@return c2.Split[] splits
function c2.SplitContainer:get_splits() end

---@class c2.Notebook
---@field page_count integer The number of pages/tabs.
c2.Notebook = {}

---@param i integer The zero based index of the page.
---@return c2.SplitContainer|nil page The page contained at the specified index (zero based).
function c2.Notebook:get_page_at(i) end

---@class c2.SplitNotebook : c2.Notebook
---@field selected_page c2.SplitContainer|nil The currently selected page.
c2.SplitNotebook = {}

---@class QWidget
QWidget = {}

---Closes the window if this widget is a window.
---@return boolean was_closed
function QWidget:close() end

---@class c2.BaseWidget : QWidget
c2.BaseWidget = {}

---@class c2.BaseWindow : c2.BaseWidget
c2.BaseWindow = {}

---@class c2.Window : c2.BaseWindow
---@field notebook c2.SplitNotebook The notebook of this window.
---@field type c2.WindowType The type of this window.
c2.Window = {}

---@class c2.WindowManager
---@field main_window c2.Window The main window.
---@field last_selected_window c2.Window The last selected window (or the main window if none were selected last).
c2.WindowManager = {}

---Open a channel in a new popup
---@param channel c2.Channel The channel to open.
---@return c2.Window window The opened window.
function c2.WindowManager:open_in_popup(channel) end

---Select a split or container
---@param thing c2.Split|c2.SplitContainer The split or container to select.
function c2.WindowManager:select(thing) end

---Gets all open windows.
---@return c2.Window[] windows
function c2.WindowManager:get_windows() end

---@class c2
---@field window_manager c2.WindowManager
c2 = {}
*/

namespace chatterino::lua::api::ui {

void createQtTypes(sol::state_view &lua);
void createUserTypes(sol::table &c2);

}  // namespace chatterino::lua::api::ui
