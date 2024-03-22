#include <QStringView>
#include <utility>

namespace triegen {

// clang-format off
// NOLINTBEGIN
std::pair<int, qsizetype> tstBasicPartialMatch(QStringView str) noexcept {
	const char16_t *in = str.utf16();
	qsizetype len = str.size();

	if (len >= 1) {
		len--;
		switch (*in++) {
			case u'b': {
				if (len >= 1 && *in == u'a') {
					len -= 1;
					in += 1;
					if (len >= 1) {
						len--;
						switch (*in++) {
							case u'm': {
								return {5, 3};
							}
							case u'r': {
								if (len >= 3 && std::memcmp((const char *)in, (const char *)u"foo", 6) == 0) {
									len -= 3;
									in += 3;
									return {2, 6};
								}
								return {8, 3};
							}
							case u'z': {
								if (len >= 3 && std::memcmp((const char *)in, (const char *)u"foo", 6) == 0) {
									len -= 3;
									in += 3;
									return {6, 6};
								}
								return {3, 3};
							}
						}
					}
					return {9, 2};
				}
				break;
			}
			case u'f': {
				if (len >= 2 && std::memcmp((const char *)in, (const char *)u"oo", 4) == 0) {
					len -= 2;
					in += 2;
					if (len >= 3 && std::memcmp((const char *)in, (const char *)u"bar", 6) == 0) {
						len -= 3;
						in += 3;
						return {1, 6};
					}
					return {4, 3};
				}
				break;
			}
			case u'q': {
				if (len >= 1) {
					len--;
					switch (*in++) {
						case u'o': {
							return {10, 2};
						}
						case u'u': {
							return {11, 2};
						}
					}
				}
				break;
			}
		}
	}
	return {-1, 0};
}
// NOLINTEND
// clang-format on

}  // namespace triegen
