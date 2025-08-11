cmake_minimum_required(VERSION 3.31)
project(cdcl_solver)

set(CMAKE_CXX_STANDARD 20)

add_executable(cdcl_solver main.cpp
        Clause.cpp
        Clause.h
        Solver.cpp
        Solver.h
        Trail.cpp
        Trail.h
        CNFParser.cpp
        CNFParser.h
        Heuristic.cpp
        Heuristic.h
        Literal.cpp
        Literal.h
)
