---@meta chatterino.ui

local ui = {}

ui.dialogs = {}

---Bit flags of message box buttons.
---@enum ui.dialogs.MessageBoxButton
ui.dialogs.MessageBoxButton = {
	NoButton = 0,
	Ok = 0,
	Save = 0,
	SaveAll = 0,
	Open = 0,
	Yes = 0,
	YesToAll = 0,
	No = 0,
	NoToAll = 0,
	Abort = 0,
	Retry = 0,
	Ignore = 0,
	Close = 0,
	Cancel = 0,
	Discard = 0,
	Help = 0,
	Apply = 0,
	Reset = 0,
	RestoreDefaults = 0,
}

---@enum ui.dialogs.MessageBoxIcon
ui.dialogs.MessageBoxIcon = {
	NoIcon = {}, ---@type ui.dialogs.MessageBoxIcon.NoIcon
	Question = {}, ---@type ui.dialogs.MessageBoxIcon.Question
	Information = {}, ---@type ui.dialogs.MessageBoxIcon.Information
	Warning = {}, ---@type ui.dialogs.MessageBoxIcon.Warning
	Critical = {}, ---@type ui.dialogs.MessageBoxIcon.Critical
}

---@class MessageBoxInit
---@field title? string Title of the dialog.
---@field text string The contents of the dialog.
---@field buttons? ui.dialogs.MessageBoxButton The buttons to show (bit flags, default: Ok).
---@field icon? ui.dialogs.MessageBoxIcon The icon on the message box (default: Information).
---@field text_format? 'markdown'|'qt-html'|'plain' The format of the dialog text (default: plain).
---@field clickable_links? boolean Hyperlinks in the dialog text will be opened when clicked (default: false).
---@field default_button? ui.dialogs.MessageBoxButton Button to select by default.
---@field escape_button? ui.dialogs.MessageBoxButton Activated button when the escape key is pressed.

---Show a message box with some content.
---@param init MessageBoxInit
---@param cb fun(button: ui.dialogs.MessageBoxButton) Callback to call after closing.
function ui.dialogs.message_box(init, cb) end

---@class CommonInputDialogInit
---@field title? string Title of the dialog.
---@field label string Label on what needs to be input.
---@field ok_button_text? string Text on the "OK" button.
---@field cancel_button_text? string Text on the "Cancel" button.

---@class NumberInputInit : CommonInputDialogInit
---@field decimals? number Number of decimals (default: 0).
---@field min? number Minimum value.
---@field max? number Maximum value.
---@field init_value? number Initial value.
---@field step? number Step by which the value is increased/decreased.

---Show a dialog requesting a number to be input.
---@param init NumberInputInit
---@param cb fun(value: number|nil) Callback to call with the value.
function ui.dialogs.number_input(init, cb) end

---@class TextInputInit : CommonInputDialogInit
---@field init_value? string Initial value.
---@field is_password? boolean True if this input is a password or similar (default: false, incompatible with is_multiline).
---@field is_multiline? boolean True if this input is a multiline input (default: false. incompatible with is_password).

---Show a dialog requesting text input.
---@param init TextInputInit
---@param cb fun(value: string|nil)
function ui.dialogs.text_input(init, cb) end

---@class ItemInputInit : CommonInputDialogInit
---@field items string[] List of possible items.
---@field editable? boolean Items are editable or not (default: true).
---@field init_index? number Initial item index (default: 1).

---Show a dialog requesting text input with some possible options (i.e. a combo box).
---@param init ItemInputInit
---@param cb fun(value: string|nil)
function ui.dialogs.item_input(init, cb) end

return ui
