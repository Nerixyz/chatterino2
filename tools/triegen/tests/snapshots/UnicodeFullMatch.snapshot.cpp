#include <QStringView>

namespace triegen {

// clang-format off
// NOLINTBEGIN
int tstUnicodeFullMatch(QStringView str) noexcept {
	const char16_t *in = str.utf16();
	qsizetype len = str.size();

	if (len >= 1 && *in == u'\x26f9') {
		len -= 1;
		in += 1;
		if (len == 0) { return 2; }
		if (len >= 1) {
			len--;
			switch (*in++) {
				case u'\x200d': {
					if (len >= 1) {
						len--;
						switch (*in++) {
							case u'\x2640': {
								if (len == 0) { return 6; }
								break;
							}
							case u'\x2642': {
								if (len == 0) { return 4; }
								break;
							}
						}
					}
					break;
				}
				case u'\xfe0f': {
					if (len == 0) { return 1; }
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
										if (len == 0) { return 5; }
									}
									break;
								}
								case u'\x2642': {
									if (len >= 1 && *in == u'\xfe0f') {
										len -= 1;
										in += 1;
										if (len == 0) { return 3; }
									}
									break;
								}
							}
						}
					}
					break;
				}
			}
		}
	}
	return -1;
}
// NOLINTEND
// clang-format on

}  // namespace triegen
