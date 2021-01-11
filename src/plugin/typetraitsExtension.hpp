#ifdef ESP8266
#ifndef _PAINLESS_MESH_TYPETRAITSEXTENSION_HPP_
#define _PAINLESS_MESH_TYPETRAITSEXTENSION_HPP_

#include <type_traits>

namespace std {
namespace detail {
template <class T>
union trivial_helper {
  T t;
};

}  // namespace detail

// An implementation of `std::is_trivially_copyable` since STL version
// is not equally supported by all compilers, especially GCC 4.9.
// Uniform implementation of this trait is important for ABI compatibility
// as it has an impact on SmallVector's ABI (among others).
template <typename T>
class is_trivially_copyable {
  // copy constructors
  static constexpr bool has_trivial_copy_constructor =
      std::is_copy_constructible<detail::trivial_helper<T>>::value;
  static constexpr bool has_deleted_copy_constructor =
      !std::is_copy_constructible<T>::value;

  // move constructors
  static constexpr bool has_trivial_move_constructor =
      std::is_move_constructible<detail::trivial_helper<T>>::value;
  static constexpr bool has_deleted_move_constructor =
      !std::is_move_constructible<T>::value;

  // copy assign
  static constexpr bool has_trivial_copy_assign =
      is_copy_assignable<detail::trivial_helper<T>>::value;
  static constexpr bool has_deleted_copy_assign = !is_copy_assignable<T>::value;

  // move assign
  static constexpr bool has_trivial_move_assign =
      is_move_assignable<detail::trivial_helper<T>>::value;
  static constexpr bool has_deleted_move_assign = !is_move_assignable<T>::value;

  // destructor
  static constexpr bool has_trivial_destructor =
      std::is_destructible<detail::trivial_helper<T>>::value;

 public:
  static constexpr bool value =
      has_trivial_destructor &&
      (has_deleted_move_assign || has_trivial_move_assign) &&
      (has_deleted_move_constructor || has_trivial_move_constructor) &&
      (has_deleted_copy_assign || has_trivial_copy_assign) &&
      (has_deleted_copy_constructor || has_trivial_copy_constructor);

#ifdef HAVE_STD_IS_TRIVIALLY_COPYABLE
  static_assert(value == std::is_trivially_copyable<T>::value,
                "inconsistent behavior between llvm:: and std:: implementation "
                "of is_trivially_copyable");
#endif
};
template <typename T>
class is_trivially_copyable<T*> : public std::true_type {};

}  // namespace std

#endif
#endif