#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <variant>

/// Customization point for values
namespace triegen::value {

/// An invalid value returned when nothing matched (e.g. -1)
template <typename T>
T invalidValue();

/// The name of the type (e.g. 'int')
template <typename T>
std::string_view valueName();

/// Any additional includes for the type (must end in a newline if specified)
template <typename T>
std::string_view additionalIncludes()
{
    return {};
}

// Specializations for int

template <>
inline int invalidValue()
{
    return -1;
}

template <>
inline std::string_view valueName<int>()
{
    return "int";
}

}  // namespace triegen::value

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

template <Matchable T>
struct Cases {
    std::map<char16_t, std::unique_ptr<Node<T>>> cases;

    bool operator==(const Cases &other) const;
};

template <Matchable T>
struct End {
    T value;
    std::unique_ptr<Node<T>> next;
    size_t totalLength;

    bool operator==(const End &other) const;
};

template <Matchable T>
struct Run {
    std::u16string_view run;
    std::unique_ptr<Node<T>> next;

    bool operator==(const Run &other) const;
};

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
