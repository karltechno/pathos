set(INPUT_SOURCES
    "Input.h"
    "Input.cpp"
    "InputTypes.h"
    "InputTypes.cpp"
)

if(WIN32)
    list(APPEND INPUT_SOURCES "Win/Input_Win.cpp" "Win/Input_Win.h")
endif()

add_pathos_lib(input "${INPUT_SOURCES}")