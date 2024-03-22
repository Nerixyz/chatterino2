#pragma once

#include <triegen/panic.hpp>
#include <triegen/tree.hpp>

#include <algorithm>
#include <cassert>
#include <iomanip>

namespace triegen::detail {

template <typename T>
size_t commonPrefixLength(std::basic_string_view<T> left,
                          std::basic_string_view<T> right)
{
    size_t i = 0;
    for (; i < std::min(left.length(), right.length()); i++)
    {
        if (left[i] != right[i])
        {
            return i;
        }
    }
    return i;
}

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

inline void encodeChars(std::ostream &os, std::u16string_view view)
{
    for (auto c : view)
    {
        if (c >= 0x20 && c <= 0x7e && c != u'\\')
        {
            os << static_cast<char>(c);
            continue;
        }
        os << "\\x" << std::hex << std::setw(4) << std::setfill('0')
           << (uint16_t)c << std::setw(0) << std::dec;
    }
}

template <typename T>
bool pointerEq(const std::unique_ptr<T> &a, const std::unique_ptr<T> &b)
{
    if (!a || !b)
    {
        return a == b;
    }
    return *a == *b;
}

}  // namespace triegen::detail

namespace triegen {

template <Matchable T>
Node<T>::Node(std::u16string_view root, T value, size_t totalLength)
    : data(Run<T>{
          .run = root,
          .next{new Node<T>(End<T>{
              .value = value,
              .totalLength = totalLength,
          })},
      })
{
}

template <Matchable T>
Node<T>::Node(std::variant<End<T>, Run<T>, Cases<T>> &&data)
    : data(std::move(data))
{
}

template <Matchable T>
void Node<T>::add(std::u16string_view view, T value, size_t totalLength)
{
    auto make = [](auto &&data) {
        return std::unique_ptr<Node<T>>(
            new Node<T>(std::forward<decltype(data)>(data)));
    };

    auto vis = detail::Overloaded{
        [&](End<T> &node) {
            if (view.empty())
            {
                if (node.value != value)
                {
                    panic([&](auto &os) {
                        os << "Got identical keys with different values - "
                              "prev: "
                           << node.value << " to be inserted: " << value;
                    });
                }
                return;
            }

            if (node.next)
            {
                node.next->add(view, std::forward<T>(value), totalLength);
                return;
            }

            node.next = make(Run{
                .run = view,
                .next = make(End{
                    .value = value,
                    .totalLength = totalLength,
                }),
            });
        },
        [&](Run<T> &node) {
            if (view.empty())
            {
                auto self = make(std::move(node));
                this->data = End{
                    .value = value,
                    .next = std::move(self),
                    .totalLength = totalLength,
                };
                return;
            }

            // length of common prefix
            auto prefix = detail::commonPrefixLength(node.run, view);
            if (prefix == node.run.length())  // |view| >= |node.run|
            {
                if (!node.next)
                {
                    panic("Bad run without next node");
                }

                node.next->add(view.substr(prefix), std::forward<T>(value),
                               totalLength);
                return;
            }
            if (prefix == view.length())  // |view| < |node.run|
            {
                node.next = make(End{
                    .value = value,
                    .next = make(Run{
                        .run = node.run.substr(prefix),
                        .next = std::move(node.next),
                    }),
                    .totalLength = totalLength,
                });
                node.run = node.run.substr(0, prefix);
                return;
            }
            if (!(prefix < node.run.length() && prefix < view.length()))
            {
                panic("Invalid assumptions");
            }

            // both differ at `prefix`
            auto myChar = node.run.at(prefix);
            auto self = [&] {
                // `run` without the prefix is a single char
                if (node.run.length() - prefix == 1)
                {
                    return std::move(node.next);
                }
                return make(Run{
                    .run = node.run.substr(prefix + 1),
                    .next = std::move(node.next),
                });
            }();
            auto added = [&] {
                // `view` without the prefix is a single char
                if (view.length() - prefix == 1)
                {
                    return make(
                        End{.value = value, .totalLength = totalLength});
                }
                return make(Run{
                    .run = view.substr(prefix + 1),
                    .next =
                        make(End{.value = value, .totalLength = totalLength}),
                });
            }();

            Cases<T> newNode;
            newNode.cases.emplace(myChar, std::move(self));
            newNode.cases.emplace(view.at(prefix), std::move(added));
            if (prefix == 0)
            {
                this->data = std::move(newNode);
                return;
            }

            node.next = make(std::move(newNode));
            node.run = view.substr(0, prefix);
        },
        [&](Cases<T> &node) {
            if (view.empty())
            {
                auto self = make(std::move(node));
                this->data = End{
                    .value = value,
                    .next = std::move(self),
                    .totalLength = totalLength,
                };
                return;
            }

            auto current = node.cases.find(view.at(0));
            if (current != node.cases.end())
            {
                current->second->add(view.substr(1), std::forward<T>(value),
                                     totalLength);
                return;
            }

            auto entry = [&] {
                if (view.size() == 1)
                {
                    return make(
                        End{.value = value, .totalLength = totalLength});
                }
                return make(Run{
                    .run = view.substr(1),
                    .next =
                        make(End{.value = value, .totalLength = totalLength}),
                });
            }();
            node.cases.emplace(view.at(0), std::move(entry));
        },
    };
    std::visit(vis, this->data);
}

template <Matchable T>
void Node<T>::encode(EncodeCtx &ctx) const
{
    auto vis = detail::Overloaded{
        [&](const End<T> &node) {
            assert(node.totalLength != 0);

            if (ctx.fullMatch)
            {
                ctx.withIndent()
                    << "if (len == 0) { return " << node.value << "; }\n";
            }
            if (node.next)
            {
                node.next->encode(ctx);
            }

            if (!ctx.fullMatch)
            {
                ctx.withIndent() << "return {" << node.value << ", "
                                 << node.totalLength << "};\n";
            }
        },
        [&](const Run<T> &node) {
            ctx.withIndent() << "if (len >= " << node.run.length() << " && ";
            if (node.run.length() == 1)
            {
                ctx.of << "*in == u'";
                detail::encodeChars(ctx.of, node.run);
                ctx.of << '\'';
            }
            else
            {
                ctx.of << "std::memcmp((const char *)in, (const char *)u\"";
                detail::encodeChars(ctx.of, node.run);
                ctx.of << "\", " << node.run.length() * 2 << ") == 0";
            }
            ctx.of << ") {\n";
            ctx.indentation++;
            ctx.withIndent() << "len -= " << node.run.length() << ";\n";
            ctx.withIndent() << "in += " << node.run.length() << ";\n";
            node.next->encode(ctx);  // always has a next node
            ctx.indentation--;
            ctx.withIndent() << "}\n";
        },
        [&](const Cases<T> &node) {
            ctx.withIndent() << "if (len >= 1) {\n";
            ctx.indentation++;
            ctx.withIndent() << "len--;\n";
            ctx.withIndent() << "switch (*in++) {\n";
            ctx.indentation++;
            for (const auto &[c, next] : node.cases)
            {
                ctx.withIndent() << "case u'";
                detail::encodeChars(ctx.of, {&c, 1});
                ctx.of << "': {\n";
                ctx.indentation++;
                assert(next);
                next->encode(ctx);
                // don't print break if the next node is an end and there's
                // partial matching (would result in return ...; break;)
                if (!(next->isEnd() && !ctx.fullMatch))
                {
                    ctx.withIndent() << "break;\n";
                }
                ctx.indentation--;
                ctx.withIndent() << "}\n";
            }
            ctx.indentation--;
            ctx.withIndent() << "}\n";  // switch
            ctx.indentation--;
            ctx.withIndent() << "}\n";  // if
        },
    };

    auto prevIndent = ctx.indentation;
    std::visit(vis, this->data);
    ctx.indentation = prevIndent;
}

template <Matchable T>
bool Node<T>::isEnd() const
{
    return std::holds_alternative<End<T>>(this->data);
}

template <Matchable T>
bool Node<T>::isRun() const
{
    return std::holds_alternative<Run<T>>(this->data);
}

template <Matchable T>
bool Node<T>::isCases() const
{
    return std::holds_alternative<Cases<T>>(this->data);
}

template <typename T>
std::unique_ptr<Node<T>> makeTree()
{
    return {};
}

template <Matchable T>
void appendValue(std::unique_ptr<Node<T>> &tree, std::u16string_view key,
                 T value)
{
    if (!tree)
    {
        tree =
            std::make_unique<Node<T>>(key, std::forward<T>(value), key.size());
        return;
    }
    tree->add(key, std::forward<T>(value), key.size());
}

template <Matchable T>
void printTree(std::ostream &out, const std::unique_ptr<Node<T>> &tree,
               const PrintOptions &options)
{
    out << std::boolalpha << "#include <QStringView>\n";
    if (!options.fullMatch)
    {
        out << "#include <utility>\n";  // for std::pair
    }
    out << triegen::value::additionalIncludes<T>();
    out << "\nnamespace triegen {\n\n";
    out << "// clang-format off\n// NOLINTBEGIN\n";
    if (options.fullMatch)
    {
        out << triegen::value::valueName<T>() << " ";
    }
    else
    {
        out << "std::pair<" << triegen::value::valueName<T>()
            << ", qsizetype> ";
    }
    out << options.fnName;
    out << "(QStringView str) noexcept {\n";
    out << "\tconst char16_t *in = str.utf16();\n";
    out << "\tqsizetype len = str.size();\n\n";

    EncodeCtx ctx{
        .of = out,
        .indentation = 1,
        .fullMatch = options.fullMatch,
    };
    if (tree)
    {
        tree->encode(ctx);
    }

    ctx.withIndent() << "return ";
    if (options.fullMatch)
    {
        out << triegen::value::invalidValue<T>();
    }
    else
    {
        out << "{" << triegen::value::invalidValue<T>() << ", 0}";
    }
    out << ";\n}\n";
    out << "// NOLINTEND\n// clang-format on\n\n";
    out << "}  // namespace triegen\n";
}

template <Matchable T>
void printHeader(std::ostream &out, const PrintOptions &options)
{
    out << std::boolalpha << "#include <QStringView>\n";
    if (!options.fullMatch)
    {
        out << "#include <utility>\n";  // for std::pair
    }
    out << triegen::value::additionalIncludes<T>();
    out << "\nnamespace triegen {\n\n";
    out << "// clang-format off\n// NOLINTBEGIN\n";
    if (options.fullMatch)
    {
        out << triegen::value::valueName<T>() << " ";
    }
    else
    {
        out << "std::pair<" << triegen::value::valueName<T>()
            << ", qsizetype> ";
    }
    out << options.fnName;
    out << "(QStringView str) noexcept;\n";
    out << "// NOLINTEND\n// clang-format on\n\n";
    out << "}  // namespace triegen\n";
}

inline std::ostream &EncodeCtx::withIndent()
{
    std::fill_n(std::ostream_iterator<char>(this->of), this->indentation, '\t');
    return this->of;
}

template <Matchable T>
bool Cases<T>::operator==(const Cases<T> &other) const
{
    if (this->cases.size() != other.cases.size())
    {
        return false;
    }
    using El = std::pair<char16_t, const std::unique_ptr<Node<T>> &>;
    return std::equal(this->cases.begin(), this->cases.end(),
                      other.cases.begin(), [](El a, El b) {
                          return a.first == b.first &&
                                 detail::pointerEq(a.second, b.second);
                      });
}

template <Matchable T>
bool Run<T>::operator==(const Run<T> &other) const
{
    return this->run == other.run && detail::pointerEq(this->next, other.next);
}

template <Matchable T>
bool End<T>::operator==(const End<T> &other) const
{
    return this->totalLength == other.totalLength &&
           this->value == other.value &&
           detail::pointerEq(this->next, other.next);
}

}  // namespace triegen
