#include <QStringView>

namespace triegen {

// clang-format off
// NOLINTBEGIN
int tstBasicFullMatch(QStringView str) noexcept {
	const char16_t *in = str.utf16();
	qsizetype len = str.size();

	if (len >= 1) {
		len--;
		switch (*in++) {
			case u'b': {
				if (len >= 1 && *in == u'a') {
					len -= 1;
					in += 1;
					if (len == 0) { return 9; }
					if (len >= 1) {
						len--;
						switch (*in++) {
							case u'm': {
								if (len == 0) { return 5; }
								break;
							}
							case u'r': {
								if (len == 0) { return 8; }
								if (len >= 3 && std::memcmp((const char *)in, (const char *)u"foo", 6) == 0) {
									len -= 3;
									in += 3;
									if (len == 0) { return 2; }
								}
								break;
							}
							case u'z': {
								if (len == 0) { return 3; }
								if (len >= 3 && std::memcmp((const char *)in, (const char *)u"foo", 6) == 0) {
									len -= 3;
									in += 3;
									if (len == 0) { return 6; }
								}
								break;
							}
						}
					}
				}
				break;
			}
			case u'f': {
				if (len >= 2 && std::memcmp((const char *)in, (const char *)u"oo", 4) == 0) {
					len -= 2;
					in += 2;
					if (len == 0) { return 4; }
					if (len >= 3 && std::memcmp((const char *)in, (const char *)u"bar", 6) == 0) {
						len -= 3;
						in += 3;
						if (len == 0) { return 1; }
					}
				}
				break;
			}
			case u'q': {
				if (len >= 1) {
					len--;
					switch (*in++) {
						case u'o': {
							if (len == 0) { return 10; }
							break;
						}
						case u'u': {
							if (len == 0) { return 11; }
							break;
						}
					}
				}
				break;
			}
		}
	}
	return -1;
}
// NOLINTEND
// clang-format on

}  // namespace triegen
