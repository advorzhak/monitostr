function(monitostr_apply_compiler_settings target_name)
  target_compile_features(${target_name} PRIVATE cxx_std_20)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
    )
  elseif(MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  endif()
endfunction()
