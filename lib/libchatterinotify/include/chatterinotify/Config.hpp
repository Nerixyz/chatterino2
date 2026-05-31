#pragma once

#if __has_cpp_attribute(gsl::Pointer)
#    define CHATTERINOTIFY_GSL_POINTER [[gsl::Pointer]]
#else
#    define CHATTERINOTIFY_GSL_POINTER
#endif
