#include <QStringView>
#include <utility>

namespace triegen {

// clang-format off
// NOLINTBEGIN
std::pair<int, qsizetype> tstUnicodePartialMatch(QStringView str) noexcept {
	const char16_t *in = str.utf16();
	qsizetype len = str.size();

	if (len >= 1 && *in == u'\x26f9') {
		len -= 1;
		in += 1;
		if (len >= 1) {
			len--;
			switch (*in++) {
				case u'\x200d': {
					if (len >= 1) {
						len--;
						switch (*in++) {
							case u'\x2640': {
								return {6, 3};
							}
							case u'\x2642': {
								return {4, 3};
							}
						}
					}
					break;
				}
				case u'\xfe0f': {
					if (len >= 1 && *in == u'\x200d') {
						len -= 1;
						in += 1;
						if (len >= 1) {
							len--;
							switch (*in++) {
								case u'\x2640': {
									if (len >= 1 && *in == u'\xfe0f') {
										len -= 1;
										in += 1;
										return {5, 5};
									}
									break;
								}
								case u'\x2642': {
									if (len >= 1 && *in == u'\xfe0f') {
										len -= 1;
										in += 1;
										return {3, 5};
									}
									break;
								}
							}
						}
					}
					return {1, 2};
				}
			}
		}
		return {2, 1};
	}
	return {-1, 0};
}
// NOLINTEND
// clang-format on

}  // namespace triegen
