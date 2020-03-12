###############################################################################
#  Copyright (c) 2016-2020 Joel de Guzman
#
#  Distributed under the MIT License (https://opensource.org/licenses/MIT)
###############################################################################

project(json_test)

###############################################################################
# The executable App

add_executable(canvas_test main.cpp)

target_link_libraries(canvas_test infra canvas)

