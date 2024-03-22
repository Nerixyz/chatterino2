#pragma once

#include <triegen/values.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <variant>

namespace triegen {

struct EncodeCtx {
    std::ostream &of;
    size_t indentation = 0;
    bool fullMatch = false;

    std::ostream &withIndent();
};

template <typename T>
concept Matchable = requires(T a) {
    { triegen::value::invalidValue<T>() } -> std::convertible_to<T>;
    std::cout << a;  // must be printable
};

template <Matchable T>
class Node;

/// @brief A decision point (switch/match) for a single character
///
/// @example
///
/// Given `foo`, `bar`, `qox`, the root node of the trie would be a `Cases`:
///
/// ```text
/// Cases {
///     'f' => Run("oo") {...},
///     'b' => Run("ar") {...},
///     'q' => Run("ox") {...},
/// }
/// ```
template <Matchable T>
struct Cases {
    std::map<char16_t, std::unique_ptr<Node<T>>> cases;

    bool operator==(const Cases &other) const;
};

/// @brief A final value node (with potential followers)
///
/// @example
///
/// Given `foo`(1), and `fooo`(2) the final trie would look like this:
///
/// ```text
/// Run("foo") {
///     End(value = 1, totalLength = 3) {
///         Run("o") {
///             End(value = 2, totalLength = 4) {}
///         }
///     }
/// }
/// ```
///
/// If `fullMatch` is enabled (i.e. the entire input must be consumed), then
/// `"foo"` would return `1`, `"fooo"` would return `2`, and `"foob"` would not
/// match.
/// If `fullMatch` is disabled (prefix matches are allowed), then `"foo"` would
/// return `1`, `"fooo"` would return `2`, and `"foob"` would return `1` (but
/// only the first three character would be consumed).
template <Matchable T>
struct End {
    T value;
    std::unique_ptr<Node<T>> next;
    size_t totalLength;

    bool operator==(const End &other) const;
};

/// @brief A contiguous sequence of one or more characters that must match
///
/// @example
///
/// Given `bar` (1), and `baz` (2), the final trie would look like this:
///
/// ```text
/// Run("ba") {
///     Cases {
///         'r' => End(value = 1, totalLength = 3) {},
///         'z' => End(value = 1, totalLength = 3) {},
///     }
/// }
/// ```
template <Matchable T>
struct Run {
    std::u16string_view run;
    std::unique_ptr<Node<T>> next;

    bool operator==(const Run &other) const;
};

/// A subtrie node (combination of Cases, End, and Run)
template <Matchable T>
class Node
{
public:
    using Data = std::variant<End<T>, Run<T>, Cases<T>>;

    explicit Node(Data &&data);
    Node(std::u16string_view root, T value, size_t totalLength);

    void encode(EncodeCtx &ctx) const;
    void add(std::u16string_view view, T value, size_t totalLength);

    bool isEnd() const;
    bool isRun() const;
    bool isCases() const;

    bool operator==(const Node<T> &other) const = default;

private:
    Data data;
};

/// Creates an empty tree
template <typename T>
std::unique_ptr<Node<T>> makeTree();

/// @brief Inserts @a key with @a value into @a tree.
///
/// @a key must be kept alive while the tree is alive.
template <Matchable T>
void appendValue(std::unique_ptr<Node<T>> &tree, std::u16string_view key,
                 T value);

struct PrintOptions {
    /// Name of the function to generate
    std::string_view fnName;

    /// @brief Match the full string or only at the start
    ///
    /// If set, only strings that match to the end are considered (return type: T).
    /// If unset, strings are matched at the start and the consumed characters
    /// are returned upon a match (return type: std::pair<T, size_t>).
    bool fullMatch = true;
};

/// Prints @a tree to @a out
template <Matchable T>
void printTree(std::ostream &out, const std::unique_ptr<Node<T>> &tree,
               const PrintOptions &options);

}  // namespace triegen

#include <triegen/detail/tree_impl.hpp>
