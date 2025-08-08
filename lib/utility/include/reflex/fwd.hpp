#pragma once

/** @brief Short hand to `std::forward` */
#define reflex_fwd(__obj__) std::forward<decltype(__obj__)>(__obj__)
